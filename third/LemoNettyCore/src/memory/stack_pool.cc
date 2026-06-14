#include "lemo/memory/stack_pool.h"

#include <atomic>
#include <cstdlib>
#include <pthread.h>

namespace lemo::memory {

namespace {

std::atomic<uint64_t> g_tls_hit_alloc{0};
std::atomic<uint64_t> g_global_hit_alloc{0};
std::atomic<uint64_t> g_malloc_alloc{0};
std::atomic<uint64_t> g_live_stacks{0};
std::atomic<uint64_t> g_oom_count{0};
std::atomic<uint64_t> g_trim_tls_freed{0};
std::atomic<uint64_t> g_trim_global_freed{0};

StackPool::Config g_config;
pthread_mutex_t g_global_mutex = PTHREAD_MUTEX_INITIALIZER;
void* g_global_freelist = nullptr;
uint32_t g_global_cached = 0;

thread_local void* t_tls_freelist = nullptr;
thread_local uint8_t t_tls_cached = 0;

void* malloc_or_throw(size_t bytes) {
  void* p = std::malloc(bytes);
  if (!p) {
    g_oom_count.fetch_add(1, std::memory_order_relaxed);
    throw std::bad_alloc();
  }
  return p;
}

void push_tls(void* p) {
  *static_cast<void**>(p) = t_tls_freelist;
  t_tls_freelist = p;
  ++t_tls_cached;
}

bool pop_tls(void** out) {
  if (t_tls_cached == 0) {
    return false;
  }
  void* p = t_tls_freelist;
  t_tls_freelist = *static_cast<void**>(p);
  --t_tls_cached;
  *out = p;
  return true;
}

bool push_global(void* p) {
  if (g_global_cached >= g_config.max_global_cached) {
    return false;
  }
  pthread_mutex_lock(&g_global_mutex);
  if (g_global_cached >= g_config.max_global_cached) {
    pthread_mutex_unlock(&g_global_mutex);
    return false;
  }
  *static_cast<void**>(p) = g_global_freelist;
  g_global_freelist = p;
  ++g_global_cached;
  pthread_mutex_unlock(&g_global_mutex);
  return true;
}

bool pop_global(void** out) {
  pthread_mutex_lock(&g_global_mutex);
  if (g_global_cached == 0) {
    pthread_mutex_unlock(&g_global_mutex);
    return false;
  }
  void* p = g_global_freelist;
  g_global_freelist = *static_cast<void**>(p);
  --g_global_cached;
  pthread_mutex_unlock(&g_global_mutex);
  *out = p;
  return true;
}

uint32_t global_cached_count() {
  pthread_mutex_lock(&g_global_mutex);
  const uint32_t n = g_global_cached;
  pthread_mutex_unlock(&g_global_mutex);
  return n;
}

void free_freelist(void* head, std::atomic<uint64_t>* trim_counter) {
  while (head) {
    void* next = *static_cast<void**>(head);
    std::free(head);
    head = next;
    if (trim_counter) {
      trim_counter->fetch_add(1, std::memory_order_relaxed);
    }
  }
}

}  // namespace

void StackPool::set_config(const Config& cfg) {
  g_config = cfg;
}

StackPool::Config StackPool::config() { return g_config; }

void* StackPool::allocate(size_t bytes) {
  if (bytes == 0) {
    return nullptr;
  }
  if (bytes != kPooledStackSize) {
    void* p = malloc_or_throw(bytes);
    return p;
  }

  void* p = nullptr;
  if (pop_tls(&p)) {
    g_tls_hit_alloc.fetch_add(1, std::memory_order_relaxed);
  } else if (pop_global(&p)) {
    g_global_hit_alloc.fetch_add(1, std::memory_order_relaxed);
  } else {
    p = malloc_or_throw(kPooledStackSize);
    g_malloc_alloc.fetch_add(1, std::memory_order_relaxed);
  }
  g_live_stacks.fetch_add(1, std::memory_order_relaxed);
  return p;
}

void StackPool::deallocate(void* p, size_t bytes) {
  if (!p) {
    return;
  }
  if (bytes != kPooledStackSize) {
    std::free(p);
    return;
  }

  g_live_stacks.fetch_sub(1, std::memory_order_relaxed);

  if (t_tls_cached < g_config.max_tls_cached) {
    push_tls(p);
    return;
  }
  if (push_global(p)) {
    return;
  }
  std::free(p);
}

void StackPool::trim_thread_cache() {
  void* head = t_tls_freelist;
  t_tls_freelist = nullptr;
  t_tls_cached = 0;
  free_freelist(head, &g_trim_tls_freed);
}

void StackPool::trim_global_cache() {
  pthread_mutex_lock(&g_global_mutex);
  void* head = g_global_freelist;
  g_global_freelist = nullptr;
  g_global_cached = 0;
  pthread_mutex_unlock(&g_global_mutex);
  free_freelist(head, &g_trim_global_freed);
}

void StackPool::trim_all() {
  trim_thread_cache();
  trim_global_cache();
}

StackPool::Stats StackPool::stats() {
  Stats s;
  s.tls_hit_alloc = g_tls_hit_alloc.load(std::memory_order_relaxed);
  s.global_hit_alloc = g_global_hit_alloc.load(std::memory_order_relaxed);
  s.malloc_alloc = g_malloc_alloc.load(std::memory_order_relaxed);
  s.live_stacks = g_live_stacks.load(std::memory_order_relaxed);
  s.oom_count = g_oom_count.load(std::memory_order_relaxed);
  s.trim_tls_freed = g_trim_tls_freed.load(std::memory_order_relaxed);
  s.trim_global_freed = g_trim_global_freed.load(std::memory_order_relaxed);
  s.tls_cached = t_tls_cached;
  s.global_cached = global_cached_count();
  s.cached_bytes =
      static_cast<uint64_t>(s.tls_cached + s.global_cached) * kPooledStackSize;
  return s;
}

void StackPool::reset_stats() {
  g_tls_hit_alloc.store(0, std::memory_order_relaxed);
  g_global_hit_alloc.store(0, std::memory_order_relaxed);
  g_malloc_alloc.store(0, std::memory_order_relaxed);
  g_oom_count.store(0, std::memory_order_relaxed);
  g_trim_tls_freed.store(0, std::memory_order_relaxed);
  g_trim_global_freed.store(0, std::memory_order_relaxed);
}

}  // namespace lemo::memory
