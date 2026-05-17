#ifndef LSTL_MEMORY_OOM_H
#define LSTL_MEMORY_OOM_H

#include <cstdlib>
#include <new>

#ifdef LSTL_OOM_MODE_CERR
#include <iostream>
#endif

namespace lstl {
namespace detail {

inline void lstl_oom_fail() {
#ifdef LSTL_OOM_MODE_CERR
  std::cerr << "lstl: out of memory\n";
  std::abort();
#else
  throw std::bad_alloc();
#endif
}

}  // namespace detail
}  // namespace lstl

#endif  // LSTL_MEMORY_OOM_H
