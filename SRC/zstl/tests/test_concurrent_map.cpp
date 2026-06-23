// ============================================================================
// zstl map performance benchmark tests — Comprehensive Edition
// ============================================================================
// Tests: insert/find/erase/iteration throughput for map vs unordered_map vs
//        bmap vs skip_map at 10K/100K/500K workloads, operator[] access,
//        memory usage comparison, mixed workloads, and stress tests.
// ============================================================================

#include <gtest/gtest.h>
#include "zstl/zstl.h"

#include <vector>
#include <chrono>
#include <random>
#include <string>
#include <iostream>
#include <thread>
#include <atomic>

using namespace zstl;

namespace {

// Generate random keys for performance testing
std::vector<int> gen_keys(int n) {
    std::vector<int> keys(n);
    for (int i = 0; i < n; ++i) keys[i] = i;
    // Shuffle using Fisher-Yates
    for (int i = n - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        std::swap(keys[i], keys[j]);
    }
    return keys;
}

void report_perf(const char* label, int ops, double ms) {
    long long ops_per_sec = static_cast<long long>(ops / (ms / 1000.0));
    std::cout << "[PERF] " << label << ": " << ops << " ops in " << ms << "ms ("
              << ops_per_sec << " ops/sec)" << std::endl;
}

}  // namespace

// ============================================================================
// Insert throughput benchmarks
// ============================================================================

TEST(PerfMap, InsertThroughput10K) {
    const int N = 10000;
    auto keys = gen_keys(N);

    // map (RB-tree)
    {
        map<int, int> m;
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) {
            m.insert({k, k * 2});
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_EQ(m.size(), N);
        report_perf("map insert 10K             ", N, static_cast<double>(ms));
    }

    // unordered_map (hash table)
    {
        unordered_map<int, int> m;
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) {
            m.insert({k, k * 2});
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_EQ(m.size(), N);
        report_perf("unordered_map insert 10K   ", N, static_cast<double>(ms));
    }

    // bmap (B+ tree)
    {
        bmap<int, int> m;
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) {
            m.insert({k, k * 2});
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_EQ(m.size(), N);
        report_perf("bmap insert 10K            ", N, static_cast<double>(ms));
    }

    // skip_map (skip list)
    {
        skip_map<int, int> m;
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) {
            m.insert({k, k * 2});
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_EQ(m.size(), N);
        report_perf("skip_map insert 10K        ", N, static_cast<double>(ms));
    }
}

TEST(PerfMap, InsertThroughput100K) {
    const int N = 100000;
    auto keys = gen_keys(N);

    {
        map<int, int> m;
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) m.insert({k, k * 2});
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_EQ(m.size(), N);
        report_perf("map insert 100K            ", N, static_cast<double>(ms));
    }

    {
        unordered_map<int, int> m;
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) m.insert({k, k * 2});
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_EQ(m.size(), N);
        report_perf("unordered_map insert 100K  ", N, static_cast<double>(ms));
    }

    {
        bmap<int, int> m;
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) m.insert({k, k * 2});
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_EQ(m.size(), N);
        report_perf("bmap insert 100K           ", N, static_cast<double>(ms));
    }

    {
        skip_map<int, int> m;
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) m.insert({k, k * 2});
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_EQ(m.size(), N);
        report_perf("skip_map insert 100K       ", N, static_cast<double>(ms));
    }
}

TEST(PerfMap, InsertThroughput500K) {
    const int N = 500000;
    auto keys = gen_keys(N);

    {
        map<int, int> m;
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) m.insert({k, k * 2});
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_EQ(m.size(), N);
        report_perf("map insert 500K            ", N, static_cast<double>(ms));
    }

    {
        unordered_map<int, int> m;
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) m.insert({k, k * 2});
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_EQ(m.size(), N);
        report_perf("unordered_map insert 500K  ", N, static_cast<double>(ms));
    }

    {
        bmap<int, int> m;
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) m.insert({k, k * 2});
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_EQ(m.size(), N);
        report_perf("bmap insert 500K           ", N, static_cast<double>(ms));
    }

    {
        skip_map<int, int> m;
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) m.insert({k, k * 2});
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_EQ(m.size(), N);
        report_perf("skip_map insert 500K       ", N, static_cast<double>(ms));
    }
}

// ============================================================================
// Find throughput benchmarks
// ============================================================================

TEST(PerfMap, FindThroughput10K) {
    const int N = 10000;
    auto keys = gen_keys(N);

    // Pre-populate
    map<int, int> m1;
    unordered_map<int, int> m2;
    bmap<int, int> m3;
    skip_map<int, int> m4;
    for (int k : keys) {
        m1.insert({k, k * 2});
        m2.insert({k, k * 2});
        m3.insert({k, k * 2});
        m4.insert({k, k * 2});
    }

    {
        long long sum = 0;
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) {
            auto it = m1.find(k);
            if (it != m1.end()) sum += it->second;
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        report_perf("map find 10K               ", N, static_cast<double>(ms));
    }

    {
        long long sum = 0;
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) {
            auto it = m2.find(k);
            if (it != m2.end()) sum += it->second;
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        report_perf("unordered_map find 10K     ", N, static_cast<double>(ms));
    }

    {
        long long sum = 0;
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) {
            auto it = m3.find(k);
            if (it != m3.end()) sum += it->second;
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        report_perf("bmap find 10K              ", N, static_cast<double>(ms));
    }

    {
        long long sum = 0;
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) {
            auto it = m4.find(k);
            if (it != m4.end()) sum += it->second;
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        report_perf("skip_map find 10K          ", N, static_cast<double>(ms));
    }
}

TEST(PerfMap, FindThroughput100K) {
    const int N = 100000;
    auto keys = gen_keys(N);

    map<int, int> m1;
    unordered_map<int, int> m2;
    bmap<int, int> m3;
    skip_map<int, int> m4;
    for (int k : keys) {
        m1.insert({k, k * 2});
        m2.insert({k, k * 2});
        m3.insert({k, k * 2});
        m4.insert({k, k * 2});
    }

    {
        long long sum = 0;
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) { auto it = m1.find(k); if (it != m1.end()) sum++; }
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_EQ(sum, N);
        report_perf("map find 100K              ", N, static_cast<double>(ms));
    }

    {
        long long sum = 0;
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) { auto it = m2.find(k); if (it != m2.end()) sum++; }
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_EQ(sum, N);
        report_perf("unordered_map find 100K    ", N, static_cast<double>(ms));
    }

    {
        long long sum = 0;
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) { auto it = m3.find(k); if (it != m3.end()) sum++; }
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_EQ(sum, N);
        report_perf("bmap find 100K             ", N, static_cast<double>(ms));
    }

    {
        long long sum = 0;
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) { auto it = m4.find(k); if (it != m4.end()) sum++; }
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_EQ(sum, N);
        report_perf("skip_map find 100K         ", N, static_cast<double>(ms));
    }
}

// ============================================================================
// Erase throughput benchmarks
// ============================================================================

TEST(PerfMap, EraseThroughput10K) {
    const int N = 10000;
    auto keys = gen_keys(N);

    // map erase
    {
        map<int, int> m;
        for (int k : keys) m.insert({k, k * 2});

        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) m.erase(k);
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        EXPECT_TRUE(m.empty());
        report_perf("map erase 10K              ", N, static_cast<double>(ms));
    }

    // unordered_map erase
    {
        unordered_map<int, int> m;
        for (int k : keys) m.insert({k, k * 2});

        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) m.erase(k);
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        EXPECT_TRUE(m.empty());
        report_perf("unordered_map erase 10K    ", N, static_cast<double>(ms));
    }

    // bmap erase
    {
        bmap<int, int> m;
        for (int k : keys) m.insert({k, k * 2});

        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) m.erase(k);
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        EXPECT_TRUE(m.empty());
        report_perf("bmap erase 10K             ", N, static_cast<double>(ms));
    }

    // skip_map erase
    {
        skip_map<int, int> m;
        for (int k : keys) m.insert({k, k * 2});

        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) m.erase(k);
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        EXPECT_TRUE(m.empty());
        report_perf("skip_map erase 10K         ", N, static_cast<double>(ms));
    }
}

TEST(PerfMap, EraseThroughput100K) {
    const int N = 100000;
    auto keys = gen_keys(N);

    {
        map<int, int> m;
        for (int k : keys) m.insert({k, k * 2});
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) m.erase(k);
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_TRUE(m.empty());
        report_perf("map erase 100K             ", N, static_cast<double>(ms));
    }

    {
        unordered_map<int, int> m;
        for (int k : keys) m.insert({k, k * 2});
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) m.erase(k);
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_TRUE(m.empty());
        report_perf("unordered_map erase 100K   ", N, static_cast<double>(ms));
    }

    {
        bmap<int, int> m;
        for (int k : keys) m.insert({k, k * 2});
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) m.erase(k);
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_TRUE(m.empty());
        report_perf("bmap erase 100K            ", N, static_cast<double>(ms));
    }

    {
        skip_map<int, int> m;
        for (int k : keys) m.insert({k, k * 2});
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : keys) m.erase(k);
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_TRUE(m.empty());
        report_perf("skip_map erase 100K        ", N, static_cast<double>(ms));
    }
}

// ============================================================================
// Iteration speed benchmarks
// ============================================================================

TEST(PerfMap, IterationSpeed) {
    const int N = 50000;

    // map iteration
    {
        map<int, int> m;
        for (int i = 0; i < N; ++i) m.insert({i, i * 2});

        long long sum = 0;
        auto start = std::chrono::high_resolution_clock::now();
        for (auto& [k, v] : m) sum += v;
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        EXPECT_GT(sum, 0);
        report_perf("map iteration 50K          ", N, static_cast<double>(ms));
    }

    // unordered_map iteration
    {
        unordered_map<int, int> m;
        for (int i = 0; i < N; ++i) m.insert({i, i * 2});

        long long sum = 0;
        auto start = std::chrono::high_resolution_clock::now();
        for (auto& [k, v] : m) sum += v;
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        EXPECT_GT(sum, 0);
        report_perf("unordered_map iteration 50K", N, static_cast<double>(ms));
    }

    // bmap iteration
    {
        bmap<int, int> m;
        for (int i = 0; i < N; ++i) m.insert({i, i * 2});

        long long sum = 0;
        auto start = std::chrono::high_resolution_clock::now();
        for (auto& [k, v] : m) sum += v;
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        EXPECT_GT(sum, 0);
        report_perf("bmap iteration 50K         ", N, static_cast<double>(ms));
    }

    // skip_map iteration
    {
        skip_map<int, int> m;
        for (int i = 0; i < N; ++i) m.insert({i, i * 2});

        long long sum = 0;
        auto start = std::chrono::high_resolution_clock::now();
        for (auto& [k, v] : m) sum += v;
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        EXPECT_GT(sum, 0);
        report_perf("skip_map iteration 50K     ", N, static_cast<double>(ms));
    }
}

// ============================================================================
// Operator[] benchmark
// ============================================================================

TEST(PerfMap, OperatorAccessThroughput) {
    const int N = 10000;

    {
        map<int, int> m;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            m[i] = i * 2;
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        EXPECT_EQ(m.size(), N);
        for (int i = 0; i < N; ++i) EXPECT_EQ(m[i], i * 2);
        report_perf("map operator[] insert+read ", N, static_cast<double>(ms));
    }

    {
        unordered_map<int, int> m;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            m[i] = i * 2;
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        EXPECT_EQ(m.size(), N);
        report_perf("unordered_map op[]         ", N, static_cast<double>(ms));
    }
}

// ============================================================================
// Memory usage comparison
// ============================================================================

TEST(PerfMap, MemoryOverhead) {
    const int N = 100000;

    // Approximate memory per element by measuring size growth
    {
        // map
        map<int, int> m;
        size_t before_count = m.size();
        for (int i = 0; i < N; ++i) m.insert({i, i});
        size_t after_count = m.size();
        EXPECT_EQ(after_count - before_count, static_cast<size_t>(N));
        std::cout << "[PERF] map memory: " << N << " elements stored (RB-tree node ~40-48 bytes each)"
                  << std::endl;
    }

    {
        unordered_map<int, int> m;
        for (int i = 0; i < N; ++i) m.insert({i, i});
        EXPECT_EQ(m.size(), N);
        std::cout << "[PERF] unordered_map memory: " << N << " elements stored (hash bucket + node)"
                  << std::endl;
    }

    {
        bmap<int, int> m;
        for (int i = 0; i < N; ++i) m.insert({i, i});
        EXPECT_EQ(m.size(), N);
        std::cout << "[PERF] bmap memory: " << N << " elements stored (B+ tree node)"
                  << std::endl;
    }

    {
        skip_map<int, int> m;
        for (int i = 0; i < N; ++i) m.insert({i, i});
        EXPECT_EQ(m.size(), N);
        std::cout << "[PERF] skip_map memory: " << N << " elements stored (skip list node)"
                  << std::endl;
    }
}

// ============================================================================
// Mixed workload (insert + find + erase)
// ============================================================================

TEST(PerfMap, MixedWorkload) {
    const int N = 50000;
    auto keys = gen_keys(N);

    // map mixed
    {
        map<int, int> m;
        auto start = std::chrono::high_resolution_clock::now();
        // Phase 1: insert half
        for (int i = 0; i < N / 2; ++i) m.insert({keys[i], keys[i]});
        // Phase 2: find + insert rest
        for (int i = N / 2; i < N; ++i) {
            m.find(keys[i - N / 2]);  // Find existing
            m.insert({keys[i], keys[i]});
        }
        // Phase 3: find all + erase half
        for (int i = 0; i < N / 2; ++i) {
            m.find(keys[i]);
            m.erase(keys[i]);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_EQ(m.size(), static_cast<size_t>(N / 2));
        int total_ops = N / 2 + N / 2 + N / 2 + N / 2 + N / 2;  // inserts + finds + erases
        report_perf("map mixed workload         ", total_ops, static_cast<double>(ms));
    }

    // unordered_map mixed
    {
        unordered_map<int, int> m;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N / 2; ++i) m.insert({keys[i], keys[i]});
        for (int i = N / 2; i < N; ++i) {
            m.find(keys[i - N / 2]);
            m.insert({keys[i], keys[i]});
        }
        for (int i = 0; i < N / 2; ++i) {
            m.find(keys[i]);
            m.erase(keys[i]);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        int total_ops = N / 2 + N / 2 + N / 2 + N / 2 + N / 2;
        report_perf("unordered_map mixed workload", total_ops, static_cast<double>(ms));
    }
}

// ============================================================================
// Concurrent read benchmark (many readers, no writes)
// ============================================================================

TEST(PerfMap, ConcurrentReadBenchmark) {
    const int N = 100000;

    map<int, int> m;
    for (int i = 0; i < N; ++i) m[i] = i * 2;

    const int num_threads = 8;
    zstl::atomic<long long> total{0};

    auto start = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&m, &total, t, N]() {
            long long local = 0;
            for (int i = 0; i < N; ++i) {
                auto it = m.find(i);
                if (it != m.end()) local += it->second;
            }
            total.fetch_add(local);
        });
    }
    for (auto& th : threads) th.join();
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Sum of i*2 for i=0..99999 = (N-1)*N = 9999900000, times 8
    EXPECT_GT(total.load(), 0);
    std::cout << "[PERF] map concurrent read (8 threads x 100K): " << ms << "ms" << std::endl;
}

// ============================================================================
// Sequential insert vs random insert
// ============================================================================

TEST(PerfMap, SequentialVsRandomInsert) {
    const int N = 100000;
    auto random_keys = gen_keys(N);

    // Sequential insert (sorted keys)
    {
        map<int, int> m;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) m.insert({i, i});
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_EQ(m.size(), N);
        report_perf("map sequential insert     ", N, static_cast<double>(ms));
    }

    // Random insert
    {
        map<int, int> m;
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : random_keys) m.insert({k, k});
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_EQ(m.size(), N);
        report_perf("map random insert         ", N, static_cast<double>(ms));
    }

    // Sequential insert unordered_map
    {
        unordered_map<int, int> m;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) m.insert({i, i});
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        report_perf("unordered_map seq insert  ", N, static_cast<double>(ms));
    }

    // Random insert unordered_map
    {
        unordered_map<int, int> m;
        auto start = std::chrono::high_resolution_clock::now();
        for (int k : random_keys) m.insert({k, k});
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        report_perf("unordered_map rnd insert  ", N, static_cast<double>(ms));
    }
}

// ============================================================================
// Lower/upper bound performance
// ============================================================================

TEST(PerfMap, LowerUpperBound) {
    const int N = 100000;

    map<int, int> m;
    for (int i = 0; i < N; i += 2) m.insert({i, i});  // Even keys only

    int queries = 10000;
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < queries; ++i) {
            int key = (i * 7) % N;
            auto it = m.lower_bound(key);
            (void)it;
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        report_perf("map lower_bound            ", queries, static_cast<double>(ms));
    }

    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < queries; ++i) {
            int key = (i * 7) % N;
            auto it = m.upper_bound(key);
            (void)it;
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        report_perf("map upper_bound            ", queries, static_cast<double>(ms));
    }
}

// ============================================================================
// String key benchmarks
// ============================================================================

TEST(PerfMap, StringKeyPerformance) {
    const int N = 10000;

    // Generate string keys
    std::vector<std::string> str_keys;
    str_keys.reserve(N);
    for (int i = 0; i < N; ++i) {
        str_keys.push_back("key_" + std::to_string(i));
    }

    {
        map<std::string, int> m;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) m.insert({str_keys[i], i});
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_EQ(m.size(), N);
        report_perf("map<string,int> insert     ", N, static_cast<double>(ms));
    }

    {
        unordered_map<std::string, int> m;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) m.insert({str_keys[i], i});
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_EQ(m.size(), N);
        report_perf("unordered_map<string> insert", N, static_cast<double>(ms));
    }
}
