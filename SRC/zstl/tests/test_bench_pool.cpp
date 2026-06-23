// ============================================================================
// zstl memory pool performance benchmarks
// ============================================================================
// Benchmarks: QPS (alloc+free/sec), throughput (MB/s), latency percentiles,
//            single-threaded and multi-threaded scalability, pool vs new/delete
//            comparison, mixed-size workloads, sustained stress tests.
// ============================================================================

#include <gtest/gtest.h>
#include "zstl/zstl.h"

#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstring>
#include <random>

using namespace zstl;
using namespace std::chrono;

// ============================================================================
// Helper: format large numbers with commas (approximate — K/M/B suffixes)
// ============================================================================
static std::string fmt_qps(double qps) {
    std::ostringstream oss;
    if (qps >= 1e9) {
        oss << std::fixed << std::setprecision(2) << (qps / 1e9) << "G";
    } else if (qps >= 1e6) {
        oss << std::fixed << std::setprecision(2) << (qps / 1e6) << "M";
    } else if (qps >= 1e3) {
        oss << std::fixed << std::setprecision(2) << (qps / 1e3) << "K";
    } else {
        oss << std::fixed << std::setprecision(0) << qps;
    }
    return oss.str();
}

// ============================================================================
// Helper: run a multi-threaded pool benchmark
// ============================================================================
static void run_threaded_pool(int nthreads, int per_thread, const std::string& label) {
    std::atomic<bool> start{false};
    std::atomic<long long> ops{0};
    std::vector<std::thread> threads;

    auto t1 = high_resolution_clock::now();
    for (int t = 0; t < nthreads; ++t) {
        threads.emplace_back([&]() {
            // Warm up this thread's tcache
            for (int w = 0; w < 100; ++w) {
                void* p = pool_malloc(64);
                pool_free(p, 64);
            }
            while (!start.load(std::memory_order_acquire)) {
                // spin-wait
            }
            for (int i = 0; i < per_thread; ++i) {
                size_t sz = ((i % 50) + 1) * 16;  // 16..800 bytes
                void* p = pool_malloc(sz);
                pool_free(p, sz);
                ops.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // Synchronize thread start as closely as possible
    start.store(true, std::memory_order_release);
    for (auto& t : threads) t.join();
    auto t2 = high_resolution_clock::now();

    auto us = duration_cast<microseconds>(t2 - t1).count();
    double qps = (double)ops.load() / (us / 1000000.0);
    std::cout << "[BENCH] Pool " << label << " (" << nthreads << "T x "
              << per_thread << " ops): " << std::fixed << std::setprecision(0)
              << qps << " ops/sec (" << ops.load() << " ops in "
              << us / 1000.0 << " ms)" << std::endl;
}

// ============================================================================
// Single-thread QPS benchmarks by allocation size
// ============================================================================

TEST(BenchPool, AllocFreeQPS_Small_64B) {
    const int N = 5000000;
    std::vector<void*> ptrs(N);

    auto t1 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        ptrs[i] = pool_malloc(64);
    }
    auto t2 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        pool_free(ptrs[i], 64);
    }
    auto t3 = high_resolution_clock::now();

    auto alloc_us = duration_cast<microseconds>(t2 - t1).count();
    auto free_us  = duration_cast<microseconds>(t3 - t2).count();
    double alloc_qps = (double)N / (alloc_us / 1000000.0);
    double free_qps  = (double)N / (free_us / 1000000.0);

    std::cout << "[BENCH] Pool Alloc QPS (64B):  " << fmt_qps(alloc_qps) << " allocs/sec ("
              << alloc_us / 1000.0 << " ms)" << std::endl;
    std::cout << "[BENCH] Pool Free  QPS (64B):  " << fmt_qps(free_qps)  << " frees/sec  ("
              << free_us / 1000.0 << " ms)" << std::endl;
    EXPECT_GT(alloc_qps, 0);
    EXPECT_GT(free_qps, 0);
}

TEST(BenchPool, AllocFreeQPS_Medium_512B) {
    const int N = 2000000;
    std::vector<void*> ptrs(N);

    auto t1 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) ptrs[i] = pool_malloc(512);
    auto t2 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) pool_free(ptrs[i], 512);
    auto t3 = high_resolution_clock::now();

    auto alloc_us = duration_cast<microseconds>(t2 - t1).count();
    auto free_us  = duration_cast<microseconds>(t3 - t2).count();
    double alloc_qps = (double)N / (alloc_us / 1000000.0);
    double free_qps  = (double)N / (free_us / 1000000.0);

    std::cout << "[BENCH] Pool Alloc QPS (512B): " << fmt_qps(alloc_qps) << " allocs/sec" << std::endl;
    std::cout << "[BENCH] Pool Free  QPS (512B): " << fmt_qps(free_qps)  << " frees/sec" << std::endl;
    EXPECT_GT(alloc_qps, 0);
}

TEST(BenchPool, AllocFreeQPS_Large_4KB) {
    const int N = 1000000;
    std::vector<void*> ptrs(N);

    auto t1 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) ptrs[i] = pool_malloc(4096);
    auto t2 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) pool_free(ptrs[i], 4096);
    auto t3 = high_resolution_clock::now();

    auto alloc_us = duration_cast<microseconds>(t2 - t1).count();
    auto free_us  = duration_cast<microseconds>(t3 - t2).count();
    double alloc_qps = (double)N / (alloc_us / 1000000.0);
    double free_qps  = (double)N / (free_us / 1000000.0);

    std::cout << "[BENCH] Pool Alloc QPS (4KB):  " << fmt_qps(alloc_qps) << " allocs/sec" << std::endl;
    std::cout << "[BENCH] Pool Free  QPS (4KB):  " << fmt_qps(free_qps)  << " frees/sec" << std::endl;
    EXPECT_GT(alloc_qps, 0);
}

TEST(BenchPool, AllocFreeQPS_Oversized_64KB) {
    // Oversized allocations (> kMaxPoolSize) fallback to ::operator new
    const int N = 100000;
    std::vector<void*> ptrs(N);

    auto t1 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) ptrs[i] = pool_malloc(65536);
    auto t2 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) pool_free(ptrs[i], 65536);
    auto t3 = high_resolution_clock::now();

    auto alloc_us = duration_cast<microseconds>(t2 - t1).count();
    auto free_us  = duration_cast<microseconds>(t3 - t2).count();
    double alloc_qps = (double)N / (alloc_us / 1000000.0);
    double free_qps  = (double)N / (free_us / 1000000.0);

    std::cout << "[BENCH] Pool Alloc QPS (64KB oversize): " << fmt_qps(alloc_qps) << " allocs/sec" << std::endl;
    std::cout << "[BENCH] Pool Free  QPS (64KB oversize): " << fmt_qps(free_qps)  << " frees/sec" << std::endl;
    EXPECT_GT(alloc_qps, 0);
}

// ============================================================================
// Pool vs new/delete head-to-head comparison
// ============================================================================

TEST(BenchPool, PoolVsNewDelete_Small_64B) {
    const int N = 5000000;

    // Pool: alloc + free
    auto t1 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        void* p = pool_malloc(64);
        pool_free(p, 64);
    }
    auto t2 = high_resolution_clock::now();

    // new/delete: alloc + free
    auto t3 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        char* p = new char[64];
        delete[] p;
    }
    auto t4 = high_resolution_clock::now();

    auto pool_ms = duration_cast<milliseconds>(t2 - t1).count();
    auto new_ms  = duration_cast<milliseconds>(t4 - t3).count();
    double pool_qps = (double)N / (pool_ms / 1000.0);
    double new_qps  = (double)N / (new_ms / 1000.0);

    std::cout << "[BENCH] Pool 64B alloc+free:  " << fmt_qps(pool_qps)
              << " ops/sec (" << pool_ms << " ms)" << std::endl;
    std::cout << "[BENCH] new  64B alloc+free:  " << fmt_qps(new_qps)
              << " ops/sec (" << new_ms << " ms)" << std::endl;
    std::cout << "[BENCH] Pool vs new speedup:  "
              << std::setprecision(1) << (double)new_ms / pool_ms << "x" << std::endl;
    EXPECT_GE(pool_qps, new_qps * 0.5);  // Pool should be at least competitive
}

TEST(BenchPool, PoolVsNewDelete_Medium_512B) {
    const int N = 2000000;

    auto t1 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        void* p = pool_malloc(512);
        pool_free(p, 512);
    }
    auto t2 = high_resolution_clock::now();

    auto t3 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        char* p = new char[512];
        delete[] p;
    }
    auto t4 = high_resolution_clock::now();

    auto pool_ms = duration_cast<milliseconds>(t2 - t1).count();
    auto new_ms  = duration_cast<milliseconds>(t4 - t3).count();
    double pool_qps = (double)N / (pool_ms / 1000.0);
    double new_qps  = (double)N / (new_ms / 1000.0);

    std::cout << "[BENCH] Pool 512B alloc+free: " << fmt_qps(pool_qps)
              << " ops/sec (" << pool_ms << " ms)" << std::endl;
    std::cout << "[BENCH] new  512B alloc+free: " << fmt_qps(new_qps)
              << " ops/sec (" << new_ms << " ms)" << std::endl;
    std::cout << "[BENCH] Pool vs new speedup:   "
              << std::setprecision(1) << (double)new_ms / pool_ms << "x" << std::endl;
    EXPECT_GE(pool_qps, new_qps * 0.5);
}

TEST(BenchPool, PoolVsNewDelete_Large_4KB) {
    const int N = 1000000;

    auto t1 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        void* p = pool_malloc(4096);
        pool_free(p, 4096);
    }
    auto t2 = high_resolution_clock::now();

    auto t3 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        char* p = new char[4096];
        delete[] p;
    }
    auto t4 = high_resolution_clock::now();

    auto pool_ms = duration_cast<milliseconds>(t2 - t1).count();
    auto new_ms  = duration_cast<milliseconds>(t4 - t3).count();
    double pool_qps = (double)N / (pool_ms / 1000.0);
    double new_qps  = (double)N / (new_ms / 1000.0);

    std::cout << "[BENCH] Pool 4KB alloc+free:  " << fmt_qps(pool_qps)
              << " ops/sec (" << pool_ms << " ms)" << std::endl;
    std::cout << "[BENCH] new  4KB alloc+free:  " << fmt_qps(new_qps)
              << " ops/sec (" << new_ms << " ms)" << std::endl;
    std::cout << "[BENCH] Pool vs new speedup:   "
              << std::setprecision(1) << (double)new_ms / pool_ms << "x" << std::endl;
    EXPECT_GE(pool_qps, new_qps * 0.5);
}

// ============================================================================
// Throughput in MB/s by allocation size
// ============================================================================

TEST(BenchPool, Throughput_MBps_BySize) {
    struct TestCase {
        size_t sz;
        int    count;
    };
    TestCase cases[] = {
        {16,    5000000},
        {32,    5000000},
        {64,    5000000},
        {128,   3000000},
        {256,   2000000},
        {512,   2000000},
        {1024,  1000000},
        {2048,  1000000},
        {4096,   500000},
        {8192,   250000},
    };

    std::cout << "\n[BENCH] Pool Throughput by Allocation Size:" << std::endl;
    std::cout << "  " << std::setw(8) << "Size" << std::setw(14) << "QPS"
              << std::setw(14) << "MB/s" << std::endl;
    std::cout << "  " << std::string(36, '-') << std::endl;

    for (auto& tc : cases) {
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < tc.count; ++i) {
            void* p = pool_malloc(tc.sz);
            pool_free(p, tc.sz);
        }
        auto t2 = high_resolution_clock::now();

        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)tc.count / (us / 1000000.0);
        double mbps = (qps * tc.sz) / (1024.0 * 1024.0);

        std::cout << "  " << std::setw(6) << tc.sz << "B " << std::setw(12)
                  << std::fixed << std::setprecision(0) << qps << " "
                  << std::setw(11) << std::setprecision(1) << mbps << std::endl;
    }
}

// ============================================================================
// Mixed-size allocation patterns (simulates real workloads)
// ============================================================================

TEST(BenchPool, MixedSizeAllocation_QPS) {
    const int N = 200000;
    size_t sizes[] = {16, 16, 32, 32, 64, 64, 128, 256, 256, 512,
                      64, 64, 32, 1024, 64, 128, 2048, 64, 4096, 64};
    const int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    std::vector<std::pair<void*, size_t>> ptrs;
    ptrs.reserve(N);

    auto t1 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        size_t sz = sizes[i % num_sizes];
        void* p = pool_malloc(sz);
        ptrs.push_back({p, sz});
    }
    for (auto& kv : ptrs) pool_free(kv.first, kv.second);
    auto t2 = high_resolution_clock::now();

    auto us = duration_cast<microseconds>(t2 - t1).count();
    double qps = (double)N / (us / 1000000.0);
    std::cout << "[BENCH] Pool MixedSize QPS:     " << fmt_qps(qps)
              << " ops/sec (" << N << " ops in " << us / 1000.0 << " ms)" << std::endl;
    EXPECT_GT(qps, 0);
}

TEST(BenchPool, RandomSizeAllocation_QPS) {
    const int N = 500000;
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(1, 128);  // 16..2048 range (size classes)

    std::vector<std::pair<void*, size_t>> ptrs;
    ptrs.reserve(N);

    auto t1 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        size_t sz = static_cast<size_t>(dist(rng)) * 16;
        void* p = pool_malloc(sz);
        ptrs.push_back({p, sz});
    }
    // Free in random order for realism
    std::shuffle(ptrs.begin(), ptrs.end(), rng);
    for (auto& kv : ptrs) pool_free(kv.first, kv.second);
    auto t2 = high_resolution_clock::now();

    auto us = duration_cast<microseconds>(t2 - t1).count();
    double qps = (double)N / (us / 1000000.0);
    std::cout << "[BENCH] Pool RandomSize QPS:    " << fmt_qps(qps)
              << " ops/sec (random free order)" << std::endl;
    EXPECT_GT(qps, 0);
}

// ============================================================================
// Batch allocation benchmarks
// ============================================================================

TEST(BenchPool, BatchAllocateFree_10K) {
    const int BATCH = 10000;
    const int CYCLES = 1000;

    auto t1 = high_resolution_clock::now();
    for (int c = 0; c < CYCLES; ++c) {
        std::vector<void*> batch;
        batch.reserve(BATCH);
        for (int i = 0; i < BATCH; ++i) batch.push_back(pool_malloc(64));
        for (void* p : batch) pool_free(p, 64);
    }
    auto t2 = high_resolution_clock::now();

    auto us = duration_cast<microseconds>(t2 - t1).count();
    long long total = 1LL * BATCH * CYCLES;
    double qps = (double)total / (us / 1000000.0);
    std::cout << "[BENCH] Pool Batch (10K x 1K):  " << total << " ops in "
              << us / 1000.0 << " ms, QPS=" << fmt_qps(qps) << std::endl;
    EXPECT_GT(qps, 0);
}

TEST(BenchPool, BurstyAllocations) {
    // Simulate bursty allocation: allocate 1K, free, repeat 5K times
    const int BURST = 1000;
    const int CYCLES = 5000;

    auto t1 = high_resolution_clock::now();
    for (int c = 0; c < CYCLES; ++c) {
        void* buf[BURST];
        for (int i = 0; i < BURST; ++i) buf[i] = pool_malloc(128);
        for (int i = 0; i < BURST; ++i) pool_free(buf[i], 128);
    }
    auto t2 = high_resolution_clock::now();

    auto us = duration_cast<microseconds>(t2 - t1).count();
    long long total = 1LL * BURST * CYCLES;
    double qps = (double)total / (us / 1000000.0);
    std::cout << "[BENCH] Pool Bursty (1K x 5K):  " << total << " ops in "
              << us / 1000.0 << " ms, QPS=" << fmt_qps(qps) << std::endl;
    EXPECT_GT(qps, 0);
}

// ============================================================================
// Throughput comparison: pool vs raw new/delete in MB/s
// ============================================================================

TEST(BenchPool, Throughput_MBps_PoolVsNew) {
    const int N = 2000000;
    const size_t SZ = 256;

    // Pool
    auto t1 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        void* p = pool_malloc(SZ);
        pool_free(p, SZ);
    }
    auto t2 = high_resolution_clock::now();

    // new/delete
    auto t3 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        char* p = new char[SZ];
        delete[] p;
    }
    auto t4 = high_resolution_clock::now();

    auto pool_us = duration_cast<microseconds>(t2 - t1).count();
    auto new_us  = duration_cast<microseconds>(t4 - t3).count();

    double pool_mbps = ((double)N * SZ) / (pool_us / 1000000.0) / (1024.0 * 1024.0);
    double new_mbps  = ((double)N * SZ) / (new_us / 1000000.0) / (1024.0 * 1024.0);

    std::cout << "[BENCH] Pool Throughput (256B): " << std::fixed << std::setprecision(1)
              << pool_mbps << " MB/s" << std::endl;
    std::cout << "[BENCH] new  Throughput (256B): " << new_mbps << " MB/s" << std::endl;
    std::cout << "[BENCH] Throughput speedup:      " << std::setprecision(1)
              << pool_mbps / new_mbps << "x" << std::endl;
    EXPECT_GT(pool_mbps, 0);
}

// ============================================================================
// Multi-threaded QPS benchmarks
// ============================================================================

TEST(BenchPool, MultiThreadQPS_2threads) {
    run_threaded_pool(2, 500000, "2-Thread");
}

TEST(BenchPool, MultiThreadQPS_4threads) {
    run_threaded_pool(4, 500000, "4-Thread");
}

TEST(BenchPool, MultiThreadQPS_8threads) {
    run_threaded_pool(8, 500000, "8-Thread");
}

TEST(BenchPool, MultiThreadQPS_16threads) {
    run_threaded_pool(16, 500000, "16-Thread");
}

TEST(BenchPool, MultiThreadQPS_32threads) {
    run_threaded_pool(32, 500000, "32-Thread");
}

// ============================================================================
// Thread scaling efficiency
// ============================================================================

TEST(BenchPool, ThreadScalingEfficiency) {
    std::vector<std::pair<int, double>> results;  // (nthreads, qps)
    int per_thread = 200000;

    std::cout << "\n[BENCH] Pool Thread Scaling Efficiency:" << std::endl;
    std::cout << "  " << std::setw(8) << "Threads" << std::setw(14) << "QPS"
              << std::setw(14) << "Efficiency" << std::endl;
    std::cout << "  " << std::string(36, '-') << std::endl;

    for (int n : {1, 2, 4, 8, 16, 32}) {
        std::atomic<bool> start{false};
        std::atomic<long long> ops{0};
        std::vector<std::thread> threads;

        auto t1 = high_resolution_clock::now();
        for (int t = 0; t < n; ++t) {
            threads.emplace_back([&]() {
                for (int w = 0; w < 50; ++w) {
                    void* p = pool_malloc(64);
                    pool_free(p, 64);
                }
                while (!start.load(std::memory_order_acquire)) {}
                for (int i = 0; i < per_thread; ++i) {
                    size_t sz = ((i % 50) + 1) * 16;
                    void* p = pool_malloc(sz);
                    pool_free(p, sz);
                    ops.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        start.store(true, std::memory_order_release);
        for (auto& t : threads) t.join();
        auto t2 = high_resolution_clock::now();

        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)ops.load() / (us / 1000000.0);
        results.push_back({n, qps});
    }

    double single_qps = results[0].second;
    for (auto& [n, qps] : results) {
        double ideal = single_qps * n;
        double efficiency = (qps / ideal) * 100.0;
        std::cout << "  " << std::setw(6) << n << "T " << std::setw(12)
                  << std::fixed << std::setprecision(0) << qps << " "
                  << std::setw(11) << std::setprecision(1) << efficiency << "%" << std::endl;
    }
}

// ============================================================================
// Latency percentiles (p50, p75, p90, p99, p999, min, max)
// ============================================================================

TEST(BenchPool, LatencyPercentiles_64B) {
    const int N = 100000;
    std::vector<long long> latencies;
    latencies.reserve(N);

    for (int i = 0; i < N; ++i) {
        auto t1 = high_resolution_clock::now();
        void* p = pool_malloc(64);
        pool_free(p, 64);
        auto t2 = high_resolution_clock::now();
        latencies.push_back(duration_cast<nanoseconds>(t2 - t1).count());
    }

    std::sort(latencies.begin(), latencies.end());
    long long p50  = latencies[N * 50 / 100];
    long long p75  = latencies[N * 75 / 100];
    long long p90  = latencies[N * 90 / 100];
    long long p99  = latencies[N * 99 / 100];
    long long p999 = latencies[N * 999 / 1000];
    long long mn   = latencies[0];
    long long mx   = latencies.back();

    // Compute mean
    long long sum = 0;
    for (auto l : latencies) sum += l;
    long long mean = sum / N;

    std::cout << "[BENCH] Pool Latency (64B) N=" << N << ":" << std::endl;
    std::cout << "  min="   << mn/1000.0   << " us ";
    std::cout << "p50="   << p50/1000.0  << " us ";
    std::cout << "p75="   << p75/1000.0  << " us ";
    std::cout << "p90="   << p90/1000.0  << " us ";
    std::cout << "p99="   << p99/1000.0  << " us ";
    std::cout << "p999="  << p999/1000.0 << " us ";
    std::cout << "max="   << mx/1000.0   << " us";
    std::cout << "  mean=" << mean/1000.0 << " us" << std::endl;
    EXPECT_GT(p50, 0);
}

TEST(BenchPool, LatencyPercentiles_512B) {
    const int N = 100000;
    std::vector<long long> latencies;
    latencies.reserve(N);

    for (int i = 0; i < N; ++i) {
        auto t1 = high_resolution_clock::now();
        void* p = pool_malloc(512);
        pool_free(p, 512);
        auto t2 = high_resolution_clock::now();
        latencies.push_back(duration_cast<nanoseconds>(t2 - t1).count());
    }

    std::sort(latencies.begin(), latencies.end());
    long long p50 = latencies[N * 50 / 100];
    long long p99 = latencies[N * 99 / 100];
    long long p999 = latencies[N * 999 / 1000];
    long long mn = latencies[0];
    long long mx = latencies.back();

    long long sum = 0;
    for (auto l : latencies) sum += l;
    long long mean = sum / N;

    std::cout << "[BENCH] Pool Latency (512B) N=" << N << ":" << std::endl;
    std::cout << "  min=" << mn/1000.0 << " us "
              << "p50=" << p50/1000.0 << " us "
              << "p99=" << p99/1000.0 << " us "
              << "p999=" << p999/1000.0 << " us "
              << "max=" << mx/1000.0 << " us "
              << "mean=" << mean/1000.0 << " us" << std::endl;
    EXPECT_GT(p50, 0);
}

TEST(BenchPool, LatencyPercentiles_4096B) {
    const int N = 50000;
    std::vector<long long> latencies;
    latencies.reserve(N);

    for (int i = 0; i < N; ++i) {
        auto t1 = high_resolution_clock::now();
        void* p = pool_malloc(4096);
        pool_free(p, 4096);
        auto t2 = high_resolution_clock::now();
        latencies.push_back(duration_cast<nanoseconds>(t2 - t1).count());
    }

    std::sort(latencies.begin(), latencies.end());
    long long p50 = latencies[N * 50 / 100];
    long long p99 = latencies[N * 99 / 100];
    long long p999 = latencies[N * 999 / 1000];
    long long mn = latencies[0];
    long long mx = latencies.back();

    long long sum = 0;
    for (auto l : latencies) sum += l;
    long long mean = sum / N;

    std::cout << "[BENCH] Pool Latency (4096B) N=" << N << ":" << std::endl;
    std::cout << "  min=" << mn/1000.0 << " us "
              << "p50=" << p50/1000.0 << " us "
              << "p99=" << p99/1000.0 << " us "
              << "p999=" << p999/1000.0 << " us "
              << "max=" << mx/1000.0 << " us "
              << "mean=" << mean/1000.0 << " us" << std::endl;
    EXPECT_GT(p50, 0);
}

// ============================================================================
// Latency under contention (multi-threaded)
// ============================================================================

TEST(BenchPool, LatencyUnderContention) {
    const int N = 20000;
    std::atomic<bool> start{false};
    std::vector<std::vector<long long>> all_latencies(4);

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t]() {
            while (!start.load(std::memory_order_acquire)) {}
            all_latencies[t].reserve(N);
            for (int i = 0; i < N; ++i) {
                auto t1 = high_resolution_clock::now();
                void* p = pool_malloc(64);
                pool_free(p, 64);
                auto t2 = high_resolution_clock::now();
                all_latencies[t].push_back(duration_cast<nanoseconds>(t2 - t1).count());
            }
        });
    }

    start.store(true, std::memory_order_release);
    for (auto& t : threads) t.join();

    // Aggregate latencies from all threads
    std::vector<long long> all;
    for (auto& lat : all_latencies) all.insert(all.end(), lat.begin(), lat.end());
    std::sort(all.begin(), all.end());

    size_t total = all.size();
    std::cout << "[BENCH] Pool Latency (4T contended, 64B) N=" << total << ":" << std::endl;
    std::cout << "  p50="  << all[total*50/100]/1000.0  << " us "
              << "p90="  << all[total*90/100]/1000.0  << " us "
              << "p99="  << all[total*99/100]/1000.0  << " us "
              << "p999=" << all[total*999/1000]/1000.0 << " us "
              << "max="  << all.back()/1000.0 << " us" << std::endl;
}

// ============================================================================
// pool_realloc benchmark
// ============================================================================

TEST(BenchPool, ReallocQPS) {
    const int N = 500000;

    auto t1 = high_resolution_clock::now();
    void* p = pool_malloc(64);
    for (int i = 0; i < N; ++i) {
        // Realloc between two size classes in same class
        p = pool_realloc(p, (i % 2 == 0) ? 64 : 80, (i % 2 == 0) ? 80 : 64);
    }
    pool_free(p, (N % 2 == 0) ? 64 : 80);
    auto t2 = high_resolution_clock::now();

    auto us = duration_cast<microseconds>(t2 - t1).count();
    double qps = (double)N / (us / 1000000.0);
    std::cout << "[BENCH] Pool Realloc QPS:        " << fmt_qps(qps)
              << " reallocs/sec (same size class)" << std::endl;
    EXPECT_GT(qps, 0);
}

TEST(BenchPool, ReallocCrossClassQPS) {
    const int N = 200000;

    auto t1 = high_resolution_clock::now();
    void* p = pool_malloc(64);
    for (int i = 0; i < N; ++i) {
        // Cross size class: 64 <-> 512
        p = pool_realloc(p, (i % 2 == 0) ? 64 : 512, (i % 2 == 0) ? 512 : 64);
    }
    pool_free(p, (N % 2 == 0) ? 64 : 512);
    auto t2 = high_resolution_clock::now();

    auto us = duration_cast<microseconds>(t2 - t1).count();
    double qps = (double)N / (us / 1000000.0);
    std::cout << "[BENCH] Pool Realloc CrossClass:  " << fmt_qps(qps)
              << " reallocs/sec (64<->512)" << std::endl;
    EXPECT_GT(qps, 0);
}

// ============================================================================
// Sustained stress test: repeated allocations with gradual memory pressure
// ============================================================================

TEST(BenchPool, SustainedStress_10M) {
    const int TOTAL = 10000000;
    const int CHUNK = 10000;
    std::vector<void*> held;
    held.reserve(TOTAL / 10);

    auto t1 = high_resolution_clock::now();
    for (int i = 0; i < TOTAL; ++i) {
        void* p = pool_malloc((i % 32 + 1) * 32);  // 32..1024 bytes
        if (i % 10 == 0) {
            held.push_back(p);  // hold 10% to create memory pressure
        } else {
            pool_free(p, (i % 32 + 1) * 32);
        }
    }
    // Free held allocations
    for (size_t i = 0; i < held.size(); ++i) {
        pool_free(held[i], (((int)i * 10) % 32 + 1) * 32);
    }
    auto t2 = high_resolution_clock::now();

    auto us = duration_cast<microseconds>(t2 - t1).count();
    double qps = (double)TOTAL / (us / 1000000.0);
    std::cout << "[BENCH] Pool SustainedStress 10M: " << fmt_qps(qps)
              << " ops/sec (" << TOTAL << " ops in " << us / 1000000.0 << " s)" << std::endl;
    EXPECT_GT(qps, 0);
}

// ============================================================================
// Sequential access pattern (first-fit, stack-like alloc/free)
// ============================================================================

TEST(BenchPool, StackLikePattern) {
    const int DEPTH = 100000;
    const int CYCLES = 100;

    auto t1 = high_resolution_clock::now();
    for (int c = 0; c < CYCLES; ++c) {
        std::vector<void*> stack;
        stack.reserve(DEPTH);
        for (int i = 0; i < DEPTH; ++i) {
            stack.push_back(pool_malloc(64));
        }
        // Free in reverse order (LIFO)
        while (!stack.empty()) {
            pool_free(stack.back(), 64);
            stack.pop_back();
        }
    }
    auto t2 = high_resolution_clock::now();

    auto us = duration_cast<microseconds>(t2 - t1).count();
    long long total = 1LL * DEPTH * CYCLES * 2;  // alloc + free
    double qps = (double)total / (us / 1000000.0);
    std::cout << "[BENCH] Pool StackLike (100K x 100): " << total << " ops in "
              << us / 1000000.0 << " s, QPS=" << fmt_qps(qps) << std::endl;
    EXPECT_GT(qps, 0);
}

// ============================================================================
// FIFO pattern (queue-like: alloc at back, free at front)
// ============================================================================

TEST(BenchPool, QueueLikePattern) {
    const int N = 2000000;

    auto t1 = high_resolution_clock::now();
    std::vector<void*> queue;
    queue.reserve(256);
    for (int i = 0; i < N; ++i) {
        queue.push_back(pool_malloc(128));
        if (queue.size() >= 256) {
            pool_free(queue.front(), 128);
            queue.erase(queue.begin());
        }
    }
    // Drain
    for (void* p : queue) pool_free(p, 128);
    auto t2 = high_resolution_clock::now();

    auto us = duration_cast<microseconds>(t2 - t1).count();
    double qps = (double)N / (us / 1000000.0);
    std::cout << "[BENCH] Pool QueueLike (256 depth): " << fmt_qps(qps)
              << " ops/sec" << std::endl;
    EXPECT_GT(qps, 0);
}

// ============================================================================
// pool_malloc_class / pool_free_class (known size class) benchmarks
// ============================================================================

TEST(BenchPool, KnownClassQPS_Small) {
    const int N = 5000000;
    // 64 bytes -> find the size class index
    size_t idx = size_class_index(64);

    auto t1 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        void* p = pool_malloc_class(idx);
        pool_free_class(p, idx);
    }
    auto t2 = high_resolution_clock::now();

    auto us = duration_cast<microseconds>(t2 - t1).count();
    double qps = (double)N / (us / 1000000.0);
    std::cout << "[BENCH] Pool KnownClass (64B):     " << fmt_qps(qps)
              << " ops/sec (bypass size_class_index)" << std::endl;
    EXPECT_GT(qps, 0);
}

// ============================================================================
// Comparison: pool_malloc vs pool_malloc_class (overhead of size_class_index)
// ============================================================================

TEST(BenchPool, IndexLookupOverhead) {
    const int N = 5000000;

    // Without index lookup
    size_t idx = size_class_index(64);
    auto t1 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        void* p = pool_malloc_class(idx);
        pool_free_class(p, idx);
    }
    auto t2 = high_resolution_clock::now();

    // With index lookup
    auto t3 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        void* p = pool_malloc(64);
        pool_free(p, 64);
    }
    auto t4 = high_resolution_clock::now();

    auto direct_us = duration_cast<microseconds>(t2 - t1).count();
    auto lookup_us = duration_cast<microseconds>(t4 - t3).count();
    double direct_qps = (double)N / (direct_us / 1000000.0);
    double lookup_qps = (double)N / (lookup_us / 1000000.0);

    std::cout << "[BENCH] Pool KnownClass Direct:    " << fmt_qps(direct_qps)
              << " ops/sec" << std::endl;
    std::cout << "[BENCH] Pool With Index Lookup:    " << fmt_qps(lookup_qps)
              << " ops/sec" << std::endl;
    std::cout << "[BENCH] Index lookup overhead:     " << std::setprecision(1)
              << (1.0 - lookup_qps / direct_qps) * 100 << "%" << std::endl;
}

// ============================================================================
// Alloc-only throughput (free not measured, relevant for workload modeling)
// ============================================================================

TEST(BenchPool, AllocOnlyThroughput_1M_64B) {
    const int N = 1000000;
    std::vector<void*> ptrs(N);

    auto t1 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) ptrs[i] = pool_malloc(64);
    auto t2 = high_resolution_clock::now();

    auto us = duration_cast<microseconds>(t2 - t1).count();
    double qps = (double)N / (us / 1000000.0);
    double mbps = (qps * 64) / (1024.0 * 1024.0);

    std::cout << "[BENCH] Pool Alloc-Only (64B x 1M): " << fmt_qps(qps)
              << " allocs/sec, " << std::setprecision(1) << mbps << " MB/s" << std::endl;

    // Cleanup
    for (int i = 0; i < N; ++i) pool_free(ptrs[i], 64);
    EXPECT_GT(qps, 0);
}

TEST(BenchPool, FreeOnlyThroughput_1M_64B) {
    const int N = 1000000;
    std::vector<void*> ptrs(N);

    // Pre-allocate
    for (int i = 0; i < N; ++i) ptrs[i] = pool_malloc(64);

    auto t1 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) pool_free(ptrs[i], 64);
    auto t2 = high_resolution_clock::now();

    auto us = duration_cast<microseconds>(t2 - t1).count();
    double qps = (double)N / (us / 1000000.0);
    double mbps = (qps * 64) / (1024.0 * 1024.0);

    std::cout << "[BENCH] Pool Free-Only (64B x 1M):  " << fmt_qps(qps)
              << " frees/sec, " << std::setprecision(1) << mbps << " MB/s" << std::endl;
    EXPECT_GT(qps, 0);
}

// ============================================================================
// Stress: many small allocations interleaved with large allocations
// ============================================================================

TEST(BenchPool, SmallLargeInterleaved) {
    const int N = 500000;

    auto t1 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        void* small = pool_malloc(16);
        void* large = pool_malloc(4096);
        pool_free(large, 4096);
        pool_free(small, 16);
    }
    auto t2 = high_resolution_clock::now();

    auto us = duration_cast<microseconds>(t2 - t1).count();
    double qps = (double)(N * 2) / (us / 1000000.0);
    std::cout << "[BENCH] Pool Small+Large Interleaved: " << fmt_qps(qps)
              << " ops/sec (16B+4KB pairs)" << std::endl;
    EXPECT_GT(qps, 0);
}
