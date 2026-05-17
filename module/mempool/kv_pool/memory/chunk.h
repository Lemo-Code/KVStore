#ifndef KV_POOL_CHUNK_H
#define KV_POOL_CHUNK_H

#include <atomic>
#include <cstdint>

namespace kv {
namespace detail {

enum { KV_CHUNK_MAGIC = 0x43484B55u };  // "CHKU"

struct ChunkHeader {
  ChunkHeader* next_registry;
  uint32_t magic;
  uint16_t arena_id;
  uint16_t thread_tag;
  char* user_start;
  char* user_end;
};

inline bool chunk_contains(const ChunkHeader* header, const void* p) {
  if (!header || !p) {
    return false;
  }
  const char* cp = static_cast<const char*>(p);
  return cp >= header->user_start && cp < header->user_end;
}

}  // namespace detail
}  // namespace kv

#endif  // KV_POOL_CHUNK_H
