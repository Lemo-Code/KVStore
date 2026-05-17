#ifndef LSTL_MEMORY_SPAN_H
#define LSTL_MEMORY_SPAN_H

#include <cstddef>
#include <cstdint>

#include "../config.h"
#include "size_class.h"

namespace lstl {
namespace detail {

enum {
  LSTL_SPAN_MAGIC = 0x5350414Eu  // 'SPAN'
};

struct SpanHeader {
  uint32_t magic;
  uint16_t size_class;
  uint16_t block_size;
  uint32_t block_count;
  uint32_t free_count;
  uint32_t flags;  // bit0: bump 活跃区
  SpanHeader* next;
  void* alloc_base;
  size_t alloc_size;
};

inline SpanHeader* span_from_ptr(const void* p) {
  if (!p) {
    return 0;
  }
  const uintptr_t addr = reinterpret_cast<uintptr_t>(p);
  const uintptr_t page = addr & ~(static_cast<uintptr_t>(LSTL_PAGE_SIZE) - 1);
  SpanHeader* header = reinterpret_cast<SpanHeader*>(page);
  if (header->magic == LSTL_SPAN_MAGIC) {
    return header;
  }
  return 0;
}

inline size_t span_header_size() {
  return align_up(sizeof(SpanHeader), LSTL_ALIGN);
}

inline char* span_user_base(SpanHeader* header) {
  return reinterpret_cast<char*>(header) + span_header_size();
}

}  // namespace detail
}  // namespace lstl

#endif  // LSTL_MEMORY_SPAN_H
