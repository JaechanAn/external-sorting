//
// Created by 안재찬 on 25/09/2019.
//

#ifndef MULTICORE_EXTERNAL_SORT_GLOBAL_H
#define MULTICORE_EXTERNAL_SORT_GLOBAL_H

#include <cstring>

#define MAX_BUFFER (1800000000)
#define NUM_THREADS (40)

#define TUPLE_SIZE (100)
#define KEY_SIZE (10)
#define READ_BUFFER_SIZE (1000000000)  // 1GB
#define WRITE_BUFFER_SIZE (500000000)

#define NUM_BUCKETS (256)

#define TMP_DIRECTORY ("./tmp/")
#define TMP_FILE_SUFFIX (".data")

typedef struct tuple {
  char data[100];

  bool operator<(const struct tuple &op) const {
    return memcmp(data, &op, KEY_SIZE) < 0;
  }

  bool operator>(const struct tuple &op) const {
    return memcmp(data, &op, KEY_SIZE) > 0;
  }
} tuple_t;

typedef struct tuple_key {
  char key[10];

  bool operator<(const struct tuple_key &op) const {
    return memcmp(key, &op, KEY_SIZE) < 0;
  }

  bool operator>(const struct tuple_key &op) const {
    return memcmp(key, &op, KEY_SIZE) > 0;
  }

  bool operator==(const struct tuple_key &op) const {
    return memcmp(key, &op, KEY_SIZE) == 0;
  }

  bool operator!=(const struct tuple_key &op) const {
    return memcmp(key, &op, KEY_SIZE) != 0;
  }
} tuple_key_t;

typedef struct param {
  int input_fd;
  int output_fd;
  size_t file_size;
  size_t num_partitions;
  size_t num_tuples;
  tuple_key_t *thresholds;
  char *buffer;
  tuple_key_t *sorted_keys;
} param_t;

typedef struct section {
  size_t head;
  size_t tail;
} section_t;


#endif //MULTICORE_EXTERNAL_SORT_GLOBAL_H
