#include "lstl_test_common.h"

#include <atomic>
#include <thread>
#include <vector>

#include "alloc.h"
#include "memory/malloc_alloc.h"

namespace {

std::atomic<int> failures(0);

void worker(int id) {
  try {
    for (int round = 0; round < 200; ++round) {
      int* p = lstl::simple_alloc<int, lstl::malloc_alloc_t>::allocate(64);
      if (!p) {
        failures.fetch_add(1);
        return;
      }
      for (int i = 0; i < 64; ++i) {
        p[i] = id * 1000 + round + i;
      }
      for (int i = 0; i < 64; ++i) {
        if (p[i] != id * 1000 + round + i) {
          failures.fetch_add(1);
        }
      }
      lstl::simple_alloc<int, lstl::malloc_alloc_t>::deallocate(p, 64);
    }
  } catch (...) {
    failures.fetch_add(1);
  }
}

}  // namespace

int main() {
  const int kThreads = 8;
  std::vector<std::thread> threads;
  threads.reserve(static_cast<size_t>(kThreads));

  for (int i = 0; i < kThreads; ++i) {
    threads.push_back(std::thread(worker, i));
  }
  for (size_t i = 0; i < threads.size(); ++i) {
    threads[i].join();
  }

  LSTL_CHECK(failures.load() == 0);
  std::printf("PASS test_memory_stress (%d threads)\n", kThreads);
  return 0;
}
