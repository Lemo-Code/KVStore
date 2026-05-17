#ifndef KV_POOL_SPAN_ALLOC_H
#define KV_POOL_SPAN_ALLOC_H

#include <cstdlib>
#include <cstring>

#include "../config.h"
#include "malloc_alloc.h"
#include "size_class.h"
#include "span.h"

namespace kv {
namespace detail {

// 线程本地直接申请 span 页，无需中央锁。
inline bool alloc_thread_span(uint32_t arena_id, size_t index, size_t block_size, int nobjs,
                              char*& region_start, char*& region_end) {
  const size_t total_bytes = block_size * static_cast<size_t>(nobjs);
  size_t bytes_to_get = 2 * total_bytes;
  if (bytes_to_get < KV_POOL_PAGE_SIZE) {
    bytes_to_get = KV_POOL_PAGE_SIZE;
  }

  const size_t aligned_size = align_up(bytes_to_get, KV_POOL_PAGE_SIZE);
  void* raw = 0;
  void* free_ptr = 0;
  size_t free_size = 0;

#if defined(__linux__)
  if (posix_memalign(&raw, KV_POOL_PAGE_SIZE, aligned_size) == 0 && raw) {
    free_ptr = raw;
    free_size = aligned_size;
  }
#endif
  if (!raw) {
    free_size = aligned_size + KV_POOL_PAGE_SIZE;
    void* heap = malloc_alloc::allocate(free_size);
    if (!heap) {
      return false;
    }
    raw = reinterpret_cast<void*>(
        align_up(reinterpret_cast<uintptr_t>(heap), KV_POOL_PAGE_SIZE));
    free_ptr = heap;
  }

  SpanHeader* span = static_cast<SpanHeader*>(raw);
  std::memset(span, 0, sizeof(SpanHeader));
  span->magic = KV_SPAN_MAGIC;
  span->arena_id = arena_id;
  span->size_class = static_cast<uint16_t>(index);
  span->block_size = static_cast<uint16_t>(block_size);
  span->alloc_base = free_ptr;
  span->alloc_size = free_size;

  region_start = span_user_base(span);
  region_end = static_cast<char*>(free_ptr) + free_size;
  return true;
}

}  // namespace detail
}  // namespace kv

#endif  // KV_POOL_SPAN_ALLOC_H
