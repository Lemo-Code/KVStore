#ifndef LSTL_MEMORY_LARGE_H
#define LSTL_MEMORY_LARGE_H

#include <cstddef>
#include <cstdint>

#include "../config.h"
#include "malloc_alloc.h"
#include "size_class.h"

namespace lstl {
namespace detail {

enum { LSTL_LARGE_MAGIC = 0x4C415247u };  // 'LARG'

struct LargeHeader {
  uint32_t magic;
  uint32_t size;
};

inline void* large_allocate(size_t bytes) {
  const size_t total = sizeof(LargeHeader) + bytes;
  void* raw = malloc_alloc_t::allocate(total);
  if (!raw) {
    return 0;
  }
  LargeHeader* header = static_cast<LargeHeader*>(raw);
  header->magic = LSTL_LARGE_MAGIC;
  header->size = static_cast<uint32_t>(bytes);
  return static_cast<char*>(raw) + sizeof(LargeHeader);
}

inline void large_deallocate(void* p, size_t bytes) {
  if (!p) {
    return;
  }
  char* raw = static_cast<char*>(p) - sizeof(LargeHeader);
  LargeHeader* header = reinterpret_cast<LargeHeader*>(raw);
  (void)bytes;
  malloc_alloc_t::deallocate(raw, sizeof(LargeHeader) + header->size);
}

}  // namespace detail
}  // namespace lstl

#endif  // LSTL_MEMORY_LARGE_H
