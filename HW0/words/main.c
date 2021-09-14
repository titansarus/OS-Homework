/*

  Word Count using dedicated lists

*/

/*
Copyright © 2019 University of California, Berkeley

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "word_count.h"

/* Global data structure tracking the words encountered */
WordCount *word_counts = NULL;

/* The maximum length of each word in a file */
#define MAX_WORD_LEN 64

/*
 * 3.1.1 Total Word Count
 *
 * Returns the total amount of words found in infile.
 * Useful functions: fgetc(), isalpha().
 */
int num_words(FILE *infile) {
  if (infile == NULL) {
    return -1;
  }
  int num_words = 0;
  int number_of_characters = 0;
  char ch = fgetc(infile);
  bool word_started = false;
  while (ch != EOF) {
    if (ch == ' ' || ch == '\n' || ch == '\t' || ch == '\0' || ch == '\r') {
      if (word_started) {
        num_words += (number_of_characters > 1 ? 1 : 0);
        word_started = false;
        number_of_characters = 0;
      }
    } else {
      word_started = true;
      number_of_characters++;
    }

    ch = fgetc(infile);
  }
  if (word_started) {
    num_words += (number_of_characters > 1 ? 1 : 0);
    word_started = false;
  }
  return num_words;
}

/*
 * Comparator to sort list by frequency.
 * Useful function: strcmp().
 */
static bool wordcount_less(const WordCount *wc1, const WordCount *wc2) {
  if (wc1 == NULL && wc2 == NULL) {
    return 0;
  } else if (wc1 == NULL) {
    return 1;
  } else if (wc2 == NULL) {
    return 0;
  } else {
    if (wc1->count != wc2->count) {
      return wc1->count < wc2->count;
    } else {
      return strcmp(wc1->word, wc2->word) < 0;
    }
  }
}

/*
 * 3.1.2 Word Frequency Count
 *
 * Given infile, extracts and adds each word in the FILE to `wclist`.
 * Useful functions: fgetc(), isalpha(), tolower(), add_word().
 */
void count_words(WordCount **wclist, FILE *infile) {
  const char *delimiters = " \t\r\n\0";
  char *buffer = NULL;
  size_t len = 0;
  if (infile == NULL) {
    return;
  }
  while (getline(&buffer, &len, infile) != -1) {
    char *token = strtok(buffer, delimiters);
    while (token != NULL) {
      if (strlen(token) > 1) {
        add_word(&word_counts, token);
      }
      token = strtok(NULL, delimiters);
    }
  }
}

// In trying times, displays a helpful message.
static int display_help(void) {
  printf(
      "Flags:\n"
      "--count (-c): Count the total amount of words in the file, or STDIN if "
      "a file is not specified. This is default behavior if no flag is "
      "specified.\n"
      "--frequency (-f): Count the frequency of each word in the file, or "
      "STDIN if a file is not specified.\n"
      "--help (-h): Displays this help message.\n");
  return 0;
}

// Utility Function
void frequency_and_total_word_count(WordCount **wclist, FILE *infile,
                                    int *total_words, bool count_mode) {
  if (count_mode) {
    (*total_words) += num_words(infile);
  } else {
    count_words(wclist, infile);
  }
}

/*
 * Handle command line flags and arguments.
 */
int main(int argc, char *argv[]) {
  // Count Mode (default): outputs the total amount of words counted
  bool count_mode = true;
  int total_words = 0;

  // Freq Mode: outputs the frequency of each word
  bool freq_mode = false;

  FILE *infile = NULL;

  // Variables for command line argument parsing
  int i;
  static struct option long_options[] = {{"count", no_argument, 0, 'c'},
                                         {"frequency", no_argument, 0, 'f'},
                                         {"help", no_argument, 0, 'h'},
                                         {0, 0, 0, 0}};

  // Sets flags
  while ((i = getopt_long(argc, argv, "cfh", long_options, NULL)) != -1) {
    switch (i) {
      case 'c':
        count_mode = true;
        freq_mode = false;
        break;
      case 'f':
        count_mode = false;
        freq_mode = true;
        break;
      case 'h':
        return display_help();
    }
  }

  if (!count_mode && !freq_mode) {
    printf("Please specify a mode.\n");
    return display_help();
  }

  /* Create the empty data structure */
  init_words(&word_counts);

  if ((argc - optind) < 1) {
    // No input file specified, instead, read from STDIN instead.
    infile = stdin;
    frequency_and_total_word_count(&word_counts, infile, &total_words,
                                   count_mode);
  } else {
    for (int i = optind; i <= argc - 1; i++) {
      // char *file_name = argv[i];

      infile = fopen(argv[i], "r");
      frequency_and_total_word_count(&word_counts, infile, &total_words,
                                     count_mode);
      fclose(infile);
    }
    // At least one file specified. Useful functions: fopen(), fclose().
    // The first file can be found at argv[optind]. The last file can be
    // found at argv[argc-1].
  }

  if (count_mode) {
    printf("The total number of words is: %i\n", total_words);
  } else {
    wordcount_sort(&word_counts, wordcount_less);

    printf("The frequencies of each word are: \n");
    fprint_words(word_counts, stdout);
  }
  return 0;
}
