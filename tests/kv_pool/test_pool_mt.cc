#include "test_common.h"

#include <atomic>
#include <thread>
#include <vector>

#include "kv_pool.h"

namespace {

std::atomic<int> g_failures(0);

void worker(int id, int rounds) {
  try {
    for (int r = 0; r < rounds; ++r) {
      const size_t size = static_cast<size_t>(8 + (id % 4) * 8);
      void* p = kv::pool_alloc::allocate(size);
      if (!p) {
        g_failures.fetch_add(1);
        return;
      }
      *static_cast<char*>(p) = static_cast<char>(id);
      kv::pool_alloc::deallocate(p, size);
    }
    kv::pool_alloc::trim_thread_cache();
  } catch (...) {
    g_failures.fetch_add(1);
  }
}

}  // namespace

int main() {
  const int kThreads = 8;
  const int kRounds = 5000;

  std::vector<std::thread> threads;
  threads.reserve(static_cast<size_t>(kThreads));
  for (int i = 0; i < kThreads; ++i) {
    threads.emplace_back(worker, i, kRounds);
  }
  for (size_t i = 0; i < threads.size(); ++i) {
    threads[i].join();
  }

  KV_CHECK(g_failures.load() == 0);
  KV_CHECK(kv::pool_alloc::arena_count() >= 2u);
  std::printf("PASS test_pool_mt (%d threads x %d rounds)\n", kThreads, kRounds);
  return 0;
}
