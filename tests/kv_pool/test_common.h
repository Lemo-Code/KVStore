#ifndef KV_POOL_TEST_COMMON_H
#define KV_POOL_TEST_COMMON_H

#include <cstdio>
#include <cstdlib>

#define KV_CHECK(cond)                                                       \
  do {                                                                       \
    if (!(cond)) {                                                           \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
      std::exit(1);                                                          \
    }                                                                        \
  } while (0)

#endif  // KV_POOL_TEST_COMMON_H
