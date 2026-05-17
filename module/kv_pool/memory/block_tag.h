#ifndef KV_POOL_BLOCK_TAG_H
#define KV_POOL_BLOCK_TAG_H

#include <atomic>
#include <cstdint>

#include "../config.h"

namespace kv {
namespace detail {

#ifndef KV_POOL_TAG_SIZE
#define KV_POOL_TAG_SIZE 4
#endif

enum { KV_BLOCK_TAG_NIBBLE = 0xAu };

inline uint16_t current_thread_tag() {
  static thread_local uint16_t tag = 0;
  if (tag == 0) {
    static std::atomic<uint32_t> next{1};
    tag = static_cast<uint16_t>(next.fetch_add(1, std::memory_order_relaxed) & 0xFFFFu);
    if (tag == 0) {
      tag = 1;
    }
  }
  return tag;
}

inline void stamp_slot(void* slot, uint16_t thread_tag, uint16_t arena_id) {
  uint32_t* word = static_cast<uint32_t*>(slot);
  *word = (KV_BLOCK_TAG_NIBBLE << 28) | ((static_cast<uint32_t>(arena_id) & 0xFFFu) << 16) |
          (static_cast<uint32_t>(thread_tag) & 0xFFFFu);
}

inline uint32_t tag_word(const void* user) {
  const uint32_t* word = reinterpret_cast<const uint32_t*>(
      static_cast<const char*>(user) - KV_POOL_TAG_SIZE);
  return *word;
}

inline bool tag_is_pool_word(uint32_t word) {
  return (word >> 28) == KV_BLOCK_TAG_NIBBLE;
}

inline bool tag_is_pool(const void* user) {
  if (!user) {
    return false;
  }
  return tag_is_pool_word(tag_word(user));
}

inline uint16_t tag_thread(const void* user) {
  return static_cast<uint16_t>(tag_word(user) & 0xFFFFu);
}

inline uint16_t tag_arena(const void* user) {
  return static_cast<uint16_t>((tag_word(user) >> 16) & 0xFFFu);
}

inline void* slot_to_user(void* slot) {
  return static_cast<char*>(slot) + KV_POOL_TAG_SIZE;
}

inline void* user_to_slot(void* user) {
  return static_cast<char*>(user) - KV_POOL_TAG_SIZE;
}

}  // namespace detail
}  // namespace kv

#endif  // KV_POOL_BLOCK_TAG_H
