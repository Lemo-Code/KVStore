#ifndef KV_POOL_TCACHE_H
#define KV_POOL_TCACHE_H

#include <cstdint>

#include "../config.h"
#include "arena.h"
#include "freelist.h"

namespace kv {
namespace detail {

struct TCacheBin {
  FreeNode* head;
  uint16_t count;
};

class TCache {
 public:
  TCache() : arena_id_(0), inited_(false) {
    for (int i = 0; i < KV_POOL_FREELISTS; ++i) {
      bins_[i].head = 0;
      bins_[i].count = 0;
    }
  }

  void init(uint32_t arena_id) {
    arena_id_ = arena_id;
    inited_ = true;
  }

  bool inited() const { return inited_; }

  uint32_t arena_id() const { return arena_id_; }

  FreeNode* pop(size_t index) {
    if (bins_[index].head) {
      FreeNode* node = freelist_pop(&bins_[index].head);
      --bins_[index].count;
      return node;
    }
    return 0;
  }

  void push(Arena& arena, size_t index, FreeNode* node) {
    if (bins_[index].count < KV_POOL_TCACHE_MAX) {
      freelist_push(&bins_[index].head, node);
      ++bins_[index].count;
      return;
    }
    flush_bin(arena, index);
    freelist_push(&bins_[index].head, node);
    bins_[index].count = 1;
  }

  void flush_all(Arena& arena) {
    for (size_t i = 0; i < KV_POOL_FREELISTS; ++i) {
      flush_bin(arena, i);
    }
  }

 private:
  void flush_bin(Arena& arena, size_t index) {
    if (!bins_[index].head) {
      return;
    }
    FreeNode* head = bins_[index].head;
    FreeNode* tail = head;
    int n = 1;
    while (tail->next) {
      tail = tail->next;
      ++n;
    }
    arena.flush_tcache_bin(index, head, tail, n);
    bins_[index].head = 0;
    bins_[index].count = 0;
  }

  uint32_t arena_id_;
  bool inited_;
  TCacheBin bins_[KV_POOL_FREELISTS];
};

}  // namespace detail
}  // namespace kv

#endif  // KV_POOL_TCACHE_H
