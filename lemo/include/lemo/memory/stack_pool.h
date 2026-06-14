#pragma once

#include <cstddef>
#include <cstdint>
#include <new>

namespace lemo::memory {

/**
 * @brief 协程 ucontext 栈专用内存池。
 *
 * 三层供给：线程本地 freelist → 全局 spare → malloc。
 * 仅默认 128KB 走池；自定义栈大小直接 malloc/free。
 */
class StackPool {
 public:
  static constexpr size_t kPooledStackSize = 128 * 1024;
  static constexpr uint32_t kDefaultMaxTlsCached = 32;
  static constexpr uint32_t kDefaultMaxGlobalCached = 64;

  struct Config {
    uint32_t max_tls_cached = kDefaultMaxTlsCached;
    uint32_t max_global_cached = kDefaultMaxGlobalCached;
  };

  struct Stats {
    uint64_t tls_hit_alloc = 0;
    uint64_t global_hit_alloc = 0;
    uint64_t malloc_alloc = 0;
    uint64_t live_stacks = 0;
    uint64_t oom_count = 0;
    uint64_t trim_tls_freed = 0;
    uint64_t trim_global_freed = 0;
    /** 调用线程的 TLS 缓存块数（非全进程合计）。 */
    uint64_t tls_cached = 0;
    uint64_t global_cached = 0;
    uint64_t cached_bytes = 0;
  };

  static void set_config(const Config& cfg);
  static Config config();

  /** 分配栈内存；失败抛 std::bad_alloc。 */
  static void* allocate(size_t bytes);
  static void deallocate(void* p, size_t bytes);

  /** 归还当前线程 TLS 缓存。 */
  static void trim_thread_cache();
  /** 归还全局 spare 缓存。 */
  static void trim_global_cache();
  /** trim TLS + global。 */
  static void trim_all();

  static Stats stats();
  static void reset_stats();
};

}  // namespace lemo::memory
