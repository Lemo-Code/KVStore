#ifndef KV_POOL_ARENA_H
#define KV_POOL_ARENA_H

#include <atomic>
#include <thread>

#include "../config.h"
#include "atomic_freelist.h"
#include "chunk.h"
#include "freelist.h"
#include "remote_queue.h"

namespace kv {
namespace detail {

class Arena {
 public:
  Arena() : id_(0), chunk_head_(0) {}

  explicit Arena(uint32_t id) : id_(id), chunk_head_(0) {}

  uint32_t id() const { return id_; }

  RemoteFreeQueue& remote() { return remote_; }

  void register_chunk(ChunkHeader* chunk) {
    if (!chunk) {
      return;
    }
    ChunkHeader* old = chunk_head_.load(std::memory_order_relaxed);
    do {
      chunk->next_registry = old;
    } while (!chunk_head_.compare_exchange_weak(old, chunk, std::memory_order_release,
                                                std::memory_order_relaxed));
  }

  ChunkHeader* find_chunk(const void* p) const {
    ChunkHeader* chunk = chunk_head_.load(std::memory_order_acquire);
    while (chunk) {
      if (chunk->magic == KV_CHUNK_MAGIC && chunk_contains(chunk, p)) {
        return chunk;
      }
      chunk = chunk->next_registry;
    }
    return 0;
  }

  FreeNode* withdraw_bin(size_t index) {
    FreeNode* head = bins_[index].pop_all();
    if (head) {
      return head;
    }
    drain_remote();
    return bins_[index].pop_all();
  }

  void deposit_bin(size_t index, FreeNode* head, FreeNode* tail) {
    bins_[index].push_all(head, tail);
  }

  void flush_remote() { drain_remote(); }

 private:
  void drain_remote() {
    RemoteBatch* batch = remote_.drain_all();
    while (batch) {
      for (uint32_t i = 0; i < batch->count; ++i) {
        const uint16_t sc = batch->items[i].size_class;
        bins_[sc].push_one(static_cast<FreeNode*>(batch->items[i].ptr));
      }
      RemoteBatch* next = batch->next;
      std::free(batch);
      batch = next;
    }
  }

  uint32_t id_;
  std::atomic<ChunkHeader*> chunk_head_;
  AtomicFreelist bins_[KV_POOL_FREELISTS];
  RemoteFreeQueue remote_;
};

inline uint32_t default_arena_count() {
  const uint32_t hw =
      static_cast<uint32_t>(std::thread::hardware_concurrency());
  uint32_t n = hw > 0 ? hw * 4 : 8;
  if (n > 64) {
    n = 64;
  }
  if (n < 2) {
    n = 2;
  }
  return n;
}

}  // namespace detail
}  // namespace kv

#endif  // KV_POOL_ARENA_H
