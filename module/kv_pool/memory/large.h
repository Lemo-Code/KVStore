#ifndef KV_POOL_LARGE_H
#define KV_POOL_LARGE_H

#include <cstdint>

#include "malloc_alloc.h"

namespace kv {
namespace detail {

enum { KV_LARGE_MAGIC = 0x4C415247u };

struct LargeHeader {
  uint32_t magic;
  uint32_t size;
};

inline void* large_allocate(size_t bytes) {
  const size_t total = sizeof(LargeHeader) + bytes;
  void* raw = malloc_alloc::allocate(total);
  LargeHeader* header = static_cast<LargeHeader*>(raw);
  header->magic = KV_LARGE_MAGIC;
  header->size = static_cast<uint32_t>(bytes);
  return static_cast<char*>(raw) + sizeof(LargeHeader);
}

inline void large_deallocate(void* p) {
  if (!p) {
    return;
  }
  char* raw = static_cast<char*>(p) - sizeof(LargeHeader);
  LargeHeader* header = reinterpret_cast<LargeHeader*>(raw);
  malloc_alloc::deallocate(raw, sizeof(LargeHeader) + header->size);
}

}  // namespace detail
}  // namespace kv

#endif  // KV_POOL_LARGE_H
