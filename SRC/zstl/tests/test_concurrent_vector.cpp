// ============================================================================
// zstl vector performance benchmark tests — Comprehensive Edition
// ============================================================================
// Tests: push_back throughput, reserve+push_back vs repeated reallocation,
//        emplace_back, iterator traversal speed, sort performance,
//        POD memmove optimization, growth policy observation,
//        comparison vs std::vector, large element handling,
//        and concurrent read benchmarks.
// ============================================================================

#include <gtest/gtest.h>
#include "zstl/zstl.h"

#include <vector>
#include <chrono>
#include <numeric>
#include <list>
#include <iostream>
#include <algorithm>
#include <thread>

using namespace zstl;

// Helper: format and print throughput
namespace {
void report_perf(const char* label, int ops, double ms) {
    long long ops_per_sec = static_cast<long long>(ops / (ms / 1000.0));
    std::cout << "[PERF] " << label << ": " << ops << " ops in " << ms << "ms ("
              << ops_per_sec << " ops/sec)" << std::endl;
}
}  // namespace

// ============================================================================
// Push_back throughput
// ============================================================================

TEST(PerfVector, PushBackThroughput) {
    const int N = 1000000;

    // Measure push_back throughput without pre-allocation
    {
        vector<int> v;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            v.push_back(i);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        EXPECT_EQ(v.size(), N);
        for (int i = 0; i < N; ++i) EXPECT_EQ(v[i], i);
        report_perf("vector push_back (no reserve)", N, static_cast<double>(ms));
    }

    // Measure push_back throughput with pre-allocation
    {
        vector<int> v;
        v.reserve(N);
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            v.push_back(i);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        EXPECT_EQ(v.size(), N);
        report_perf("vector push_back (reserve) ", N, static_cast<double>(ms));
    }
}

// ============================================================================
// Benchmark vs std::vector
// ============================================================================

TEST(PerfVector, CompareWithStdVector) {
    const int N = 1000000;

    auto t1 = std::chrono::high_resolution_clock::now();
    zstl::vector<int> zv;
    for (int i = 0; i < N; ++i) zv.push_back(i);
    auto t2 = std::chrono::high_resolution_clock::now();

    std::vector<int> sv;
    for (int i = 0; i < N; ++i) sv.push_back(i);
    auto t3 = std::chrono::high_resolution_clock::now();

    auto zstl_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    auto std_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();
    std::cout << "[PERF] vector push_back: zstl=" << zstl_ms << "ms, std=" << std_ms << "ms"
              << std::endl;
}

// ============================================================================
// Reserve vs no-reserve
// ============================================================================

TEST(PerfVector, ReserveVsNoReserve) {
    const int N = 1000000;

    auto t1 = std::chrono::high_resolution_clock::now();
    zstl::vector<int> v1;
    v1.reserve(N);
    for (int i = 0; i < N; ++i) v1.push_back(i);
    auto t2 = std::chrono::high_resolution_clock::now();

    zstl::vector<int> v2;
    for (int i = 0; i < N; ++i) v2.push_back(i);
    auto t3 = std::chrono::high_resolution_clock::now();

    auto reserve_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    auto noreserve_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();
    std::cout << "[PERF] vector: with_reserve=" << reserve_ms << "ms, without=" << noreserve_ms
              << "ms" << std::endl;
}

// ============================================================================
// EmplaceBack vs push_back
// ============================================================================

TEST(PerfVector, EmplaceBackVsPushBack) {
    const int N = 100000;

    // push_back
    {
        vector<int> v;
        v.reserve(N);
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) v.push_back(i);
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        report_perf("vector push_back        ", N, static_cast<double>(ms));
    }

    // emplace_back
    {
        vector<int> v;
        v.reserve(N);
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) v.emplace_back(i);
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        report_perf("vector emplace_back     ", N, static_cast<double>(ms));
    }
}

// ============================================================================
// Reserve + push_back vs repeated reallocation
// ============================================================================

TEST(PerfVector, ReserveVsRepeatedReallocation) {
    const int N = 50000;

    // Without reserve (repeated reallocation)
    {
        vector<int> v;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) v.push_back(i);
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        EXPECT_EQ(v.size(), N);
        std::cout << "[PERF] vector no-reserve: " << ms << "ms, final capacity="
                  << v.capacity() << " (size=" << v.size() << ")" << std::endl;
    }

    // With reserve
    {
        vector<int> v;
        v.reserve(N);
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) v.push_back(i);
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        EXPECT_EQ(v.size(), N);
        std::cout << "[PERF] vector with-reserve: " << ms << "ms, final capacity="
                  << v.capacity() << std::endl;
    }
}

// ============================================================================
// Iterator traversal speed
// ============================================================================

TEST(PerfVector, IteratorTraversal) {
    const int N = 10000000;
    vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = i;

    // Forward iteration
    {
        auto start = std::chrono::high_resolution_clock::now();
        long long sum = 0;
        for (auto it = v.begin(); it != v.end(); ++it) sum += *it;
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        EXPECT_EQ(sum, static_cast<long long>(N) * (N - 1) / 2);
        std::cout << "[PERF] vector iteration (iterator): 10M in " << ms << "ms, sum="
                  << sum << std::endl;
    }

    // Index-based access
    {
        auto start = std::chrono::high_resolution_clock::now();
        long long sum = 0;
        for (int i = 0; i < N; ++i) sum += v[i];
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        EXPECT_EQ(sum, static_cast<long long>(N) * (N - 1) / 2);
        std::cout << "[PERF] vector iteration (index):   10M in " << ms << "ms" << std::endl;
    }

    // Range-based for
    {
        auto start = std::chrono::high_resolution_clock::now();
        long long sum = 0;
        for (auto x : v) sum += x;
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        EXPECT_EQ(sum, static_cast<long long>(N) * (N - 1) / 2);
        std::cout << "[PERF] vector iteration (range-for): 10M in " << ms << "ms" << std::endl;
    }
}

// ============================================================================
// Sort performance: vector vs list
// ============================================================================

TEST(PerfVector, SortVectorVsList) {
    const int N = 50000;

    // Sort on zstl::vector
    {
        vector<int> v(N);
        for (int i = 0; i < N; ++i) v[i] = N - i;  // reverse sorted

        auto start = std::chrono::high_resolution_clock::now();
        zstl::sort(v.begin(), v.end());
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        // Verify sorted
        for (int i = 1; i < N; ++i) EXPECT_LE(v[i - 1], v[i]);
        report_perf("zstl::sort on vector", N, static_cast<double>(ms));
    }

    // Sort on zstl::list (via copy)
    {
        list<int> lst;
        for (int i = 0; i < N; ++i) lst.push_back(N - i);

        auto start = std::chrono::high_resolution_clock::now();
        vector<int> tmp(lst.begin(), lst.end());
        zstl::sort(tmp.begin(), tmp.end());
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        report_perf("sort via vector copy  ", N, static_cast<double>(ms));
    }
}

// ============================================================================
// Growth policy observation
// ============================================================================

TEST(PerfVector, GrowthPolicy) {
    vector<int> v;
    const int N = 1000;

    size_t last_cap = v.capacity();
    int realloc_count = 0;

    for (int i = 0; i < N; ++i) {
        v.push_back(i);
        if (v.capacity() != last_cap) {
            ++realloc_count;
            last_cap = v.capacity();
        }
    }

    EXPECT_EQ(v.size(), N);
    // With growth factor ~1.5-2x, starting from min capacity 4,
    // we expect roughly O(log N) reallocations
    EXPECT_LT(realloc_count, 30);  // log2(1000) ~ 10, be generous

    std::cout << "[PERF] vector growth: " << realloc_count << " reallocations for "
              << N << " push_backs, final capacity=" << v.capacity() << std::endl;
}

// ============================================================================
// Large element sort performance
// ============================================================================

TEST(PerfVector, LargeSizeSortPerformance) {
    const int N = 20000;

    // Create pseudo-random data
    vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = (i * 7919 + 104729) % (N * 10);

    auto start = std::chrono::high_resolution_clock::now();
    zstl::sort(v.begin(), v.end());
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Verify sorted
    for (int i = 1; i < N; ++i) EXPECT_LE(v[i - 1], v[i]);
    report_perf("zstl::sort random ints", N, static_cast<double>(ms));
}

// ============================================================================
// Sort benchmark at scale
// ============================================================================

TEST(PerfVector, SortPerformance) {
    const int N = 1000000;

    // Prepare data
    zstl::vector<int> v(N);
    srand(12345);
    for (int i = 0; i < N; ++i) v[i] = rand();

    auto start = std::chrono::high_resolution_clock::now();
    zstl::sort(v.begin(), v.end());
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_TRUE(zstl::is_sorted(v.begin(), v.end()));
    std::cout << "[PERF] vector sort: 1M elements in " << ms << "ms" << std::endl;
}

// ============================================================================
// POD memmove optimization verification
// ============================================================================

TEST(PerfVector, PODMemmoveOptimization) {
    const int N = 5000000;

    zstl::vector<int> v;
    v.reserve(N);
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) v.push_back(i);
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "[PERF] vector POD push_back: " << N << " in " << ms << "ms" << std::endl;
    EXPECT_EQ(v.size(), N);
}

// ============================================================================
// POD vs non-POD push_back benchmark
// ============================================================================

struct NonPOD {
    int x;
    std::string s;
    NonPOD(int val) : x(val), s(std::to_string(val)) {}
    NonPOD(const NonPOD& o) : x(o.x), s(o.s) {}
    NonPOD& operator=(const NonPOD& o) { x = o.x; s = o.s; return *this; }
};

TEST(PerfVector, PODvsNonPOD) {
    const int N = 500000;

    // POD
    {
        vector<int> v;
        v.reserve(N);
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) v.push_back(i);
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        report_perf("vector<int> push_back        ", N, static_cast<double>(ms));
    }

    // Non-POD
    {
        vector<NonPOD> v;
        v.reserve(N);
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) v.push_back(NonPOD(i));
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        report_perf("vector<NonPOD> push_back     ", N, static_cast<double>(ms));
    }
}

// ============================================================================
// Random access vs sequential access
// ============================================================================

TEST(PerfVector, RandomVsSequentialAccess) {
    const int N = 1000000;
    vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = i;

    // Sequential access
    {
        auto start = std::chrono::high_resolution_clock::now();
        long long sum = 0;
        for (int i = 0; i < N; ++i) sum += v[i];
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "[PERF] vector sequential access: 1M in " << ms << "ms, sum=" << sum << std::endl;
    }

    // Strided access (stride 16)
    {
        auto start = std::chrono::high_resolution_clock::now();
        long long sum = 0;
        for (int i = 0; i < N; i += 16) sum += v[i];
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "[PERF] vector strided access (16): 1M in " << ms << "ms" << std::endl;
    }

    // Random access
    {
        std::vector<int> indices(N);
        srand(42);
        for (int i = 0; i < N; ++i) indices[i] = rand() % N;

        auto start = std::chrono::high_resolution_clock::now();
        long long sum = 0;
        for (int i = 0; i < N; ++i) sum += v[indices[i]];
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "[PERF] vector random access: 1M in " << ms << "ms" << std::endl;
    }
}

// ============================================================================
// Clear and re-use benchmark
// ============================================================================

TEST(PerfVector, ClearAndReuse) {
    const int N = 1000000;
    const int rounds = 10;

    vector<int> v;
    v.reserve(N);

    auto start = std::chrono::high_resolution_clock::now();
    for (int r = 0; r < rounds; ++r) {
        for (int i = 0; i < N; ++i) v.push_back(i);
        v.clear();  // Does not free memory
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    int total_ops = N * rounds;
    report_perf("vector clear-reuse (10x1M)", total_ops, static_cast<double>(ms));
}

// ============================================================================
// Bulk insert (insert range)
// ============================================================================

TEST(PerfVector, BulkInsertRange) {
    const int N = 1000000;

    // Prepare source data
    std::vector<int> source(N);
    for (int i = 0; i < N; ++i) source[i] = i;

    // Insert at end
    {
        vector<int> v;
        auto start = std::chrono::high_resolution_clock::now();
        v.insert(v.end(), source.begin(), source.end());
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_EQ(v.size(), N);
        report_perf("vector insert(range) at end", N, static_cast<double>(ms));
    }

    // Insert at beginning (requires shift)
    {
        const int M = 10000;
        vector<int> v(M, 0);
        auto start = std::chrono::high_resolution_clock::now();
        v.insert(v.begin(), source.begin(), source.begin() + M);
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_EQ(v.size(), static_cast<size_t>(M * 2));
        report_perf("vector insert(range) at beg", M, static_cast<double>(ms));
    }
}

// ============================================================================
// Erase benchmark
// ============================================================================

TEST(PerfVector, ErasePerformance) {
    const int N = 100000;

    // Erase from end (cheap)
    {
        vector<int> v(N);
        for (int i = 0; i < N; ++i) v[i] = i;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) v.pop_back();
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_TRUE(v.empty());
        report_perf("vector pop_back (100K)    ", N, static_cast<double>(ms));
    }

    // Erase from front (expensive: requires shift)
    {
        const int M = 10000;
        vector<int> v(M);
        for (int i = 0; i < M; ++i) v[i] = i;
        auto start = std::chrono::high_resolution_clock::now();
        while (!v.empty()) v.erase(v.begin());
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_TRUE(v.empty());
        report_perf("vector erase(begin) (10K)  ", M, static_cast<double>(ms));
    }
}

// ============================================================================
// Resize benchmark
// ============================================================================

TEST(PerfVector, ResizePerformance) {
    // Growing resize
    {
        vector<int> v;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 100; ++i) {
            v.resize((i + 1) * 10000, 0);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "[PERF] vector resize(grow) to 1M: " << ms << "ms, size=" << v.size() << std::endl;
    }

    // Shrinking resize
    {
        vector<int> v(1000000, 1);
        auto start = std::chrono::high_resolution_clock::now();
        v.resize(100);
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_EQ(v.size(), 100u);
        std::cout << "[PERF] vector resize(shrink) 1M->100: " << ms << "ms" << std::endl;
    }
}

// ============================================================================
// Move construction/assignment benchmark
// ============================================================================

TEST(PerfVector, MovePerformance) {
    const int N = 1000000;
    vector<int> src(N);
    for (int i = 0; i < N; ++i) src[i] = i;

    // Move construction
    {
        auto start = std::chrono::high_resolution_clock::now();
        vector<int> dst(zstl::move(src));
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_EQ(dst.size(), N);
        EXPECT_TRUE(src.empty());
        std::cout << "[PERF] vector move-construct 1M: " << ms << "ms" << std::endl;
    }
}

// ============================================================================
// Concurrent read benchmark (many readers, no locks)
// ============================================================================

TEST(PerfVector, ConcurrentReadBenchmark) {
    const int N = 1000000;
    vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = i;

    const int num_threads = 8;

    auto start = std::chrono::high_resolution_clock::now();
    zstl::atomic<long long> total_sum{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&v, &total_sum]() {
            long long local = 0;
            for (int i = 0; i < 1000000; ++i) local += v[i];
            total_sum.fetch_add(local);
        });
    }
    for (auto& th : threads) th.join();

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Sum of 0..999999 = 499999500000, times 8
    EXPECT_EQ(total_sum.load(), num_threads * 499999500000LL);
    std::cout << "[PERF] vector concurrent read (8 threads x 1M): " << ms << "ms" << std::endl;
}

// ============================================================================
// Push_back of large objects
// ============================================================================

struct LargeObject {
    int data[64];  // 256 bytes per object
    LargeObject() { std::fill(data, data + 64, 0); }
    explicit LargeObject(int v) { std::fill(data, data + 64, v); }
};

TEST(PerfVector, LargeObjectPushBack) {
    const int N = 100000;

    // zstl::vector
    {
        vector<LargeObject> v;
        v.reserve(N);
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) v.push_back(LargeObject(i));
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        report_perf("zstl::vector LargeObject  ", N, static_cast<double>(ms));
    }

    // std::vector
    {
        std::vector<LargeObject> v;
        v.reserve(N);
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) v.push_back(LargeObject(i));
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        report_perf("std::vector  LargeObject  ", N, static_cast<double>(ms));
    }
}

// ============================================================================
// Pre-sorted vs reverse-sorted sort
// ============================================================================

TEST(PerfVector, SortPreSortedVsReverse) {
    const int N = 500000;

    // Already sorted
    {
        vector<int> v(N);
        for (int i = 0; i < N; ++i) v[i] = i;
        auto start = std::chrono::high_resolution_clock::now();
        zstl::sort(v.begin(), v.end());
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_TRUE(zstl::is_sorted(v.begin(), v.end()));
        report_perf("sort already sorted    ", N, static_cast<double>(ms));
    }

    // Reverse sorted
    {
        vector<int> v(N);
        for (int i = 0; i < N; ++i) v[i] = N - i;
        auto start = std::chrono::high_resolution_clock::now();
        zstl::sort(v.begin(), v.end());
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_TRUE(zstl::is_sorted(v.begin(), v.end()));
        report_perf("sort reverse sorted    ", N, static_cast<double>(ms));
    }

    // Random
    {
        vector<int> v(N);
        srand(9999);
        for (int i = 0; i < N; ++i) v[i] = rand();
        auto start = std::chrono::high_resolution_clock::now();
        zstl::sort(v.begin(), v.end());
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_TRUE(zstl::is_sorted(v.begin(), v.end()));
        report_perf("sort random            ", N, static_cast<double>(ms));
    }
}

// ============================================================================
// Front/back access benchmark
// ============================================================================

TEST(PerfVector, FrontBackAccess) {
    const int N = 10000000;
    vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = i;

    auto start = std::chrono::high_resolution_clock::now();
    long long sum = 0;
    for (int i = 0; i < N; ++i) {
        sum += v.front();
        sum += v.back();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "[PERF] vector front/back access: 10M in " << ms << "ms" << std::endl;
}

// ============================================================================
// Data pointer iteration benchmark
// ============================================================================

TEST(PerfVector, DataPointerIteration) {
    const int N = 10000000;
    vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = i;

    // Via data() pointer
    {
        auto start = std::chrono::high_resolution_clock::now();
        long long sum = 0;
        const int* p = v.data();
        const int* end = p + v.size();
        while (p != end) sum += *p++;
        auto tend = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tend - start).count();
        std::cout << "[PERF] vector data() iteration: 10M in " << ms << "ms, sum=" << sum << std::endl;
    }

    // Via iterator
    {
        auto start = std::chrono::high_resolution_clock::now();
        long long sum = 0;
        for (auto it = v.begin(); it != v.end(); ++it) sum += *it;
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "[PERF] vector begin/end iteration: 10M in " << ms << "ms" << std::endl;
    }
}
