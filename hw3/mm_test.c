
#include <stdio.h>

#include "mm_alloc.h"

int main(int argc, char **argv) {
  int *data, *data2, *d1, *d2, *d3, *d4;

  data = (int *)mm_malloc(100000 * sizeof(int));
  data2 = (int *)mm_malloc(100000 * sizeof(int));
  for (int i = 0; i < 100000; i++) {
    data[i] = i;
    data2[i] = i;
  }
  for (int i = 0; i < 100000; i++) {
    printf("%x: %d\n", &data[i], data[i]);
  }
  for (int i = 0; i < 100000; i++) {
    printf("%x: %d\n", &data2[i], data2[i]);
  }
  mm_free(data2);
  mm_realloc(data, 150000);
  mm_free(data);

  d1 = (int *)mm_malloc(1000);
  d2 = (int *)mm_malloc(1000);
  d3 = (int *)mm_malloc(1000);
  mm_free(d1);
  mm_free(d3);
  mm_free(d2);
  d4 = (int *)mm_malloc(3082);
  for (int i = 0; i < 3082; i++) {
    d4[i] = i;
  }

  printf("malloc sanity test successful!\n");
  return 0;
}