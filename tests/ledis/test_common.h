#pragma once

#include <cstdio>
#include <cstdlib>

#define LEDIS_CHECK(cond)                                           \
  do {                                                              \
    if (!(cond)) {                                                  \
      std::fprintf(stderr, "CHECK failed: %s at %s:%d\n", #cond,    \
                   __FILE__, __LINE__);                             \
      std::exit(1);                                                 \
    }                                                               \
  } while (0)
