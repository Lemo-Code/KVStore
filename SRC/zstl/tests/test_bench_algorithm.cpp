// ============================================================================
// zstl algorithm performance benchmarks
// ============================================================================
// Benchmarks: sort (introsort), find, copy (memmove fast path), fill (memset
//            fast path), transform, lower_bound, binary_search. Each compared
//            against std:: equivalents.
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
#include <random>
#include <numeric>
#include <cstring>

using namespace zstl;
using namespace std::chrono;

// ============================================================================
// Helper: format large numbers
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
// Sort benchmarks — zstl::sort (introsort) vs std::sort
// ============================================================================

TEST(BenchAlgorithm, SortQPS_Int_1M) {
    const int N = 1000000;
    std::mt19937 rng(42);

    // Generate a single random array, then copy for each test
    std::vector<int> base(N);
    for (int i = 0; i < N; ++i) base[i] = rng();

    // zstl::sort
    {
        zstl::vector<int> v(N);
        zstl::copy(base.begin(), base.end(), v.begin());
        auto t1 = high_resolution_clock::now();
        zstl::sort(v.begin(), v.end());
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        EXPECT_TRUE(zstl::is_sorted(v.begin(), v.end()));
        std::cout << "[BENCH] zstl::sort (int, 1M random):  " << us / 1000.0
                  << " ms" << std::endl;
    }

    // std::sort
    {
        std::vector<int> v(N);
        std::copy(base.begin(), base.end(), v.begin());
        auto t1 = high_resolution_clock::now();
        std::sort(v.begin(), v.end());
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        EXPECT_TRUE(std::is_sorted(v.begin(), v.end()));
        std::cout << "[BENCH] std::sort  (int, 1M random):  " << us / 1000.0
                  << " ms" << std::endl;
    }
}

TEST(BenchAlgorithm, SortQPS_Int_AlreadySorted_1M) {
    const int N = 1000000;
    zstl::vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = i;

    // zstl::sort on sorted data
    {
        auto t1 = high_resolution_clock::now();
        zstl::sort(v.begin(), v.end());
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        EXPECT_TRUE(zstl::is_sorted(v.begin(), v.end()));
        std::cout << "[BENCH] zstl::sort (int, 1M sorted):   " << us / 1000.0
                  << " ms" << std::endl;
    }
}

TEST(BenchAlgorithm, SortQPS_Int_ReverseSorted_1M) {
    const int N = 1000000;
    zstl::vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = N - i;

    // zstl::sort on reverse sorted
    {
        auto t1 = high_resolution_clock::now();
        zstl::sort(v.begin(), v.end());
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        EXPECT_TRUE(zstl::is_sorted(v.begin(), v.end()));
        std::cout << "[BENCH] zstl::sort (int, 1M reversed): " << us / 1000.0
                  << " ms" << std::endl;
    }
}

TEST(BenchAlgorithm, SortQPS_Int_AllEqual_1M) {
    const int N = 1000000;
    zstl::vector<int> v(N, 42);

    auto t1 = high_resolution_clock::now();
    zstl::sort(v.begin(), v.end());
    auto t2 = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(t2 - t1).count();
    EXPECT_TRUE(zstl::is_sorted(v.begin(), v.end()));
    std::cout << "[BENCH] zstl::sort (int, 1M equal):     " << us / 1000.0
              << " ms" << std::endl;
}

TEST(BenchAlgorithm, SortThroughput_MBps_1M) {
    const int N = 1000000;
    std::mt19937 rng(42);
    zstl::vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = rng();

    auto t1 = high_resolution_clock::now();
    zstl::sort(v.begin(), v.end());
    auto t2 = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(t2 - t1).count();
    double mbps = ((double)N * sizeof(int)) / (us / 1000000.0) / (1024.0 * 1024.0);
    double qps = (double)N / (us / 1000000.0);

    std::cout << "[BENCH] zstl::sort Throughput (1M):     "
              << std::fixed << std::setprecision(1) << mbps << " MB/s"
              << " (" << fmt_qps(qps) << " elements/sec)" << std::endl;
    EXPECT_GT(qps, 0);
}

TEST(BenchAlgorithm, PartialSortQPS) {
    const int N = 1000000;
    const int K = 100;  // sort top 100
    std::mt19937 rng(42);
    zstl::vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = rng();

    auto t1 = high_resolution_clock::now();
    zstl::partial_sort(v.begin(), v.begin() + K, v.end());
    auto t2 = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(t2 - t1).count();

    std::cout << "[BENCH] zstl::partial_sort (top 100/1M): " << us / 1000.0
              << " ms" << std::endl;
    // Verify first K are sorted
    EXPECT_TRUE(zstl::is_sorted(v.begin(), v.begin() + K));
}

TEST(BenchAlgorithm, StableSortVsSort) {
    const int N = 500000;
    std::mt19937 rng(42);
    zstl::vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = rng();

    // sort
    {
        zstl::vector<int> v1 = v;
        auto t1 = high_resolution_clock::now();
        zstl::sort(v1.begin(), v1.end());
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        std::cout << "[BENCH] zstl::sort        (0.5M):      " << us / 1000.0
                  << " ms" << std::endl;
    }

    // stable_sort
    {
        zstl::vector<int> v2 = v;
        auto t1 = high_resolution_clock::now();
        zstl::stable_sort(v2.begin(), v2.end());
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        std::cout << "[BENCH] zstl::stable_sort  (0.5M):      " << us / 1000.0
                  << " ms" << std::endl;
    }
}

// ============================================================================
// Find benchmarks — zstl::find vs std::find
// ============================================================================

TEST(BenchAlgorithm, FindQPS_Hit_Middle_1M) {
    const int N = 1000000;
    zstl::vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = i;
    int target = N / 2;

    // zstl::find
    {
        volatile auto it = v.begin();
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < 100; ++i) {
            it = zstl::find(v.begin(), v.end(), target);
        }
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        // Average over 100 iterations
        double avg_us_per_find = us / 100.0;
        double qps = 1000000.0 / avg_us_per_find;
        std::cout << "[BENCH] zstl::find  (hit middle, 1M):   "
                  << std::fixed << std::setprecision(1) << avg_us_per_find << " us/op, "
                  << fmt_qps(qps) << " ops/sec" << std::endl;
        EXPECT_NE(it, v.end());
    }

    // std::find
    {
        std::vector<int> sv(N);
        for (int i = 0; i < N; ++i) sv[i] = i;
        volatile long long sink = 0;
        auto it = sv.begin();
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < 100; ++i) {
            it = std::find(sv.begin(), sv.end(), target);
            sink = (it - sv.begin());
        }
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double avg_us_per_find = us / 100.0;
        double qps = 1000000.0 / avg_us_per_find;
        std::cout << "[BENCH] std::find (hit middle, 1M):   "
                  << std::fixed << std::setprecision(1) << avg_us_per_find << " us/op, "
                  << fmt_qps(qps) << " ops/sec" << std::endl;
        EXPECT_NE(it, sv.end());
        EXPECT_GT(sink, 0);
    }
}

TEST(BenchAlgorithm, FindQPS_Miss_1M) {
    const int N = 1000000;
    zstl::vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = i;
    int target = N + 1;  // will not be found

    // zstl::find miss
    {
        volatile long long sink = 0;
        auto it = v.begin();
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < 100; ++i) {
            it = zstl::find(v.begin(), v.end(), target);
            sink = (it - v.begin());
        }
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double avg_us = us / 100.0;
        double qps = 1000000.0 / avg_us;
        std::cout << "[BENCH] zstl::find  (miss, 1M):         "
                  << std::fixed << std::setprecision(1) << avg_us << " us/op, "
                  << fmt_qps(qps) << " ops/sec" << std::endl;
        EXPECT_EQ(it, v.end());
        EXPECT_GT(sink, 0);
    }

    // std::find miss
    {
        std::vector<int> sv(N);
        for (int i = 0; i < N; ++i) sv[i] = i;
        volatile long long sink = 0;
        auto it = sv.begin();
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < 100; ++i) {
            it = std::find(sv.begin(), sv.end(), target);
            sink = (it - sv.begin());
        }
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double avg_us = us / 100.0;
        double qps = 1000000.0 / avg_us;
        std::cout << "[BENCH] std::find (miss, 1M):         "
                  << std::fixed << std::setprecision(1) << avg_us << " us/op, "
                  << fmt_qps(qps) << " ops/sec" << std::endl;
        EXPECT_EQ(it, sv.end());
    }
}

TEST(BenchAlgorithm, FindIfQPS) {
    const int N = 1000000;
    zstl::vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = i;

    // Find first even number > 900000
    auto pred = [](int x) { return x > 900000 && x % 2 == 0; };

    volatile auto it = v.begin();
    auto t1 = high_resolution_clock::now();
    for (int i = 0; i < 500; ++i) {
        it = zstl::find_if(v.begin(), v.end(), pred);
    }
    auto t2 = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(t2 - t1).count();
    double avg_us = us / 500.0;
    double qps = 1000000.0 / avg_us;
    std::cout << "[BENCH] zstl::find_if (predicate, 1M):    "
              << std::fixed << std::setprecision(1) << avg_us << " us/op, "
              << fmt_qps(qps) << " ops/sec" << std::endl;
    EXPECT_NE(it, v.end());
}

// ============================================================================
// Copy benchmarks — zstl::copy (memmove fast path) vs std::copy
// ============================================================================

TEST(BenchAlgorithm, CopyQPS_Int_1M) {
    const int N = 10000000;
    zstl::vector<int> src(N);
    for (int i = 0; i < N; ++i) src[i] = i;

    // zstl::copy
    {
        zstl::vector<int> dst(N);
        auto t1 = high_resolution_clock::now();
        zstl::copy(src.begin(), src.end(), dst.begin());
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double mbps = ((double)N * sizeof(int)) / (us / 1000000.0) / (1024.0 * 1024.0);
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] zstl::copy  (int, 10M):         "
                  << std::fixed << std::setprecision(1) << mbps << " MB/s ("
                  << us / 1000.0 << " ms, " << fmt_qps(qps) << " elems/sec)" << std::endl;
        EXPECT_EQ(dst[0], 0);
        EXPECT_EQ(dst[N - 1], N - 1);
    }

    // std::copy
    {
        std::vector<int> src_v(N);
        for (int i = 0; i < N; ++i) src_v[i] = i;
        std::vector<int> dst(N);
        auto t1 = high_resolution_clock::now();
        std::copy(src_v.begin(), src_v.end(), dst.begin());
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double mbps = ((double)N * sizeof(int)) / (us / 1000000.0) / (1024.0 * 1024.0);
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] std::copy (int, 10M):         "
                  << std::fixed << std::setprecision(1) << mbps << " MB/s ("
                  << us / 1000.0 << " ms, " << fmt_qps(qps) << " elems/sec)" << std::endl;
        EXPECT_EQ(dst[0], 0);
        EXPECT_EQ(dst[N - 1], N - 1);
    }
}

TEST(BenchAlgorithm, CopyQPS_Char_10M) {
    const int N = 10000000;
    zstl::vector<char> src(N, 'A');

    // zstl::copy
    {
        zstl::vector<char> dst(N);
        auto t1 = high_resolution_clock::now();
        zstl::copy(src.begin(), src.end(), dst.begin());
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double mbps = ((double)N) / (us / 1000000.0) / (1024.0 * 1024.0);
        std::cout << "[BENCH] zstl::copy  (char, 10M):        "
                  << std::fixed << std::setprecision(1) << mbps << " MB/s ("
                  << us / 1000.0 << " ms)" << std::endl;
        EXPECT_EQ(dst[0], 'A');
    }

    // std::copy
    {
        std::vector<char> src_v(N, 'A');
        std::vector<char> dst(N);
        auto t1 = high_resolution_clock::now();
        std::copy(src_v.begin(), src_v.end(), dst.begin());
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double mbps = ((double)N) / (us / 1000000.0) / (1024.0 * 1024.0);
        std::cout << "[BENCH] std::copy (char, 10M):        "
                  << std::fixed << std::setprecision(1) << mbps << " MB/s ("
                  << us / 1000.0 << " ms)" << std::endl;
        EXPECT_EQ(dst[0], 'A');
    }
}

// ============================================================================
// Fill benchmarks — zstl::fill (memset fast path) vs std::fill
// ============================================================================

TEST(BenchAlgorithm, FillQPS_Char_10M) {
    const int N = 10000000;

    // zstl::fill
    {
        zstl::vector<char> v(N);
        auto t1 = high_resolution_clock::now();
        zstl::fill(v.begin(), v.end(), 'X');
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double mbps = ((double)N) / (us / 1000000.0) / (1024.0 * 1024.0);
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] zstl::fill  (char, 10M):        "
                  << std::fixed << std::setprecision(1) << mbps << " MB/s ("
                  << us / 1000.0 << " ms, " << fmt_qps(qps) << " elems/sec)" << std::endl;
        EXPECT_EQ(v[0], 'X');
    }

    // std::fill
    {
        std::vector<char> v(N);
        auto t1 = high_resolution_clock::now();
        std::fill(v.begin(), v.end(), 'X');
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double mbps = ((double)N) / (us / 1000000.0) / (1024.0 * 1024.0);
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] std::fill (char, 10M):        "
                  << std::fixed << std::setprecision(1) << mbps << " MB/s ("
                  << us / 1000.0 << " ms, " << fmt_qps(qps) << " elems/sec)" << std::endl;
        EXPECT_EQ(v[0], 'X');
    }
}

TEST(BenchAlgorithm, FillQPS_Int_10M) {
    const int N = 10000000;

    // zstl::fill — no memset fast path for int (size != 1)
    {
        zstl::vector<int> v(N);
        auto t1 = high_resolution_clock::now();
        zstl::fill(v.begin(), v.end(), 42);
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double mbps = ((double)N * sizeof(int)) / (us / 1000000.0) / (1024.0 * 1024.0);
        std::cout << "[BENCH] zstl::fill  (int, 10M):         "
                  << std::fixed << std::setprecision(1) << mbps << " MB/s ("
                  << us / 1000.0 << " ms)" << std::endl;
        EXPECT_EQ(v[0], 42);
    }

    // std::fill
    {
        std::vector<int> v(N);
        auto t1 = high_resolution_clock::now();
        std::fill(v.begin(), v.end(), 42);
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double mbps = ((double)N * sizeof(int)) / (us / 1000000.0) / (1024.0 * 1024.0);
        std::cout << "[BENCH] std::fill (int, 10M):         "
                  << std::fixed << std::setprecision(1) << mbps << " MB/s ("
                  << us / 1000.0 << " ms)" << std::endl;
        EXPECT_EQ(v[0], 42);
    }
}

// ============================================================================
// Transform benchmarks — zstl::transform vs std::transform
// ============================================================================

TEST(BenchAlgorithm, TransformQPS_1M) {
    const int N = 5000000;
    zstl::vector<int> src(N);
    for (int i = 0; i < N; ++i) src[i] = i;

    // zstl::transform (unary)
    {
        zstl::vector<int> dst(N);
        auto t1 = high_resolution_clock::now();
        zstl::transform(src.begin(), src.end(), dst.begin(),
                        [](int x) { return x * 2 + 1; });
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] zstl::transform (unary, 5M):    "
                  << fmt_qps(qps) << " ops/sec (" << us / 1000.0 << " ms)" << std::endl;
        EXPECT_EQ(dst[100], 201);
    }

    // std::transform (unary)
    {
        std::vector<int> src_v(N);
        for (int i = 0; i < N; ++i) src_v[i] = i;
        std::vector<int> dst(N);
        auto t1 = high_resolution_clock::now();
        std::transform(src_v.begin(), src_v.end(), dst.begin(),
                       [](int x) { return x * 2 + 1; });
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] std::transform (unary, 5M):    "
                  << fmt_qps(qps) << " ops/sec (" << us / 1000.0 << " ms)" << std::endl;
        EXPECT_EQ(dst[100], 201);
    }
}

TEST(BenchAlgorithm, TransformBinaryQPS_1M) {
    const int N = 5000000;
    zstl::vector<int> a(N, 3);
    zstl::vector<int> b(N, 5);
    zstl::vector<int> dst(N);

    auto t1 = high_resolution_clock::now();
    zstl::transform(a.begin(), a.end(), b.begin(), dst.begin(),
                    [](int x, int y) { return x * 10 + y; });
    auto t2 = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(t2 - t1).count();
    double qps = (double)N / (us / 1000000.0);
    std::cout << "[BENCH] zstl::transform (binary, 5M):     "
              << fmt_qps(qps) << " ops/sec (" << us / 1000.0 << " ms)" << std::endl;
    EXPECT_EQ(dst[0], 35);
}

// ============================================================================
// Lower bound / binary search benchmarks
// ============================================================================

TEST(BenchAlgorithm, LowerBoundQPS_1M) {
    const int N = 1000000;
    zstl::vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = i * 2;  // sorted, gap of 2

    const int SEARCHES = 1000;
    std::mt19937 rng(42);
    std::vector<int> queries(SEARCHES);
    for (int i = 0; i < SEARCHES; ++i) queries[i] = rng() % (N * 2);

    // zstl::lower_bound
    {
        volatile long long sum = 0;
        auto t1 = high_resolution_clock::now();
        for (auto q : queries) {
            auto it = zstl::lower_bound(v.begin(), v.end(), q);
            sum += (it - v.begin());
        }
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double ns_per_op = (us * 1000.0) / SEARCHES;
        double qps = (double)SEARCHES / (us / 1000000.0);
        std::cout << "[BENCH] zstl::lower_bound (1M sorted):  "
                  << std::fixed << std::setprecision(0) << ns_per_op << " ns/op, "
                  << fmt_qps(qps) << " ops/sec" << std::endl;
        EXPECT_GT(sum, 0);
    }

    // std::lower_bound
    {
        std::vector<int> sv(N);
        for (int i = 0; i < N; ++i) sv[i] = i * 2;
        volatile long long sum = 0;
        auto t1 = high_resolution_clock::now();
        for (auto q : queries) {
            auto it = std::lower_bound(sv.begin(), sv.end(), q);
            sum += (it - sv.begin());
        }
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double ns_per_op = (us * 1000.0) / SEARCHES;
        double qps = (double)SEARCHES / (us / 1000000.0);
        std::cout << "[BENCH] std::lower_bound (1M sorted):  "
                  << std::fixed << std::setprecision(0) << ns_per_op << " ns/op, "
                  << fmt_qps(qps) << " ops/sec" << std::endl;
        EXPECT_GT(sum, 0);
    }
}

TEST(BenchAlgorithm, BinarySearchQPS_1M) {
    const int N = 1000000;
    zstl::vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = i * 2;

    const int SEARCHES = 1000;
    std::mt19937 rng(42);
    std::vector<int> queries(SEARCHES);
    for (int i = 0; i < SEARCHES; ++i) queries[i] = rng() % (N * 2);

    // zstl::binary_search
    {
        volatile int found = 0;
        auto t1 = high_resolution_clock::now();
        for (auto q : queries) {
            if (zstl::binary_search(v.begin(), v.end(), q)) found = found + 1;
        }
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double ns_per_op = (us * 1000.0) / SEARCHES;
        double qps = (double)SEARCHES / (us / 1000000.0);
        std::cout << "[BENCH] zstl::binary_search (1M):       "
                  << std::fixed << std::setprecision(0) << ns_per_op << " ns/op, "
                  << fmt_qps(qps) << " ops/sec" << std::endl;
        EXPECT_GT(found, 0);
    }

    // std::binary_search
    {
        std::vector<int> sv(N);
        for (int i = 0; i < N; ++i) sv[i] = i * 2;
        volatile int found = 0;
        auto t1 = high_resolution_clock::now();
        for (auto q : queries) {
            if (std::binary_search(sv.begin(), sv.end(), q)) found = found + 1;
        }
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double ns_per_op = (us * 1000.0) / SEARCHES;
        double qps = (double)SEARCHES / (us / 1000000.0);
        std::cout << "[BENCH] std::binary_search (1M):       "
                  << std::fixed << std::setprecision(0) << ns_per_op << " ns/op, "
                  << fmt_qps(qps) << " ops/sec" << std::endl;
        EXPECT_GT(found, 0);
    }
}

// ============================================================================
// for_each / iota benchmark
// ============================================================================

TEST(BenchAlgorithm, ForEachQPS_1M) {
    const int N = 10000000;
    zstl::vector<int> v(N);

    // zstl::for_each
    {
        auto t1 = high_resolution_clock::now();
        zstl::for_each(v.begin(), v.end(), [](int& x) { x = 42; });
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] zstl::for_each (10M):           "
                  << fmt_qps(qps) << " ops/sec (" << us / 1000.0 << " ms)" << std::endl;
        EXPECT_EQ(v[0], 42);
    }
}

TEST(BenchAlgorithm, ReverseQPS_1M) {
    const int N = 1000000;
    zstl::vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = i;

    auto t1 = high_resolution_clock::now();
    zstl::reverse(v.begin(), v.end());
    auto t2 = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(t2 - t1).count();
    double qps = (double)N / (us / 1000000.0);
    std::cout << "[BENCH] zstl::reverse (1M):              "
              << fmt_qps(qps) << " ops/sec (" << us / 1000.0 << " ms)" << std::endl;
    EXPECT_EQ(v[0], N - 1);
    EXPECT_EQ(v[N - 1], 0);
}

// ============================================================================
// Rotate benchmark
// ============================================================================

TEST(BenchAlgorithm, RotateQPS_1M) {
    const int N = 1000000;
    zstl::vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = i;

    auto t1 = high_resolution_clock::now();
    // Rotate so the middle becomes the beginning
    auto new_begin = zstl::rotate(v.begin(), v.begin() + N / 2, v.end());
    auto t2 = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(t2 - t1).count();
    double qps = (double)N / (us / 1000000.0);
    std::cout << "[BENCH] zstl::rotate (1M):               "
              << fmt_qps(qps) << " ops/sec (" << us / 1000.0 << " ms)" << std::endl;
    EXPECT_EQ(new_begin, v.begin() + N - N / 2);
    EXPECT_EQ(v[0], N / 2);
}

// ============================================================================
// Multi-threaded algorithm benchmarks
// ============================================================================

TEST(BenchAlgorithm, ParallelReadOnlyAlgorithms) {
    // Test that read-only algorithms scale well with threads (no false sharing)
    const int N = 5000000;
    zstl::vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = i;

    for (int nthreads : {2, 4, 8}) {
        std::atomic<bool> start{false};
        std::atomic<long long> ops{0};
        std::vector<std::thread> threads;
        int per_thread = N / nthreads;

        auto t1 = high_resolution_clock::now();
        for (int t = 0; t < nthreads; ++t) {
            threads.emplace_back([&, t]() {
                while (!start.load(std::memory_order_acquire)) {}
                int base = t * per_thread;
                // Use lower_bound on each thread's segment
                for (int i = base; i < base + per_thread; ++i) {
                    auto it = zstl::lower_bound(v.begin(), v.end(), i);
                    if (it != v.end()) ops.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        start.store(true, std::memory_order_release);
        for (auto& t : threads) t.join();
        auto t2 = high_resolution_clock::now();

        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)ops.load() / (us / 1000000.0);
        std::cout << "[BENCH] lower_bound " << nthreads << "T parallel:     "
                  << fmt_qps(qps) << " ops/sec (" << ops.load() << " ops in "
                  << us / 1000.0 << " ms)" << std::endl;
    }
}
