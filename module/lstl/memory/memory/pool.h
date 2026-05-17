#ifndef LSTL_MEMORY_POOL_H
#define LSTL_MEMORY_POOL_H

#include <cstddef>
#include <cstdint>

#include "pool_single.h"

namespace lstl {

// 单线程二级空间配置器门面（默认 alloc 策略）
struct pool_alloc_t {
  static void* allocate(size_t n) { return detail::PoolSingle::allocate(n); }

  static void deallocate(void* p, size_t n) { detail::PoolSingle::deallocate(p, n); }

  static void (*set_malloc_handler(void (*f)()))() {
    return detail::PoolSingle::set_malloc_handler(f);
  }

  static size_t purge_idle_memory() { return detail::PoolSingle::purge_idle_memory(); }

  static size_t small_mapped_bytes() { return detail::PoolSingle::mapped_bytes(); }

#ifndef LSTL_POOL_DISABLE_STATS
  static uint64_t small_alloc_count() { return detail::PoolSingle::small_alloc_count(); }

  static uint64_t small_free_count() { return detail::PoolSingle::small_free_count(); }

  static uint64_t refill_count() { return detail::PoolSingle::refill_count(); }
#endif
};

}  // namespace lstl

#endif  // LSTL_MEMORY_POOL_H
