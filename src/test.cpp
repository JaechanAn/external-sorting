//
// Created by 안재찬 on 27/09/2019.
//

#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <chrono>

#include <unistd.h>
#include <fcntl.h>
#include <omp.h>

#include "global.h"
#include "paralleld_radix_sort.h"

using namespace std;

int main() {
  int input_fd = open("data/input_1gb.data", O_RDONLY);
  if (input_fd == -1) {
    return 1;
  }
  size_t num_tuples = 1000000;
  char *buffer = (char *) malloc(TUPLE_SIZE * num_tuples);
  size_t ret = pread(input_fd, buffer, TUPLE_SIZE * num_tuples, 0);
  printf("%zu tuples read\n", ret / TUPLE_SIZE);
  tuple_key_t *keys = (tuple_key_t *) malloc(KEY_SIZE * num_tuples);
  for (size_t i = 0; i < num_tuples; i++) {
    memcpy(keys + i, buffer + TUPLE_SIZE * i, KEY_SIZE);
  }

  chrono::time_point<chrono::system_clock> t1, t2;
  long long int duration;
  t1 = chrono::high_resolution_clock::now();
  parallel_radix_sort(keys, num_tuples, 0, NUM_THREADS);
  t2 = chrono::high_resolution_clock::now();

  duration = chrono::duration_cast<chrono::milliseconds>(t2 - t1).count();
  cout << "[Phase1] took: " << duration << "(milliseconds)" << endl;

  close(input_fd);
  free(buffer);
  free(keys);
  return 0;
}