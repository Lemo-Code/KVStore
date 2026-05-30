#ifndef LSTL_MEMORY_OOM_H
#define LSTL_MEMORY_OOM_H

#include <cstdio>
#include <cstdlib>

#include "../exception.h"

namespace lstl {
namespace detail {

inline void lstl_oom_fail() {
#ifdef LSTL_OOM_MODE_CERR
  const char msg[] = "lstl: out of memory\n";
  (void)::fwrite(msg, 1, sizeof(msg) - 1, stderr);
  ::abort();
#else
  throw bad_alloc();
#endif
}

}  // namespace detail
}  // namespace lstl

#endif  // LSTL_MEMORY_OOM_H
