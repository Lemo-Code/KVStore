#ifndef KV_POOL_POOL_H
#define KV_POOL_POOL_H

#include <cstddef>
#include <cstdint>

#include "freelist.h"
#include "pool_detail.h"
#include "size_class.h"

namespace kv {

struct pool_alloc {
  static inline void* allocate(size_t n) {
    if (n == 0) {
      return 0;
    }
    if (detail::is_large_request(n)) {
      return detail::large_allocate(n);
    }
    if (!detail::is_small_request(n)) {
      return detail::malloc_alloc::allocate(n);
    }

    detail::ThreadLocalPool& pool = detail::tls_pool();
    const size_t bytes = detail::align_up(n);
    const size_t index = detail::size_class_index(bytes);
    detail::FreeNode** list = &pool.free_list_[index];
    detail::FreeNode* node = detail::freelist_pop(list);
    if (__builtin_expect(!node, 0)) {
      node = static_cast<detail::FreeNode*>(pool.refill_on_miss(index, bytes));
      if (!node) {
        return 0;
      }
    }
    return node;
  }

  static inline void deallocate(void* p, size_t n) {
    if (!p || n == 0) {
      return;
    }
    if (detail::is_large_request(n)) {
      detail::large_deallocate(p);
      return;
    }
    if (!detail::is_small_request(n)) {
      detail::malloc_alloc::deallocate(p, n);
      return;
    }

    detail::ThreadLocalPool& pool = detail::tls_pool();
    const size_t bytes = detail::align_up(n);
    const size_t index = detail::size_class_index(bytes);
    detail::FreeNode* node = static_cast<detail::FreeNode*>(p);

#if KV_POOL_FAST_SAME_THREAD
    pool.push(index, node);
    return;
#else
    if (__builtin_expect(pool.owns_pointer(p), 1)) {
      pool.push(index, node);
      return;
    }

    detail::ChunkHeader* header = detail::find_chunk_global(p);
    if (header && header->thread_tag != pool.thread_tag()) {
      detail::remote_enqueue(header->arena_id, static_cast<uint16_t>(index), p);
      return;
    }

    pool.push(index, node);
#endif
  }

  static inline void trim_thread_cache() {
    detail::flush_remote_partials();
    detail::ThreadLocalPool& pool = detail::tls_pool();
    if (pool.arena()) {
      pool.arena()->flush_remote();
    }
    pool.flush_all();
  }

  static inline uint32_t arena_count() {
    detail::PoolState::instance().init();
    return static_cast<uint32_t>(detail::PoolState::instance().arenas.size());
  }

  static inline uint64_t remote_enqueue_count() {
    return detail::PoolState::instance().remote_enqueues.load(std::memory_order_relaxed);
  }
};

}  // namespace kv

#endif  // KV_POOL_POOL_H
