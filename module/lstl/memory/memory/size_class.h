#ifndef LSTL_MEMORY_SIZE_CLASS_H
#define LSTL_MEMORY_SIZE_CLASS_H

#include <cstddef>

#include "../config.h"

namespace lstl {
namespace detail {

inline size_t align_up(size_t bytes, size_t align = LSTL_ALIGN) {
  return (bytes + align - 1) & ~(align - 1);
}

inline size_t size_class_index(size_t bytes) {
  return (align_up(bytes) / LSTL_ALIGN) - 1;
}

inline size_t size_class_bytes(size_t index) {
  return (index + 1) * LSTL_ALIGN;
}

inline bool is_small_request(size_t bytes) {
  return bytes <= LSTL_POOL_MAX_BYTES;
}

inline bool is_large_request(size_t bytes) {
  return bytes > LSTL_POOL_MAX_BYTES && bytes <= LSTL_LARGE_MAX;
}

}  // namespace detail
}  // namespace lstl

#endif  // LSTL_MEMORY_SIZE_CLASS_H
