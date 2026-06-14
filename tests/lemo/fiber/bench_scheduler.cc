/**
 * @file bench_scheduler.cc
 * @brief lemo::fiber::Scheduler 性能压测。
 *
 *   bin/lemo/fiber/bench_scheduler
 *   bin/lemo/fiber/bench_scheduler --quick
 *   bin/lemo/fiber/bench_scheduler --tasks 200000 --threads 4,8
 */
#include "lemo/fiber/module.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::high_resolution_clock;
using Us = std::chrono::microseconds;

struct BenchConfig {
  std::vector<int> thread_counts{4, 8};
  int tasks = 100000;
  int yield_iters = 50000;
  bool quick = false;
};

struct Result {
  const char* scenario;
  int threads;
  int tasks;
  double wall_ms;
  double ops_per_s;
  double us_per_op;
  uint64_t local_pop = 0;
  uint64_t global_pop = 0;
  uint64_t steal = 0;
  uint64_t overflow = 0;
};

BenchConfig ParseConfig(int argc, char** argv) {
  BenchConfig cfg;
  if (std::getenv("LEMO_BENCH_QUICK") != nullptr) {
    cfg.quick = true;
  }
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--quick") == 0) {
      cfg.quick = true;
    } else if (std::strcmp(argv[i], "--tasks") == 0 && i + 1 < argc) {
      cfg.tasks = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--yield") == 0 && i + 1 < argc) {
      cfg.yield_iters = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
      cfg.thread_counts.clear();
      const char* p = argv[++i];
      const char* start = p;
      while (*p) {
        if (*p == ',') {
          cfg.thread_counts.push_back(std::atoi(start));
          start = p + 1;
        }
        ++p;
      }
      if (start < p) {
        cfg.thread_counts.push_back(std::atoi(start));
      }
    }
  }
  if (cfg.quick) {
    cfg.tasks = 20000;
    cfg.yield_iters = 10000;
    cfg.thread_counts = {4};
  }
  return cfg;
}

void WaitUntil(const std::atomic<int>& done, int target) {
  while (done.load(std::memory_order_acquire) < target) {
    std::this_thread::yield();
  }
}

void ParseSchedulerStats(const std::string& dump, Result& r) {
  auto find_num = [&](const char* key) -> uint64_t {
    const std::string pat = std::string(key) + "=";
    const size_t pos = dump.find(pat);
    if (pos == std::string::npos) return 0;
    return static_cast<uint64_t>(
        std::strtoull(dump.c_str() + pos + pat.size(), nullptr, 10));
  };
  r.local_pop = find_num("local_pop");
  r.global_pop = find_num("global_pop");
  r.steal = find_num("steal");
  r.overflow = find_num("overflow");
}

Result MakeResult(const char* scenario, int threads, int tasks, Us wall) {
  Result r;
  r.scenario = scenario;
  r.threads = threads;
  r.tasks = tasks;
  r.wall_ms = wall.count() / 1000.0;
  r.ops_per_s = tasks * 1e6 / wall.count();
  r.us_per_op = static_cast<double>(wall.count()) / tasks;
  return r;
}

void CollectStats(lemo::fiber::Scheduler& sch, Result& r) {
  std::ostringstream oss;
  sch.dump(oss);
  ParseSchedulerStats(oss.str(), r);
}

Result BenchEmptyCb(int threads, int tasks) {
  lemo::fiber::Scheduler sch(static_cast<size_t>(threads), false, "bench_empty_cb");
  sch.start();
  std::atomic<int> done{0};
  const auto t0 = Clock::now();
  for (int i = 0; i < tasks; ++i) {
    sch.schedule([&done]() { done.fetch_add(1, std::memory_order_relaxed); });
  }
  WaitUntil(done, tasks);
  const Us wall = std::chrono::duration_cast<Us>(Clock::now() - t0);
  Result r = MakeResult("empty_cb", threads, tasks, wall);
  CollectStats(sch, r);
  sch.stop();
  return r;
}

Result BenchMultiProducer(int threads, int tasks) {
  lemo::fiber::Scheduler sch(static_cast<size_t>(threads), false, "bench_mp");
  sch.start();
  const int producers = threads;
  const int per_producer = tasks / producers;
  const int remainder = tasks % producers;
  std::atomic<int> done{0};
  const auto t0 = Clock::now();
  std::vector<std::thread> pts;
  for (int p = 0; p < producers; ++p) {
    const int count = per_producer + (p < remainder ? 1 : 0);
    pts.emplace_back([&sch, &done, count]() {
      for (int i = 0; i < count; ++i) {
        sch.schedule([&done]() { done.fetch_add(1, std::memory_order_relaxed); });
      }
    });
  }
  for (size_t i = 0; i < pts.size(); ++i) pts[i].join();
  WaitUntil(done, tasks);
  const Us wall = std::chrono::duration_cast<Us>(Clock::now() - t0);
  Result r = MakeResult("multi_producer", threads, tasks, wall);
  CollectStats(sch, r);
  sch.stop();
  return r;
}

Result BenchYieldChain(int threads, int iters) {
  lemo::fiber::Scheduler sch(static_cast<size_t>(threads), false, "bench_yield");
  sch.start();
  std::atomic<int> done{0};
  const auto t0 = Clock::now();
  sch.schedule([&done, iters]() {
    for (int i = 0; i < iters; ++i) {
      lemo::fiber::Fiber::YieldToReady();
    }
    done.store(1, std::memory_order_release);
  });
  WaitUntil(done, 1);
  const Us wall = std::chrono::duration_cast<Us>(Clock::now() - t0);
  Result r = MakeResult("yield_chain", threads, iters, wall);
  CollectStats(sch, r);
  sch.stop();
  return r;
}

void PrintHeader() {
  std::printf(
      "%-16s %7s %10s %12s %12s %10s %10s %8s %8s %8s\n",
      "scenario", "threads", "ops", "wall_ms", "ops/s", "us/op",
      "local_pop", "global", "steal", "overflow");
}

void PrintResult(const Result& r) {
  std::printf(
      "%-16s %7d %10d %12.2f %12.0f %10.3f %10llu %8llu %8llu %8llu\n",
      r.scenario, r.threads, r.tasks, r.wall_ms, r.ops_per_s, r.us_per_op,
      static_cast<unsigned long long>(r.local_pop),
      static_cast<unsigned long long>(r.global_pop),
      static_cast<unsigned long long>(r.steal),
      static_cast<unsigned long long>(r.overflow));
}

}  // namespace

int main(int argc, char** argv) {
  const BenchConfig cfg = ParseConfig(argc, argv);
  std::printf("=== lemo::fiber::Scheduler benchmark ===\n");
  std::printf("tasks=%d yield_iters=%d\n", cfg.tasks, cfg.yield_iters);
  PrintHeader();

  for (int th : cfg.thread_counts) {
    PrintResult(BenchEmptyCb(th, cfg.tasks));
    if (th > 1) {
      PrintResult(BenchMultiProducer(th, cfg.tasks));
    }
    PrintResult(BenchYieldChain(th, cfg.yield_iters));
  }
  return 0;
}
