#ifndef LSTL_TEMPORARY_BUFFER_H
#define LSTL_TEMPORARY_BUFFER_H

#include <cstddef>
#include <utility>

#include "config.h"
#include "memory/malloc_alloc.h"

namespace lstl {

// 临时缓冲固定走 malloc，避免占用用户自定义池
template <typename T>
inline std::pair<T*, ptrdiff_t> get_temporary_buffer(ptrdiff_t len) {
  const ptrdiff_t max_elems =
      static_cast<ptrdiff_t>(LSTL_MAX_BYTES / (sizeof(T) > 0 ? sizeof(T) : 1));
  if (len > max_elems) {
    len = max_elems;
  }
  if (len <= 0) {
    return std::pair<T*, ptrdiff_t>(0, 0);
  }

  T* p = static_cast<T*>(malloc_alloc_t::allocate(static_cast<size_t>(len) * sizeof(T)));
  if (!p) {
    return std::pair<T*, ptrdiff_t>(0, 0);
  }
  return std::pair<T*, ptrdiff_t>(p, len);
}

template <typename T>
inline void return_temporary_buffer(T* p) {
  malloc_alloc_t::deallocate(p, 0);
}

}  // namespace lstl

#endif  // LSTL_TEMPORARY_BUFFER_H
