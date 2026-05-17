#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "alloc.h"
#include "memory/malloc_alloc.h"
#include "memory/pool.h"
#include "sgi_pool_alloc.h"

namespace {

typedef std::chrono::high_resolution_clock Clock;

struct BenchResult {
  double seconds;
  double mops;
  double vs_sgi;
};

BenchResult make_result(double seconds, int iterations, double baseline_sgi) {
  BenchResult r;
  r.seconds = seconds;
  r.mops = (iterations / 1e6) / seconds;
  r.vs_sgi = baseline_sgi > 0.0 ? seconds / baseline_sgi : 0.0;
  return r;
}

double bench_malloc(int iterations, size_t size) {
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

double bench_sgi_pool(int iterations, size_t size) {
  volatile char sink = 0;
  Clock::time_point t0 = Clock::now();
  for (int i = 0; i < iterations; ++i) {
    void* p = bench::sgi::pool_alloc::allocate(size);
    sink ^= *static_cast<char*>(p);
    bench::sgi::pool_alloc::deallocate(p, size);
  }
  Clock::time_point t1 = Clock::now();
  (void)sink;
  return std::chrono::duration<double>(t1 - t0).count();
}

double bench_lstl_pool(int iterations, size_t size) {
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

double run_median(int iterations, size_t size, double (*fn)(int, size_t), int runs) {
  std::vector<double> samples;
  samples.reserve(static_cast<size_t>(runs));
  for (int i = 0; i < runs; ++i) {
    samples.push_back(fn(iterations, size));
  }
  std::sort(samples.begin(), samples.end());
  return samples[static_cast<size_t>(runs / 2)];
}

struct CaseSummary {
  const char* label;
  size_t size;
  double sgi_mops;
  double lstl_mops;
  double lstl_vs_sgi;
};

std::vector<CaseSummary> g_summaries;

void print_row(const char* name, const BenchResult& r) {
  std::printf("  %-22s %8.4f s  %8.2f Mops/s  vs SGI二级: %5.2fx\n", name, r.seconds,
              r.mops, r.vs_sgi);
}

void bench_case(const char* label, int iterations, size_t size) {
  const int kRuns = 5;

  bench_malloc(iterations / 10, size);
  bench_sgi_pool(iterations / 10, size);
  bench_lstl_pool(iterations / 10, size);

  const double t_malloc = run_median(iterations, size, bench_malloc, kRuns);
  const double t_sgi = run_median(iterations, size, bench_sgi_pool, kRuns);
  const double t_lstl = run_median(iterations, size, bench_lstl_pool, kRuns);

  std::printf("\n[%s] size=%zuB, iter=%d, runs=%d (median)\n", label, size, iterations,
              kRuns);
  print_row("malloc/free (一级)", make_result(t_malloc, iterations, t_sgi));
  print_row("SGI pool_alloc (二级)", make_result(t_sgi, iterations, t_sgi));
  print_row("lstl::pool_alloc (二级)", make_result(t_lstl, iterations, t_sgi));

  CaseSummary summary;
  summary.label = label;
  summary.size = size;
  summary.sgi_mops = (iterations / 1e6) / t_sgi;
  summary.lstl_mops = (iterations / 1e6) / t_lstl;
  summary.lstl_vs_sgi = t_lstl / t_sgi;
  g_summaries.push_back(summary);
}

void print_summary() {
  std::printf("\n========== 二级空间配置器对比：SGI vs LSTL ==========\n");
  std::printf("%-12s %6s  %12s  %12s  %10s\n", "场景", "大小", "SGI Mops/s",
              "LSTL Mops/s", "LSTL/SGI");
  for (size_t i = 0; i < g_summaries.size(); ++i) {
    const CaseSummary& s = g_summaries[i];
    const double speedup = s.lstl_vs_sgi > 0.0 ? 1.0 / s.lstl_vs_sgi : 0.0;
    std::printf("%-12s %4zub  %12.2f  %12.2f  %9.2fx\n", s.label, s.size, s.sgi_mops,
                s.lstl_mops, speedup);
  }
  std::printf("\nLSTL/SGI > 1 表示 LSTL 更快；两者接口均为 allocate(n)/deallocate(p,n)\n");
}

}  // namespace

int main() {
  std::printf("单线程二级空间配置器对比：SGI __default_alloc vs lstl::pool_alloc\n");
  std::printf("vs SGI二级: 倍数越小越快；1.00x 表示与 SGI 二级池持平\n");

  bench_case("small_8B", 2000000, 8);
  bench_case("small_16B", 2000000, 16);
  bench_case("small_32B", 1500000, 32);
  bench_case("small_64B", 1500000, 64);
  bench_case("small_128B", 1000000, 128);
  bench_case("large_256B", 500000, 256);
  bench_case("large_1KB", 200000, 1024);
  bench_case("large_4KB", 100000, 4096);
  bench_case("huge_64KB", 20000, 65536);

  print_summary();

  std::printf("\n说明:\n");
  std::printf("  - SGI 基线: __default_alloc_template 单线程语义（16 档自由链表 + chunk）\n");
  std::printf("  - LSTL: pool_alloc（默认 LSTL_POOL_LIGHT=1，热路径对齐 SGI）\n");
  std::printf("  - 编译 -DLSTL_POOL_TRACK_SPAN=ON 启用 Span 追踪/purge（小块会慢于 SGI）\n");
  return 0;
}
