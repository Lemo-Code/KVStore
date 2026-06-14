#include "lemo/memory/stack_pool.h"

#include "test_common.h"

#include <cstdio>
#include <thread>

using lemo::memory::StackPool;

static StackPool::Config default_config() {
  StackPool::Config cfg;
  cfg.max_tls_cached = StackPool::kDefaultMaxTlsCached;
  cfg.max_global_cached = StackPool::kDefaultMaxGlobalCached;
  return cfg;
}

static void test_tls_reuse_hit() {
  StackPool::set_config(default_config());
  StackPool::trim_all();
  StackPool::reset_stats();

  void* p = StackPool::allocate(StackPool::kPooledStackSize);
  LEMO_CHECK(p != nullptr);
  StackPool::deallocate(p, StackPool::kPooledStackSize);

  void* q = StackPool::allocate(StackPool::kPooledStackSize);
  LEMO_CHECK(q != nullptr);
  StackPool::deallocate(q, StackPool::kPooledStackSize);

  const StackPool::Stats s = StackPool::stats();
  LEMO_CHECK(s.tls_hit_alloc >= 1);
  LEMO_CHECK(s.live_stacks == 0);
}

static void test_global_spare_cross_thread() {
  StackPool::Config cfg;
  cfg.max_tls_cached = 0;
  cfg.max_global_cached = 8;
  StackPool::set_config(cfg);
  StackPool::trim_all();
  StackPool::reset_stats();

  std::thread producer([]() {
    void* p = StackPool::allocate(StackPool::kPooledStackSize);
    LEMO_CHECK(p != nullptr);
    StackPool::deallocate(p, StackPool::kPooledStackSize);
  });
  producer.join();

  StackPool::reset_stats();
  void* q = StackPool::allocate(StackPool::kPooledStackSize);
  LEMO_CHECK(q != nullptr);
  const StackPool::Stats s = StackPool::stats();
  LEMO_CHECK(s.global_hit_alloc >= 1);
  StackPool::deallocate(q, StackPool::kPooledStackSize);

  StackPool::set_config(default_config());
  StackPool::trim_all();
}

static void test_trim_clears_cache() {
  StackPool::set_config(default_config());
  StackPool::trim_all();
  StackPool::reset_stats();

  void* p = StackPool::allocate(StackPool::kPooledStackSize);
  StackPool::deallocate(p, StackPool::kPooledStackSize);

  StackPool::Stats before = StackPool::stats();
  LEMO_CHECK(before.tls_cached >= 1);

  StackPool::trim_thread_cache();
  const StackPool::Stats after = StackPool::stats();
  LEMO_CHECK(after.trim_tls_freed >= 1);
  LEMO_CHECK(after.tls_cached == 0);
}

static void test_custom_size_bypass_pool() {
  StackPool::set_config(default_config());
  StackPool::trim_all();
  StackPool::reset_stats();

  const size_t custom = 64 * 1024;
  void* p = StackPool::allocate(custom);
  LEMO_CHECK(p != nullptr);
  StackPool::deallocate(p, custom);

  const StackPool::Stats s = StackPool::stats();
  LEMO_CHECK(s.malloc_alloc == 0);
  LEMO_CHECK(s.live_stacks == 0);
}

int main() {
  test_tls_reuse_hit();
  test_global_spare_cross_thread();
  test_trim_clears_cache();
  test_custom_size_bypass_pool();
  std::fprintf(stderr, "PASS test_stack_pool\n");
  return 0;
}
