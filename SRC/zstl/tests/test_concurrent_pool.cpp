// ============================================================================
// zstl pool performance benchmark tests — Comprehensive Edition
// ============================================================================
// Tests: allocation/deallocation throughput for pool vs new/delete,
//        multiple size classes, multi-threaded throughput,
//        batch allocate/free patterns, cache locality benchmarks,
//        realloc stress, long-running endurance, and oversized fallback.
// ============================================================================

#include <gtest/gtest.h>
#include "zstl/zstl.h"

#include <vector>
#include <chrono>
#include <thread>
#include <random>
#include <iostream>
#include <cstring>
#include <atomic>

using namespace zstl;

namespace {

void report_perf(const char* label, int ops, double ms) {
    long long ops_per_sec = static_cast<long long>(ops / (ms / 1000.0));
    std::cout << "[PERF] " << label << ": " << ops << " ops in " << ms << "ms ("
              << ops_per_sec << " ops/sec)" << std::endl;
}

void report_perf_nolabel(const char* prefix, double alloc_ms, double free_ms, double total_ms,
                         long long total_ops) {
    long long throughput = static_cast<long long>(total_ops / (total_ms / 1000.0));
    std::cout << "[PERF] " << prefix << ": alloc=" << alloc_ms << "ms, free=" << free_ms
              << "ms, total=" << total_ms << "ms, throughput=" << throughput << " ops/sec"
              << std::endl;
}

}  // namespace

// ============================================================================
// Pool vs new/delete throughput (single size class)
// ============================================================================

TEST(PerfPool, SingleSizeClassThroughput) {
    const int N = 100000;
    const size_t kSize = 64;

    // Pool allocation
    {
        std::vector<void*> ptrs(N);
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            ptrs[i] = pool_malloc(kSize);
            std::memset(ptrs[i], 0, kSize);
        }
        auto alloc_end = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) pool_free(ptrs[i], kSize);
        auto free_end = std::chrono::high_resolution_clock::now();

        double alloc_ms = std::chrono::duration<double, std::milli>(alloc_end - start).count();
        double free_ms = std::chrono::duration<double, std::milli>(free_end - alloc_end).count();
        double total_ms = std::chrono::duration<double, std::milli>(free_end - start).count();
        report_perf_nolabel("Pool 64B           ", alloc_ms, free_ms, total_ms, N * 2);
    }

    // new/delete
    {
        std::vector<char*> ptrs(N);
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            ptrs[i] = new char[kSize];
            std::memset(ptrs[i], 0, kSize);
        }
        auto alloc_end = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) delete[] ptrs[i];
        auto free_end = std::chrono::high_resolution_clock::now();

        double alloc_ms = std::chrono::duration<double, std::milli>(alloc_end - start).count();
        double free_ms = std::chrono::duration<double, std::milli>(free_end - alloc_end).count();
        double total_ms = std::chrono::duration<double, std::milli>(free_end - start).count();
        report_perf_nolabel("new/delete 64B     ", alloc_ms, free_ms, total_ms, N * 2);
    }
}

// ============================================================================
// Pool vs new/delete with multiple size classes
// ============================================================================

TEST(PerfPool, MultiSizeClassThroughput) {
    const int N = 50000;
    const size_t sizes[] = {8, 32, 128, 512, 1024, 4096};

    // Pool
    {
        std::vector<std::pair<void*, size_t>> ptrs;
        ptrs.reserve(N);

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            size_t sz = sizes[i % 6];
            void* p = pool_malloc(sz);
            std::memset(p, 0xAB, sz);
            ptrs.push_back({p, sz});
        }
        auto alloc_end = std::chrono::high_resolution_clock::now();
        for (auto& [p, sz] : ptrs) pool_free(p, sz);
        auto end = std::chrono::high_resolution_clock::now();

        double alloc_ms = std::chrono::duration<double, std::milli>(alloc_end - start).count();
        double free_ms = std::chrono::duration<double, std::milli>(end - alloc_end).count();
        double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
        report_perf_nolabel("Pool multi-size    ", alloc_ms, free_ms, total_ms, N * 2);
    }

    // new/delete
    {
        std::vector<std::pair<char*, size_t>> ptrs;
        ptrs.reserve(N);

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            size_t sz = sizes[i % 6];
            char* p = new char[sz];
            std::memset(p, 0xAB, sz);
            ptrs.push_back({p, sz});
        }
        auto alloc_end = std::chrono::high_resolution_clock::now();
        for (auto& [p, sz] : ptrs) delete[] p;
        auto end = std::chrono::high_resolution_clock::now();

        double alloc_ms = std::chrono::duration<double, std::milli>(alloc_end - start).count();
        double free_ms = std::chrono::duration<double, std::milli>(end - alloc_end).count();
        double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
        report_perf_nolabel("new/delete multi   ", alloc_ms, free_ms, total_ms, N * 2);
    }
}

// ============================================================================
// Mid-size allocation throughput
// ============================================================================

TEST(PerfPool, MidSizeAllocThroughput) {
    const int N = 50000;
    const size_t kSize = 256;

    // Pool
    {
        std::vector<void*> ptrs(N);
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            ptrs[i] = pool_malloc(kSize);
            ASSERT_NE(ptrs[i], nullptr);
        }
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        for (auto p : ptrs) pool_free(p, kSize);
        report_perf("Pool 256B alloc            ", N, ms);
    }

    // new/delete
    {
        std::vector<char*> ptrs(N);
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            ptrs[i] = new char[kSize];
            ASSERT_NE(ptrs[i], nullptr);
        }
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        for (auto p : ptrs) delete[] p;
        report_perf("new/delete 256B alloc      ", N, ms);
    }
}

// ============================================================================
// Multi-threaded throughput
// ============================================================================

TEST(PerfPool, MultiThreadedThroughput) {
    const int kThreads = 4;
    const int kAllocsPerThread = 25000;

    // Pool: multi-threaded
    {
        std::vector<std::thread> threads;
        auto start = std::chrono::high_resolution_clock::now();

        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back([]() {
                std::vector<void*> ptrs;
                ptrs.reserve(kAllocsPerThread);
                for (int i = 0; i < kAllocsPerThread; ++i) {
                    size_t sz = (i % 4 + 1) * 64;  // 64, 128, 192, 256
                    void* p = pool_malloc(sz);
                    std::memset(p, 0, sz);
                    ptrs.push_back(p);
                }
                for (size_t i = 0; i < ptrs.size(); ++i) {
                    pool_free(ptrs[i], (i % 4 + 1) * 64);
                }
            });
        }

        for (auto& th : threads) th.join();
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        int total_ops = kThreads * kAllocsPerThread * 2;
        report_perf("Pool 4T threaded           ", total_ops, ms);
    }

    // new/delete: multi-threaded
    {
        std::vector<std::thread> threads;
        auto start = std::chrono::high_resolution_clock::now();

        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back([]() {
                std::vector<char*> ptrs;
                ptrs.reserve(kAllocsPerThread);
                for (int i = 0; i < kAllocsPerThread; ++i) {
                    size_t sz = (i % 4 + 1) * 64;
                    char* p = new char[sz];
                    std::memset(p, 0, sz);
                    ptrs.push_back(p);
                }
                for (int i = 0; i < kAllocsPerThread; ++i) {
                    delete[] ptrs[i];
                }
            });
        }

        for (auto& th : threads) th.join();
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        int total_ops = kThreads * kAllocsPerThread * 2;
        report_perf("new/del 4T threaded        ", total_ops, ms);
    }
}

// ============================================================================
// 8-thread throughput
// ============================================================================

TEST(PerfPool, EightThreadThroughput) {
    const int kThreads = 8;
    const int kAllocsPerThread = 100000;

    // Pool
    {
        std::atomic<bool> start{false};
        zstl::atomic<long long> total_ops{0};

        std::vector<std::thread> threads;
        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back([&]() {
                while (!start.load()) {}
                for (int i = 0; i < kAllocsPerThread; ++i) {
                    size_t sz = ((i % 16) + 1) * 16;
                    void* p = pool_malloc(sz);
                    if (p) {
                        pool_free(p, sz);
                        total_ops.fetch_add(1);
                    }
                }
            });
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        start.store(true);
        for (auto& th : threads) th.join();
        auto t2 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t2 - t1).count();

        report_perf("Pool 8T x 100K             ", static_cast<int>(total_ops.load() * 2), ms);
    }

    // new/delete
    {
        std::atomic<bool> start{false};
        zstl::atomic<long long> total_ops{0};

        std::vector<std::thread> threads;
        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back([&]() {
                while (!start.load()) {}
                for (int i = 0; i < kAllocsPerThread; ++i) {
                    size_t sz = ((i % 16) + 1) * 16;
                    char* p = new char[sz];
                    delete[] p;
                    total_ops.fetch_add(1);
                }
            });
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        start.store(true);
        for (auto& th : threads) th.join();
        auto t2 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t2 - t1).count();

        report_perf("new/del 8T x 100K          ", static_cast<int>(total_ops.load() * 2), ms);
    }
}

// ============================================================================
// Batch allocate/free patterns
// ============================================================================

TEST(PerfPool, BatchAllocFreePattern) {
    const int kBatchSize = 200;
    const int kRounds = 50;
    const size_t kSize = 128;

    std::vector<void*> ptrs;
    ptrs.reserve(kBatchSize);

    double total_alloc_ms = 0;
    double total_free_ms = 0;

    for (int round = 0; round < kRounds; ++round) {
        // Batch allocate
        {
            auto start = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < kBatchSize; ++i) {
                void* p = pool_malloc(kSize);
                std::memset(p, 0, kSize);
                ptrs.push_back(p);
            }
            auto end = std::chrono::high_resolution_clock::now();
            total_alloc_ms += std::chrono::duration<double, std::milli>(end - start).count();
        }

        // Batch free
        {
            auto start = std::chrono::high_resolution_clock::now();
            for (auto p : ptrs) {
                pool_free(p, kSize);
            }
            auto end = std::chrono::high_resolution_clock::now();
            total_free_ms += std::chrono::duration<double, std::milli>(end - start).count();
        }
        ptrs.clear();
    }

    int total_ops = kBatchSize * kRounds * 2;
    double total_ms = total_alloc_ms + total_free_ms;
    report_perf_nolabel("Pool batch 200x50  ",
                        total_alloc_ms, total_free_ms, total_ms, total_ops);
}

// ============================================================================
// Large allocation throughput
// ============================================================================

TEST(PerfPool, LargeAllocThroughput) {
    const int N = 20000;
    const size_t kSize = 4096;

    // Pool
    {
        std::vector<void*> ptrs(N);
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            ptrs[i] = pool_malloc(kSize);
            ASSERT_NE(ptrs[i], nullptr);
        }
        auto alloc_end = std::chrono::high_resolution_clock::now();
        for (auto p : ptrs) pool_free(p, kSize);
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        report_perf("Pool 4KB x 20K             ", N * 2, ms);
    }

    // new/delete
    {
        std::vector<char*> ptrs(N);
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            ptrs[i] = new char[kSize];
            ASSERT_NE(ptrs[i], nullptr);
        }
        auto alloc_end = std::chrono::high_resolution_clock::now();
        for (auto p : ptrs) delete[] p;
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        report_perf("new/del 4KB x 20K          ", N * 2, ms);
    }
}

// ============================================================================
// pool_malloc_class throughput
// ============================================================================

TEST(PerfPool, MallocClassThroughput) {
    const int N = 50000;
    const size_t kIdx = 5;  // Class index 5

    std::vector<void*> ptrs(N);
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        ptrs[i] = pool_malloc_class(kIdx);
        ASSERT_NE(ptrs[i], nullptr);
    }
    auto alloc_end = std::chrono::high_resolution_clock::now();

    for (auto p : ptrs) {
        pool_free_class(p, kIdx);
    }
    auto end = std::chrono::high_resolution_clock::now();

    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    report_perf("pool_malloc_class idx=5    ", N * 2, ms);
}

// ============================================================================
// Very small allocation throughput
// ============================================================================

TEST(PerfPool, SmallAllocThroughput) {
    const int N = 200000;
    const size_t kSize = 16;

    // Pool
    {
        std::vector<void*> ptrs(N);
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            ptrs[i] = pool_malloc(kSize);
            ASSERT_NE(ptrs[i], nullptr);
        }
        auto alloc_end = std::chrono::high_resolution_clock::now();
        for (auto p : ptrs) pool_free(p, kSize);
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        report_perf("Pool 16B x 200K            ", N * 2, ms);
    }

    // new/delete
    {
        std::vector<char*> ptrs(N);
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            ptrs[i] = new char[kSize];
            ASSERT_NE(ptrs[i], nullptr);
        }
        auto alloc_end = std::chrono::high_resolution_clock::now();
        for (auto p : ptrs) delete[] p;
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        report_perf("new/del 16B x 200K         ", N * 2, ms);
    }
}

// ============================================================================
// Cache locality benchmark
// ============================================================================

TEST(PerfPool, CacheLocality) {
    const int N = 100000;
    const size_t kSize = 64;

    // Allocate N objects from pool, write to each, then read
    {
        std::vector<void*> ptrs(N);
        for (int i = 0; i < N; ++i) {
            ptrs[i] = pool_malloc(kSize);
            ASSERT_NE(ptrs[i], nullptr);
        }

        // Write phase
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            std::memset(ptrs[i], static_cast<int>(i & 0xFF), kSize);
        }
        auto write_end = std::chrono::high_resolution_clock::now();

        // Read phase
        volatile int checksum = 0;
        for (int i = 0; i < N; ++i) {
            checksum += *(static_cast<char*>(ptrs[i]));
        }
        auto read_end = std::chrono::high_resolution_clock::now();

        (void)checksum;
        for (auto p : ptrs) pool_free(p, kSize);

        double write_ms = std::chrono::duration<double, std::milli>(write_end - start).count();
        double read_ms = std::chrono::duration<double, std::milli>(read_end - write_end).count();
        std::cout << "[PERF] Pool cache locality: write=" << write_ms
                  << "ms, read=" << read_ms << "ms (100K x 64B)" << std::endl;
    }

    // Same with new/delete
    {
        std::vector<char*> ptrs(N);
        for (int i = 0; i < N; ++i) {
            ptrs[i] = new char[kSize];
            ASSERT_NE(ptrs[i], nullptr);
        }

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            std::memset(ptrs[i], static_cast<int>(i & 0xFF), kSize);
        }
        auto write_end = std::chrono::high_resolution_clock::now();

        volatile int checksum = 0;
        for (int i = 0; i < N; ++i) {
            checksum += ptrs[i][0];
        }
        auto read_end = std::chrono::high_resolution_clock::now();

        (void)checksum;
        for (auto p : ptrs) delete[] p;

        double write_ms = std::chrono::duration<double, std::milli>(write_end - start).count();
        double read_ms = std::chrono::duration<double, std::milli>(read_end - write_end).count();
        std::cout << "[PERF] new/del cache locality: write=" << write_ms
                  << "ms, read=" << read_ms << "ms (100K x 64B)" << std::endl;
    }
}

// ============================================================================
// Alloc-only throughput (no free in loop)
// ============================================================================

TEST(PerfPool, AllocOnlyThroughput) {
    const int N = 200000;
    const size_t kSize = 32;

    // Pool
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            void* p = pool_malloc(kSize);
            pool_free(p, kSize);
        }
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        report_perf("Pool alloc+free loop 32B   ", N * 2, ms);
    }

    // new/delete
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            char* p = new char[kSize];
            delete[] p;
        }
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        report_perf("new/del alloc+free loop 32B", N * 2, ms);
    }
}

// ============================================================================
// Realloc (different size) stress
// ============================================================================

TEST(PerfPool, ReallocStress) {
    const int N = 20000;
    const size_t sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048};

    // Pool realloc pattern
    {
        auto start = std::chrono::high_resolution_clock::now();
        void* p = pool_malloc(64);
        for (int i = 0; i < N; ++i) {
            size_t new_sz = sizes[i % 8];
            void* new_p = pool_malloc(new_sz);
            pool_free(p, sizes[(i > 0) ? ((i - 1) % 8) : 0]);  // approximate
            p = new_p;
        }
        pool_free(p, sizes[(N - 1) % 8]);
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        report_perf("Pool realloc 20K           ", N * 2 + 1, ms);
    }

    // new/delete realloc pattern
    {
        auto start = std::chrono::high_resolution_clock::now();
        char* p = new char[64];
        for (int i = 0; i < N; ++i) {
            size_t new_sz = sizes[i % 8];
            char* new_p = new char[new_sz];
            delete[] p;
            p = new_p;
        }
        delete[] p;
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        report_perf("new/del realloc 20K        ", N * 2 + 1, ms);
    }
}

// ============================================================================
// Full range size stress (16B to 64KB random)
// ============================================================================

TEST(PerfPool, FullRangeSizeStress) {
    const int N = 50000;

    // Pool
    {
        std::mt19937 rng(12345);
        std::uniform_int_distribution<size_t> sz_dist(16, 65536);

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            size_t sz = sz_dist(rng);
            void* p = pool_malloc(sz);
            if (p) pool_free(p, sz);
        }
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        report_perf("Pool random 16B-64KB       ", N * 2, ms);
    }

    // new/delete
    {
        std::mt19937 rng(12345);
        std::uniform_int_distribution<size_t> sz_dist(16, 65536);

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            size_t sz = sz_dist(rng);
            char* p = new char[sz];
            delete[] p;
        }
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        report_perf("new/del random 16B-64KB    ", N * 2, ms);
    }
}

// ============================================================================
// Many small + few large (mixed workload)
// ============================================================================

TEST(PerfPool, MixedSmallLargeWorkload) {
    const int N = 100000;

    // Pool
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            size_t sz;
            if (i % 100 == 0) {
                sz = 256 * 1024;  // 256KB oversized (new/delete fallback)
            } else if (i % 10 == 0) {
                sz = 4096;  // 4KB
            } else {
                sz = 64;  // common small
            }
            void* p = pool_malloc(sz);
            if (p) pool_free(p, sz);
        }
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        report_perf("Pool mixed small+large     ", N * 2, ms);
    }

    // new/delete
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            size_t sz;
            if (i % 100 == 0) {
                sz = 256 * 1024;
            } else if (i % 10 == 0) {
                sz = 4096;
            } else {
                sz = 64;
            }
            char* p = new char[sz];
            delete[] p;
        }
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        report_perf("new/del mixed small+large  ", N * 2, ms);
    }
}

// ============================================================================
// Pool trim overhead benchmark
// ============================================================================

TEST(PerfPool, PoolTrimOverhead) {
    const int N = 50000;
    const size_t kSize = 64;

    // Allocate many, then trim
    {
        std::vector<void*> ptrs;
        ptrs.reserve(N);
        for (int i = 0; i < N; ++i) {
            ptrs.push_back(pool_malloc(kSize));
        }
        for (auto p : ptrs) pool_free(p, kSize);

        // Measure trim time
        auto start = std::chrono::high_resolution_clock::now();
        MultiSizeClassPool::instance().pool_trim();
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << "[PERF] pool_trim time: " << ms << "ms (after 50K alloc/free)"
                  << std::endl;
    }
}

// ============================================================================
// One million alloc/free comparison
// ============================================================================

TEST(PerfPool, OneMillionAllocFree) {
    const int N = 1000000;

    // Pool
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            size_t sz = 16 + (i % 32) * 16;
            void* p = pool_malloc(sz);
            if (p) pool_free(p, sz);
        }
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        report_perf("Pool 1M alloc+free         ", N * 2, ms);
    }

    // new/delete
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            size_t sz = 16 + (i % 32) * 16;
            char* p = new char[sz];
            delete[] p;
        }
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        report_perf("new/del 1M alloc+free      ", N * 2, ms);
    }
}
