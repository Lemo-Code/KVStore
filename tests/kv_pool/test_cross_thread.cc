#include "test_common.h"

#include <atomic>
#include <future>
#include <vector>

#include "kv_pool.h"

namespace {

std::atomic<int> g_failures(0);
const size_t kSize = 32;

std::vector<void*> worker_alloc(int count) {
  std::vector<void*> items;
  items.reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    void* p = kv::pool_alloc::allocate(kSize);
    if (!p) {
      g_failures.fetch_add(1);
      return items;
    }
    items.push_back(p);
  }
  kv::pool_alloc::trim_thread_cache();
  return items;
}

}  // namespace

int main() {
  const int kItems = 10000;

  std::vector<void*> items =
      std::async(std::launch::async, worker_alloc, kItems).get();

  KV_CHECK(static_cast<int>(items.size()) == kItems);

  for (size_t i = 0; i < items.size(); ++i) {
    kv::pool_alloc::deallocate(items[i], kSize);
  }
  kv::pool_alloc::trim_thread_cache();

  KV_CHECK(g_failures.load() == 0);
  KV_CHECK(kv::pool_alloc::remote_enqueue_count() > 0u);

  std::printf("PASS test_cross_thread (items=%d remote_enqueues=%llu)\n", kItems,
              static_cast<unsigned long long>(kv::pool_alloc::remote_enqueue_count()));
  return 0;
}
