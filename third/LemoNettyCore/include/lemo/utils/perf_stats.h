#pragma once

#include <atomic>

// Release 默认关闭热路径统计；调试时可 -DLEMO_PERF_STATS=1
#if defined(LEMO_PERF_STATS) && LEMO_PERF_STATS
#define LEMO_PERF_STATS_ENABLED 1
#else
#define LEMO_PERF_STATS_ENABLED 0
#endif

#define LEMO_PERF_FETCH_ADD(counter, delta)            \
  do {                                                 \
    if (LEMO_PERF_STATS_ENABLED) {                     \
      (counter).fetch_add((delta), std::memory_order_relaxed); \
    }                                                  \
  } while (0)

#define LEMO_PERF_INC(counter) LEMO_PERF_FETCH_ADD(counter, 1)
