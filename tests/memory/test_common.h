#ifndef LSTL_TEST_COMMON_H
#define LSTL_TEST_COMMON_H

#include <cstdio>
#include <cstdlib>

#define LSTL_CHECK(cond)                                                     \
  do {                                                                       \
    if (!(cond)) {                                                           \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
      std::exit(1);                                                          \
    }                                                                        \
  } while (0)

#define LSTL_TEST_MAIN()                                                     \
  int main() {                                                               \
    std::printf("PASS %s\n", __FILE__);                                      \
    return 0;                                                                \
  }

#endif  // LSTL_TEST_COMMON_H
