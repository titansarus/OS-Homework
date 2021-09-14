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

word_count provides lists of words and associated count

Functional methods take the head of a list as first arg.
Mutators take a reference to a list as first arg.
*/

#include "word_count.h"

/* Basic utililties */

char *new_string(char *str) {
  return strcpy((char *)malloc(strlen(str) + 1), str);
}

void init_words(WordCount **wclist) {
  /* Initialize word count.  */
  *wclist = (WordCount *)malloc(sizeof(WordCount));
  (*wclist)->count = 0;
  (*wclist)->word = NULL;
  (*wclist)->next = NULL;
}

size_t len_words(WordCount *wchead) {
  size_t len = 0;
  while (wchead != NULL) {
    len++;
    wchead = wchead->next;
  }

  return len;
}

WordCount *find_word(WordCount *wchead, char *word) {
  /* Return count for word, if it exists */
  WordCount *wc = wchead;
  while (wc != NULL) {
    if (wc->word != NULL && strcmp(wc->word, word) == 0) {
      return wc;
    }
    wc = wc->next;
  }
  return wc;
}

void add_word(WordCount **wclist, char *word) {
  char *word_temp = new_string(word);
  for (int i = 0; word_temp[i]; i++) {  // Lowercase the word
    word_temp[i] = tolower(word_temp[i]);
  }
  WordCount *wc = *wclist;
  if (wc->word == NULL) {
    wc->word = word_temp;
    wc->count = 1;
    wc->next = NULL;
    return;
  }
  while (wc->next != NULL) {
    if (wc->word != NULL && strcmp(wc->word, word_temp) == 0) {
      wc->count++;
      return;
    }
    wc = wc->next;
  }
  if (wc->word != NULL && strcmp(wc->word, word_temp) == 0) {
    wc->count++;
    return;
  }
  WordCount *new_word = (WordCount *)malloc(sizeof(WordCount));
  new_word->count = 1;
  new_word->next = NULL;
  new_word->word = word_temp;
  wc->next = new_word;

  /* If word is present in word_counts list, increment the count, otw insert
   * with count 1. */
}

void fprint_words(WordCount *wchead, FILE *ofile) {
  /* print word counts to a file */
  WordCount *wc;
  for (wc = wchead; wc; wc = wc->next) {
    if (wc->count > 0) {
      fprintf(ofile, "%i\t%s\n", wc->count, wc->word);
    }
  }
}
