#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <cstring>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "global.h"

using namespace std;

typedef struct phase2_param {
  key_t *thresholds;
} phase2_param_t;

int prepare_environment();

void phase1_v2(int input_fd, phase2_param_t &phase2_param);
void phase2(int input_fd, phase2_param_t &phase2_param);
void radix_sort(tuple_t *data, size_t sz, key_t *thresholds, size_t num_buckets);

void phase1(const char *filename);
void parallel_sort(tuple_t *data, size_t sz);
void output_tmp(const char *filename, size_t round, const char *buffer, size_t sz);

int main(int argc, char *argv[]) {
  ios_base::sync_with_stdio(false);

  if (argc < 3) {
    printf("Program usage: ./run input_file_name output_file_name");
    return 0;
  }

  prepare_environment();

  int input_fd;
  if ((input_fd = open(argv[1], O_RDONLY)) == -1) {
    printf("[Error] failed to open input file %s\n", argv[1]);
    return 0;
  }

  /// [Phase 1] START
  // Input file's file descriptor
  phase2_param_t phase2_param;
  phase1_v2(input_fd, phase2_param);
  /// [Phase 1] END

  /// [Phase 2] START
  phase2(input_fd, phase2_param);
  /// [Phase 2] END
//  for (size_t i = 0; i < num_partitions; i++) {
//    size_t head_offset = READ_BUFFER_SIZE * i;
//    size_t read_amount = i != num_partitions - 1 ?
//                         READ_BUFFER_SIZE : file_size % READ_BUFFER_SIZE; // The last part will have remainders
//
//    for (size_t offset = 0; offset < read_amount;) {
//      size_t ret = pread(input_fd, input_buffer + offset, READ_BUFFER_SIZE + head_offset - offset,
//                         head_offset + offset);
//      offset += ret;
//    }
//
//    tuple_t *data = (tuple_t *) input_buffer; // Need to use buffer as tuple type below, just a typecasting pointer
//
//    // For each tuple
//    for (size_t j = 0; j < read_amount / TUPLE_SIZE; j++) {
//      // Find the right place
//      for (size_t k = 0; k < num_partitions - 1; k++) {
//        key_t key = *(key_t *) (data + j);
//        if (key < threshold[k]) {
//        }
//      }
//    }
//  }
  /// [Phase 2] END

  // Read + sort + tmp-output
  //  phase1(argv[1]);

  // N-way merge
  // phase2(argv[2]);

  return 0;
}

int prepare_environment() {
  if (mkdir(TMP_DIRECTORY, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
    if (errno == EEXIST) {
      // alredy exists
    } else {
      printf("mkdir %s failed\n", TMP_DIRECTORY);
      return -1;
    }
  }
  return 0;
}

void phase1_v2(int input_fd, phase2_param_t &phase2_param) {
  size_t file_size = lseek(input_fd, 0, SEEK_END);                  // Input file size
  size_t num_tuples = file_size / TUPLE_SIZE;                       // Number of tuples
  size_t num_cycles = (file_size - 1) / PHASE1_BUFFER_SIZE + 1;   // Total cycles run to process the input file
  printf("File size: %zu, Tuples existing: %zu, Num partitions: %zu\n", file_size, num_tuples, num_cycles);

  // Buffer for reading N bytes from file at once (N = READ_BUFFER_SIZE)
  char *input_buffer;
  if ((input_buffer = (char *) malloc(sizeof(char) * PHASE1_BUFFER_SIZE)) == NULL) {
    printf("Buffer allocation failed (input read buffer)\n");
    return;
  }

  key_t *keys;
  if ((keys = (key_t *) malloc(sizeof(key_t) * num_tuples)) == NULL) {
    printf("Buffer allocation failed (key buffer)\n");
    return;
  }

  // For each cycle, we must only extract the keys
  size_t key_offset = 0;
  for (size_t i = 0; i < num_cycles; i++) {
    size_t head_offset = PHASE1_BUFFER_SIZE * i;
    size_t read_amount = i != num_cycles - 1 ? PHASE1_BUFFER_SIZE :
                         (file_size - 1) % PHASE1_BUFFER_SIZE + 1; // The last part will have remainders

    for (size_t offset = 0; offset < read_amount;) {
      size_t ret = pread(input_fd, input_buffer + offset, PHASE1_BUFFER_SIZE + head_offset - offset,
                         head_offset + offset);
      offset += ret;
    }

    printf("Read file offset %zu ~ %zu\n", head_offset, head_offset + read_amount);

    tuple_t *data = (tuple_t *) input_buffer; // Need to use buffer as tuple type below, just a typecasting pointer
    // Need to extract only the keys
    for (size_t j = 0; j < read_amount / TUPLE_SIZE; j++) {
      memcpy(keys + key_offset + j, data + j, sizeof(key_t));
    }
    key_offset += read_amount / TUPLE_SIZE;
  }
  free(input_buffer);
  printf("Key array generated: %zu\n", key_offset);

  // Sort the key list, ascending order
  sort(keys, keys + num_tuples);

  // Need $(num_partitions - 1) keys to separate the whole input into $num_partitions parts.
  // They will be used as following (ex. num_partitions = 4)
  // if (key < threshold[0]): put to file 0,
  // else if (key < threshold[1]): put to file 1,
  // else if (key < threshold[2]): put to file 2,
  // else: put to file 3
  phase2_param.thresholds = (key_t *) malloc(sizeof(key_t) * (NUM_PARTITIONS - 1));
  for (size_t i = 1; i < NUM_PARTITIONS; i++) {
    printf("Threshold %zu = %zu\n", i, i * num_tuples / NUM_PARTITIONS);
    memcpy(phase2_param.thresholds + i - 1, keys + i * num_tuples / NUM_PARTITIONS, sizeof(key_t));
  }
  free(keys);
}

void phase2(int input_fd, phase2_param_t &phase2_param) {
  size_t file_size = lseek(input_fd, 0, SEEK_END);                  // Input file size
  size_t num_tuples = file_size / TUPLE_SIZE;                       // Number of tuples
  size_t num_cycles = (file_size - 1) / PHASE2_BUFFER_SIZE + 1; // Total cycles run to process the input file

  // Buffer for reading N bytes from file at once (N = READ_BUFFER_SIZE)
  char *input_buffer;
  if ((input_buffer = (char *) malloc(sizeof(char) * PHASE2_BUFFER_SIZE)) == NULL) {
    printf("Buffer allocation failed (input read buffer)\n");
    return;
  }

  for (size_t i = 0; i < num_cycles; i++) {
    size_t head_offset = PHASE2_BUFFER_SIZE * i;
    size_t read_amount = i != num_cycles - 1 ? PHASE2_BUFFER_SIZE :
                         (file_size - 1) % PHASE2_BUFFER_SIZE + 1; // The last part will have remainders

    for (size_t offset = 0; offset < read_amount;) {
      size_t ret = pread(input_fd, input_buffer + offset, PHASE2_BUFFER_SIZE + head_offset - offset,
                         head_offset + offset);
      offset += ret;
    }

    printf("Read file offset %zu ~ %zu\n", head_offset, head_offset + read_amount);

    size_t cnt[NUM_PARTITIONS];
    memset(cnt, 0, NUM_PARTITIONS);

    tuple_t *data = (tuple_t *) input_buffer; // Need to use buffer as tuple type below, just a typecasting pointer
    radix_sort(data, read_amount / TUPLE_SIZE, phase2_param.thresholds, NUM_PARTITIONS);
  }

  free(input_buffer);
}

void radix_sort(tuple_t *data, size_t sz, key_t *thresholds, size_t num_buckets) {
  size_t cnt[num_buckets];
  memset(cnt, 0, NUM_PARTITIONS);

  for (size_t i = 0; i < num_buckets; i++) {
  }
}

// TODO: use parallel radix sort
void parallel_sort(tuple_t *data, size_t sz) {
  sort(data, data + sz);
//  sort(data, data + sz, [](const tuple_t &a, const tuple_t &b) {
//    return memcmp(&a, &b, KEY_SIZE) < 0;
//  });
}

void output_tmp(const char *filename, size_t round, const char *buffer, size_t sz) {
  int fd;
  if ((fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0777)) == -1) {
    printf("[Error] failed to open input file %s\n", filename);
    return;
  }

  for (size_t offset = 0; offset < sz;) {
    size_t ret = pwrite(fd, buffer, sz - offset, round * PHASE1_BUFFER_SIZE + offset);
    offset += ret;
  }

  printf("File output done to %s (%zu bytes)\n", filename, sz);
}

// Sort in memory
void phase1(const char *filename) {
  int fd;
  if ((fd = open(filename, O_RDONLY)) == -1) {
    printf("[Error] failed to open input file %s\n", filename);
    return;
  }
  size_t file_size = lseek(fd, 0, SEEK_END);
  printf("File size: %zu\n", file_size);

  char *buffer;
  if ((buffer = (char *) malloc(sizeof(char) * PHASE1_BUFFER_SIZE)) == NULL) {
    printf("Buffer allocation failed\n");
    return;
  }

  size_t num_partitions = (file_size - 1) / PHASE1_BUFFER_SIZE + 1;
  printf("Num partitions: %zu\n", num_partitions);

  for (size_t i = 0; i < num_partitions; i++) {
    size_t head_offset = PHASE1_BUFFER_SIZE * i;
    size_t read_amount = i != num_partitions - 1 ?
                         PHASE1_BUFFER_SIZE : file_size % PHASE1_BUFFER_SIZE; // The last part will have remainders

    for (size_t offset = 0; offset < read_amount;) {
      size_t ret = pread(fd, buffer + offset, PHASE1_BUFFER_SIZE + head_offset - offset, head_offset + offset);
      offset += ret;
    }

    printf("Read file offset %zu ~ %zu\n", head_offset, head_offset + read_amount);

    auto data = (tuple_t *) buffer;
    size_t num_data = PHASE1_BUFFER_SIZE / TUPLE_SIZE;
  }

  close(fd);
  free(buffer);
}