#ifndef KV_POOL_SPAN_H
#define KV_POOL_SPAN_H

#include <cstdint>

#include "../config.h"
#include "size_class.h"

namespace kv {
namespace detail {

enum { KV_SPAN_MAGIC = 0x5350414Eu };

struct SpanHeader {
  uint32_t magic;
  uint16_t arena_id;
  uint16_t size_class;
  uint16_t block_size;
  uint16_t reserved;
  uint32_t block_count;
  uint32_t free_count;
  uint32_t flags;
  SpanHeader* next;
  void* alloc_base;
  size_t alloc_size;
};

inline size_t span_header_size() {
  return align_up(sizeof(SpanHeader), KV_POOL_ALIGN);
}

inline char* span_user_base(SpanHeader* header) {
  return reinterpret_cast<char*>(header) + span_header_size();
}

inline const SpanHeader* span_at_page(const void* p) {
  const uintptr_t addr = reinterpret_cast<uintptr_t>(p);
  const uintptr_t page = addr & ~(static_cast<uintptr_t>(KV_POOL_PAGE_SIZE) - 1);
  return reinterpret_cast<const SpanHeader*>(page);
}

inline bool span_owned_by(const void* p, uint32_t arena_id) {
  if (!p) {
    return false;
  }
  const SpanHeader* header = span_at_page(p);
  return header->magic == KV_SPAN_MAGIC && header->arena_id == arena_id;
}

inline SpanHeader* span_from_ptr(const void* p) {
  if (!p) {
    return 0;
  }
  const SpanHeader* header = span_at_page(p);
  if (header->magic == KV_SPAN_MAGIC) {
    return const_cast<SpanHeader*>(header);
  }
  return 0;
}

}  // namespace detail
}  // namespace kv

#endif  // KV_POOL_SPAN_H
