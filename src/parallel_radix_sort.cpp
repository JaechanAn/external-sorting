//
// Created by 안재찬 on 27/09/2019.
//

#include "parallel_radix_sort.h"

#include <cstdio>
#include <utility>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <omp.h>

namespace radix_sort {

  template void
  parallel_radix_sort<tuple_key_t>(tuple_key_t *, size_t, size_t, size_t); // instantiates f<double>(double)
  template void parallel_radix_sort<tuple_t>(tuple_t *, size_t, size_t, size_t); // instantiates f<double>(double)

  template<class T>
  void parallel_radix_sort(T *data, size_t sz, size_t level, size_t num_processors) {
    if (sz < 64) {
      std::sort(data, data + sz);
      return;
    }
    if (num_processors == 0) {
      num_processors = 1;
    }

    size_t buckets[NUM_BUCKETS];
    memset(buckets, 0, sizeof(size_t) * NUM_BUCKETS);

    // Build histogram
    #pragma omp parallel shared(sz, data, num_processors, level, buckets) default(none)
    {
      #pragma omp for
      for (size_t i = 0; i < num_processors; i++) {
        size_t head_offset = i * sz / num_processors;
        size_t chunk_size = i == num_processors - 1 ? sz / num_processors + sz % num_processors : sz / num_processors;
        for (size_t offset = head_offset; offset < head_offset + chunk_size; offset++) {
          size_t b = bucket(&data[offset], level);
          #pragma omp atomic
          buckets[b]++;
        }
      }
    }

    size_t sum = 0;
    section_t g[NUM_BUCKETS];
    section_t p[NUM_BUCKETS][num_processors];

    // Set bucket [head, tail]
    // Partition for repair
    for (size_t i = 0; i < NUM_BUCKETS; i++) {
      g[i].head = sum;
      sum += buckets[i];
      g[i].tail = sum;
    }

    bool is_empty = false;
    while (!is_empty) {
      // Partition For Permutation
      for (size_t bucket_id = 0; bucket_id < NUM_BUCKETS; bucket_id++) {
        size_t total = g[bucket_id].tail - g[bucket_id].head;

        if (total == 0) {
          for (size_t thread_id = 0; thread_id < num_processors; thread_id++) {
            p[bucket_id][thread_id].head = p[bucket_id][thread_id].tail = g[bucket_id].tail;
          }
          continue;
        }

        size_t needed_threads = num_processors;
        size_t chunk_size = total / num_processors;

        if (total < num_processors) {
          needed_threads = 1;
          chunk_size = 1;
        }

        for (size_t thread_id = 0; thread_id < needed_threads; thread_id++) {
          p[bucket_id][thread_id].head = g[bucket_id].head + chunk_size * thread_id;
          p[bucket_id][thread_id].tail = p[bucket_id][thread_id].head + chunk_size;
        }
        p[bucket_id][needed_threads - 1].tail = g[bucket_id].tail;

        for (size_t thread_id = needed_threads; thread_id < num_processors; thread_id++) {
          p[bucket_id][thread_id].head = p[bucket_id][thread_id].tail = g[bucket_id].tail;
        }
      }

      // Permutation stage
      #pragma omp parallel shared(data, level, num_processors, p) default(none)
      {
        #pragma omp for
        for (size_t thread_id = 0; thread_id < num_processors; thread_id++) {
          permute(data, level, (section_t *) p, num_processors, thread_id);
        }
      }

      // Repair stage
      is_empty = true;
      #pragma omp parallel shared(data, level, num_processors, g, p, is_empty) default(none)
      {
        #pragma omp for
        for (size_t bucket_id = 0; bucket_id < NUM_BUCKETS; bucket_id++) {
          repair(data, level, g, (section_t *) p, num_processors, bucket_id);
          if (g[bucket_id].tail - g[bucket_id].head > 0) {
            is_empty = false;
          }
        }
      }
    }

    if (level < KEY_SIZE) {
      size_t offsets[NUM_BUCKETS];
      offsets[0] = 0;
      for (size_t i = 1; i < NUM_BUCKETS; i++) {
        offsets[i] = g[i - 1].tail;
      }

      #pragma omp parallel shared(data, offsets, buckets, level, num_processors) default(none)
      {
        #pragma omp for
        for (size_t bucket_id = 0; bucket_id < NUM_BUCKETS; bucket_id++) {
          parallel_radix_sort(data + offsets[bucket_id], buckets[bucket_id],
                              level + 1, num_processors);
        }
      }
    }
  }

  // 8-bit used for radix
  size_t bucket(void *data, const size_t &level) {
    return (size_t) (*(static_cast<char *>(data) + level) & 0xFF);
  }

  template<class T>
  void permute(T *data, const size_t &level, section_t *p, const size_t &num_threads, const size_t &thread_id) {
    for (size_t bucket_id = 0; bucket_id < NUM_BUCKETS; bucket_id++) {
      size_t head = ((p + bucket_id * num_threads) + thread_id)->head;
      while (head < ((p + bucket_id * num_threads) + thread_id)->tail) {
        T &v = data[head];
        size_t k = bucket(&v, level);
        while (k != bucket_id &&
               ((p + k * num_threads) + thread_id)->head < ((p + k * num_threads) + thread_id)->tail) {
          std::swap(v, data[((p + k * num_threads) + thread_id)->head++]);
          k = bucket(&v, level);
        }
        head++;
        if (k == bucket_id) {
          ((p + bucket_id * num_threads) + thread_id)->head++;
        }
      }
    }
  }

  template<class T>
  void repair(T *data, const size_t &level, section_t *g, section_t *p, const size_t &num_threads,
              const size_t &bucket_id) {
    size_t tail = g[bucket_id].tail;
    for (size_t thread_id = 0; thread_id < num_threads; thread_id++) {
      size_t head = ((p + bucket_id * num_threads) + thread_id)->head;
      while (head < ((p + bucket_id * num_threads) + thread_id)->tail && head < tail) {
        T &v = data[head++];
        if (bucket(&v, level) != bucket_id) {
          while (head < tail) {
            T &w = data[--tail];
            if (bucket(&w, level) == bucket_id) {
              std::swap(v, w);
              break;
            }
          }
        }
      }
    }
    g[bucket_id].head = tail;
  }

}
