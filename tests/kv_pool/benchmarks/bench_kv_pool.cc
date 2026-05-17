#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

#include "alloc.h"
#include "kv_pool.h"
#include "memory/pool.h"

namespace {

typedef std::chrono::high_resolution_clock Clock;

struct BenchRow {
  const char* label;
  size_t size;
  int threads;
  double seconds;
  double total_ops;
  double mops;
  double vs_malloc;
};

std::vector<BenchRow> g_rows;

double bench_malloc_st(int iterations, size_t size) {
  volatile char sink = 0;
  Clock::time_point t0 = Clock::now();
  for (int i = 0; i < iterations; ++i) {
    void* p = std::malloc(size);
    sink ^= *static_cast<char*>(p);
    std::free(p);
  }
  Clock::time_point t1 = Clock::now();
  (void)sink;
  return std::chrono::duration<double>(t1 - t0).count();
}

double bench_lstl_pool_st(int iterations, size_t size) {
  volatile char sink = 0;
  Clock::time_point t0 = Clock::now();
  for (int i = 0; i < iterations; ++i) {
    void* p = lstl::pool_alloc_t::allocate(size);
    sink ^= *static_cast<char*>(p);
    lstl::pool_alloc_t::deallocate(p, size);
  }
  Clock::time_point t1 = Clock::now();
  (void)sink;
  return std::chrono::duration<double>(t1 - t0).count();
}

double bench_kv_pool_st(int iterations, size_t size) {
  volatile char sink = 0;
  Clock::time_point t0 = Clock::now();
  for (int i = 0; i < iterations; ++i) {
    void* p = kv::pool_alloc::allocate(size);
    sink ^= *static_cast<char*>(p);
    kv::pool_alloc::deallocate(p, size);
  }
  Clock::time_point t1 = Clock::now();
  (void)sink;
  return std::chrono::duration<double>(t1 - t0).count();
}

struct MtShared {
  size_t size;
  int rounds;
  std::atomic<int> failures;
};

void mt_malloc_worker(MtShared* shared) {
  try {
    for (int i = 0; i < shared->rounds; ++i) {
      void* p = std::malloc(shared->size);
      if (!p) {
        shared->failures.fetch_add(1);
        return;
      }
      std::free(p);
    }
  } catch (...) {
    shared->failures.fetch_add(1);
  }
}

void mt_kv_pool_worker(MtShared* shared) {
  try {
    for (int i = 0; i < shared->rounds; ++i) {
      void* p = kv::pool_alloc::allocate(shared->size);
      if (!p) {
        shared->failures.fetch_add(1);
        return;
      }
      kv::pool_alloc::deallocate(p, shared->size);
    }
    kv::pool_alloc::trim_thread_cache();
  } catch (...) {
    shared->failures.fetch_add(1);
  }
}

double bench_mt(void (*worker)(MtShared*), int threads, int rounds, size_t size,
                MtShared* shared) {
  shared->size = size;
  shared->rounds = rounds;
  shared->failures.store(0);

  std::vector<std::thread> pool;
  pool.reserve(static_cast<size_t>(threads));

  Clock::time_point t0 = Clock::now();
  for (int i = 0; i < threads; ++i) {
    pool.emplace_back(worker, shared);
  }
  for (size_t i = 0; i < pool.size(); ++i) {
    pool[i].join();
  }
  Clock::time_point t1 = Clock::now();

  if (shared->failures.load() != 0) {
    std::printf("  [ERROR] worker failures=%d\n", shared->failures.load());
  }
  return std::chrono::duration<double>(t1 - t0).count();
}

double bench_cross_thread_kv(int items, size_t size) {
  MtShared shared;
  shared.size = size;
  shared.rounds = items;
  shared.failures.store(0);

  std::vector<void*> items_vec;
  items_vec.reserve(static_cast<size_t>(items));

  Clock::time_point t0 = Clock::now();

  std::thread worker([&]() {
    for (int i = 0; i < items; ++i) {
      void* p = kv::pool_alloc::allocate(size);
      if (!p) {
        shared.failures.fetch_add(1);
        return;
      }
      items_vec.push_back(p);
    }
    kv::pool_alloc::trim_thread_cache();
  });
  worker.join();

  for (size_t i = 0; i < items_vec.size(); ++i) {
    kv::pool_alloc::deallocate(items_vec[i], size);
  }
  kv::pool_alloc::trim_thread_cache();

  Clock::time_point t1 = Clock::now();
  return std::chrono::duration<double>(t1 - t0).count();
}

double run_median_st(double (*fn)(int, size_t), int iterations, size_t size, int runs) {
  std::vector<double> samples;
  samples.reserve(static_cast<size_t>(runs));
  fn(iterations / 8, size);
  for (int i = 0; i < runs; ++i) {
    samples.push_back(fn(iterations, size));
  }
  std::sort(samples.begin(), samples.end());
  return samples[static_cast<size_t>(runs / 2)];
}

void record(const char* label, size_t size, int threads, double seconds, double total_ops,
            double baseline_seconds) {
  BenchRow row;
  row.label = label;
  row.size = size;
  row.threads = threads;
  row.seconds = seconds;
  row.total_ops = total_ops;
  row.mops = (total_ops / 1e6) / seconds;
  row.vs_malloc = baseline_seconds > 0.0 ? seconds / baseline_seconds : 1.0;
  g_rows.push_back(row);
}

void print_row(const BenchRow& r) {
  std::printf("  %-22s %6.4f s  %10.0f ops  %8.2f Mops/s  vs malloc: %5.2fx\n", r.label,
              r.seconds, r.total_ops, r.mops, r.vs_malloc);
}

void bench_single_thread_cases() {
  const int kRuns = 5;
  const size_t sizes[] = {8, 16, 32, 64, 128};
  const int iters[] = {2000000, 2000000, 1500000, 1500000, 1000000};

  std::printf("\n=== 单线程 alloc/free（每线程独立，median x%d）===\n", kRuns);
  for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
    const size_t sz = sizes[i];
    const int iter = iters[i];
    const double t_malloc = run_median_st(bench_malloc_st, iter, sz, kRuns);
    const double t_lstl = run_median_st(bench_lstl_pool_st, iter, sz, kRuns);
    const double t_kv = run_median_st(bench_kv_pool_st, iter, sz, kRuns);

    std::printf("\n[size=%zub iter=%d]\n", sz, iter);
    record("malloc", sz, 1, t_malloc, iter, t_malloc);
    record("lstl::pool (单线程)", sz, 1, t_lstl, iter, t_malloc);
    record("kv::pool (多线程池ST)", sz, 1, t_kv, iter, t_malloc);
    print_row(g_rows[g_rows.size() - 3]);
    print_row(g_rows[g_rows.size() - 2]);
    print_row(g_rows[g_rows.size() - 1]);
  }
}

void bench_multi_thread_cases() {
  const int kThreads = 8;
  const int kRounds = 200000;
  const size_t sizes[] = {16, 32, 64};
  MtShared shared;

  std::printf("\n=== 多线程同线程 alloc/free（%d 线程 x %d 轮，总 ops=%d/档）===\n",
              kThreads, kRounds, kThreads * kRounds);

  for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
    const size_t sz = sizes[i];
    const double total_ops = static_cast<double>(kThreads * kRounds);

    bench_mt(mt_malloc_worker, kThreads, kRounds / 4, sz, &shared);
    const double t_malloc = bench_mt(mt_malloc_worker, kThreads, kRounds, sz, &shared);
    const double t_kv = bench_mt(mt_kv_pool_worker, kThreads, kRounds, sz, &shared);

    std::printf("\n[size=%zub]\n", sz);
    record("malloc", sz, kThreads, t_malloc, total_ops, t_malloc);
    record("kv::pool", sz, kThreads, t_kv, total_ops, t_malloc);
    print_row(g_rows[g_rows.size() - 2]);
    print_row(g_rows[g_rows.size() - 1]);
    std::printf("    kv/malloc 吞吐比: %.2fx\n", (total_ops / t_kv) / (total_ops / t_malloc));
  }
}

void bench_cross_thread_case() {
  const int kItems = 500000;
  const size_t kSize = 32;

  std::printf("\n=== Redis 模式：worker alloc + 主线程 free（kv::pool）===\n");
  const double t_cross = bench_cross_thread_kv(kItems, kSize);
  const double mops = (static_cast<double>(kItems) / 1e6) / t_cross;
  std::printf("  items=%d size=%zub time=%.4f s  %.2f Mops/s  remote_enqueues=%llu\n",
              kItems, kSize, t_cross, mops,
              static_cast<unsigned long long>(kv::pool_alloc::remote_enqueue_count()));
}

void print_summary() {
  std::printf("\n========== 汇总 ==========\n");
  std::printf("%-22s %4s %3s  %10s  %8s  %8s\n", "场景", "大小", "线程", "Mops/s",
              "总耗时", "vs malloc");
  for (size_t i = 0; i < g_rows.size(); ++i) {
    const BenchRow& r = g_rows[i];
    const double speedup = r.vs_malloc > 0.0 ? 1.0 / r.vs_malloc : 0.0;
    std::printf("%-22s %3zub %2d  %10.2f  %7.4fs  %6.2fx\n", r.label, r.size, r.threads,
                r.mops, r.seconds, speedup);
  }
  std::printf("\nvs malloc > 1 表示更快；多线程结果为 8 线程聚合吞吐\n");
}

}  // namespace

int main() {
  std::printf("KVStore 内存池性能压测 (Release)\n");
  std::printf("lstl::pool = 单线程 LIGHT 池；kv::pool = 多线程 TCache+Arena+MPSC\n");

  bench_single_thread_cases();
  bench_multi_thread_cases();
  bench_cross_thread_case();
  print_summary();
  return 0;
}
