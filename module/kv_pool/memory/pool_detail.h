#ifndef KV_POOL_POOL_DETAIL_H
#define KV_POOL_POOL_DETAIL_H

#include <atomic>
#include <cstdlib>
#include <new>
#include <vector>

#include "../config.h"
#include "arena.h"
#include "chunk.h"
#include "freelist.h"
#include "large.h"
#include "malloc_alloc.h"
#include "remote_queue.h"
#include "size_class.h"
#include "thread_pool.h"

namespace kv {
namespace detail {

struct PoolState {
  std::vector<Arena*> arenas;
  std::atomic<uint64_t> remote_enqueues;
  std::atomic<uint32_t> init_state;

  enum { kInitIdle = 0, kInitRunning = 1, kInitDone = 2 };

  PoolState() : remote_enqueues(0), init_state(kInitIdle) {}

  static PoolState& instance() {
    static PoolState state;
    return state;
  }

  void init() {
    uint32_t state = init_state.load(std::memory_order_acquire);
    if (state == kInitDone) {
      return;
    }
    if (state == kInitIdle) {
      uint32_t expected = kInitIdle;
      if (init_state.compare_exchange_strong(expected, kInitRunning,
                                            std::memory_order_acq_rel)) {
        uint32_t n = KV_POOL_ARENA_COUNT;
        if (n == 0) {
          n = default_arena_count();
        }
        arenas.reserve(n);
        for (uint32_t i = 0; i < n; ++i) {
          arenas.push_back(new Arena(i));
        }
        init_state.store(kInitDone, std::memory_order_release);
        return;
      }
    }
    while (init_state.load(std::memory_order_acquire) != kInitDone) {
    }
  }

  uint32_t bind_arena(uint16_t thread_tag) {
    init();
    const uint32_t n = static_cast<uint32_t>(arenas.size());
    return static_cast<uint32_t>(thread_tag) % n;
  }
};

struct TlsFastCtx : ThreadLocalPool {
  RemoteBatch* remote_partials[64];

  TlsFastCtx() {
    for (int i = 0; i < 64; ++i) {
      remote_partials[i] = 0;
    }
    PoolState& state = PoolState::instance();
    state.init();
    const uint16_t tag = current_thread_tag();
    const uint32_t arena_id = state.bind_arena(tag);
    init(arena_id, tag, state.arenas[arena_id]);
  }

  ~TlsFastCtx() {
    PoolState& state = PoolState::instance();
    state.init();
    if (inited() && arena()) {
      flush_all();
    }
    for (size_t i = 0; i < state.arenas.size() && i < 64; ++i) {
      RemoteBatch* batch = remote_partials[i];
      if (batch && batch->count > 0) {
        state.arenas[i]->remote().push(batch);
        remote_partials[i] = 0;
      } else if (batch) {
        std::free(batch);
        remote_partials[i] = 0;
      }
    }
  }
};

inline TlsFastCtx& tls_ctx() {
  static thread_local TlsFastCtx ctx;
  return ctx;
}

inline ThreadLocalPool& tls_pool() { return tls_ctx(); }

inline void remote_enqueue(uint32_t arena_id, uint16_t size_class, void* ptr) {
  PoolState& state = PoolState::instance();
  state.init();
  if (arena_id >= state.arenas.size()) {
    state.arenas[0]->deposit_bin(size_class, static_cast<FreeNode*>(ptr),
                                 static_cast<FreeNode*>(ptr));
    return;
  }

  TlsFastCtx& ctx = tls_ctx();
  if (arena_id >= 64) {
    state.arenas[arena_id]->deposit_bin(size_class, static_cast<FreeNode*>(ptr),
                                        static_cast<FreeNode*>(ptr));
    return;
  }

  RemoteBatch*& batch_slot = ctx.remote_partials[arena_id];
  if (!batch_slot) {
    batch_slot = static_cast<RemoteBatch*>(std::malloc(sizeof(RemoteBatch)));
    if (!batch_slot) {
      throw std::bad_alloc();
    }
    batch_slot->count = 0;
    batch_slot->next = 0;
  }

  batch_slot->items[batch_slot->count].ptr = ptr;
  batch_slot->items[batch_slot->count].size_class = size_class;
  ++batch_slot->count;

  state.remote_enqueues.fetch_add(1, std::memory_order_relaxed);

  if (batch_slot->count >= KV_POOL_REMOTE_BATCH) {
    state.arenas[arena_id]->remote().push(batch_slot);
    batch_slot = 0;
  }
}

inline void flush_remote_partials() {
  PoolState& state = PoolState::instance();
  state.init();
  TlsFastCtx& ctx = tls_ctx();
  for (size_t i = 0; i < state.arenas.size() && i < 64; ++i) {
    RemoteBatch* batch = ctx.remote_partials[i];
    if (batch && batch->count > 0) {
      state.arenas[i]->remote().push(batch);
      ctx.remote_partials[i] = 0;
    }
  }
}

inline ChunkHeader* find_chunk_global(const void* p) {
  PoolState& state = PoolState::instance();
  state.init();
  for (size_t i = 0; i < state.arenas.size(); ++i) {
    ChunkHeader* header = state.arenas[i]->find_chunk(p);
    if (header) {
      return header;
    }
  }
  return 0;
}

}  // namespace detail
}  // namespace kv

#endif  // KV_POOL_POOL_DETAIL_H
