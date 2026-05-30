#ifndef KV_POOL_SIZE_CLASS_H
#define KV_POOL_SIZE_CLASS_H

#include <cstddef>

#include "../config.h"

namespace kv {
namespace detail {

inline size_t align_up(size_t bytes, size_t align = KV_POOL_ALIGN) {
  return (bytes + align - 1) & ~(align - 1);
}

inline size_t size_class_index(size_t bytes) {
  return (align_up(bytes) / KV_POOL_ALIGN) - 1;
}

inline size_t size_class_bytes(size_t index) {
  return (index + 1) * KV_POOL_ALIGN;
}

inline bool is_small_request(size_t bytes) {
  return bytes <= KV_POOL_MAX_BYTES;
}

#ifndef KV_POOL_TAG_SIZE
#define KV_POOL_TAG_SIZE 4
#endif

inline size_t internal_bytes(size_t user_bytes) {
  return align_up(user_bytes + KV_POOL_TAG_SIZE);
}

inline size_t internal_index(size_t user_bytes) {
  return size_class_index(internal_bytes(user_bytes));
}

inline bool is_untagged_request(size_t user_bytes) {
  return align_up(user_bytes) <= KV_POOL_UNTAGGED_MAX;
}

inline size_t pool_block_size(size_t user_bytes) {
  const size_t bytes = align_up(user_bytes);
  if (is_untagged_request(bytes)) {
    return bytes;
  }
  return internal_bytes(bytes);
}

inline size_t pool_index(size_t user_bytes) {
  const size_t bytes = align_up(user_bytes);
  if (is_untagged_request(bytes)) {
    return size_class_index(bytes);
  }
  return internal_index(bytes);
}

inline bool is_large_request(size_t bytes) {
  return bytes > KV_POOL_MAX_BYTES && bytes <= KV_POOL_LARGE_MAX;
}

}  // namespace detail
}  // namespace kv

#endif  // KV_POOL_SIZE_CLASS_H
