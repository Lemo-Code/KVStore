#ifndef NET_TEST_COMMON_H
#define NET_TEST_COMMON_H

#include <cstdio>
#include <cstdlib>

/** 断言失败时打印位置并退出（风格对齐 KV_CHECK / LSTL_CHECK） */
#define NET_CHECK(cond)                                                        \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)

#endif  // NET_TEST_COMMON_H
