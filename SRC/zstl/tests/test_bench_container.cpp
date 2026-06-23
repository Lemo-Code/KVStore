// ============================================================================
// zstl container performance benchmarks
// ============================================================================
// Benchmarks: vector push_back/random-access/sort, list push_back/iteration,
//            deque push_front/back, map/unordered_map/bmap/skip_map insert/find,
//            string copy/append/find (SSO vs heap), all compared against
//            std:: equivalents where applicable.
// ============================================================================

#include <gtest/gtest.h>
#include "zstl/zstl.h"

#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <map>
#include <unordered_map>
#include <list>
#include <deque>
#include <string>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <random>

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
// Vector benchmarks
// ============================================================================

TEST(BenchContainer, VectorPushBack_Int_1M) {
    const int N = 5000000;

    // zstl::vector
    {
        zstl::vector<int> v;
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) v.push_back(i);
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] zstl::vector push_back (int):  " << fmt_qps(qps)
                  << " ops/sec (" << us / 1000.0 << " ms)" << std::endl;
        EXPECT_GT(qps, 0);
    }

    // std::vector
    {
        std::vector<int> v;
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) v.push_back(i);
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] std::vector  push_back (int):  " << fmt_qps(qps)
                  << " ops/sec (" << us / 1000.0 << " ms)" << std::endl;
        EXPECT_GT(qps, 0);
    }
}

TEST(BenchContainer, VectorPushBack_POD_Struct) {
    struct Pod64 { int64_t a; int64_t b; int64_t c; int64_t d;
                   int64_t e; int64_t f; int64_t g; int64_t h; };
    const int N = 2000000;

    // zstl::vector
    {
        zstl::vector<Pod64> v;
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) v.push_back(Pod64{});
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        double mbps = (qps * sizeof(Pod64)) / (1024.0 * 1024.0);
        std::cout << "[BENCH] zstl::vector push_back (64B POD): " << fmt_qps(qps)
                  << " ops/sec, " << std::setprecision(1) << mbps << " MB/s" << std::endl;
        EXPECT_GT(qps, 0);
    }

    // std::vector
    {
        std::vector<Pod64> v;
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) v.push_back(Pod64{});
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        double mbps = (qps * sizeof(Pod64)) / (1024.0 * 1024.0);
        std::cout << "[BENCH] std::vector  push_back (64B POD): " << fmt_qps(qps)
                  << " ops/sec, " << std::setprecision(1) << mbps << " MB/s" << std::endl;
        EXPECT_GT(qps, 0);
    }
}

TEST(BenchContainer, VectorPreallocatedPushBack) {
    const int N = 5000000;

    // zstl::vector with reserve
    {
        zstl::vector<int> v;
        v.reserve(N);
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) v.push_back(i);
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] zstl::vector push_back reserved:   " << fmt_qps(qps)
                  << " ops/sec" << std::endl;
        EXPECT_GT(qps, 0);
    }

    // std::vector with reserve
    {
        std::vector<int> v;
        v.reserve(N);
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) v.push_back(i);
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] std::vector  push_back reserved:   " << fmt_qps(qps)
                  << " ops/sec" << std::endl;
        EXPECT_GT(qps, 0);
    }
}

TEST(BenchContainer, VectorRandomAccess) {
    const int N = 10000000;
    zstl::vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = i;

    // Sequential read
    {
        volatile long long sum = 0;
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) sum += v[i];
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] zstl::vector sequential read:  " << fmt_qps(qps)
                  << " reads/sec" << std::endl;
        EXPECT_GT(sum, 0);
    }

    // Random access (fixed stride)
    {
        volatile long long sum = 0;
        const int stride = 10007;  // prime stride to avoid prefetch bias
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) sum += v[(i * stride) % N];
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] zstl::vector random access:     " << fmt_qps(qps)
                  << " reads/sec" << std::endl;
        EXPECT_GT(sum, 0);
    }

    // std::vector comparison
    {
        std::vector<int> sv(N);
        for (int i = 0; i < N; ++i) sv[i] = i;
        volatile long long sum = 0;
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) sum += sv[i];
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] std::vector  sequential read:  " << fmt_qps(qps)
                  << " reads/sec" << std::endl;
        EXPECT_GT(sum, 0);
    }
}

TEST(BenchContainer, VectorSort_Int_1M) {
    const int N = 1000000;
    std::mt19937 rng(42);

    // zstl::vector + zstl::sort
    {
        zstl::vector<int> v(N);
        for (int i = 0; i < N; ++i) v[i] = rng();
        auto t1 = high_resolution_clock::now();
        zstl::sort(v.begin(), v.end());
        auto t2 = high_resolution_clock::now();
        auto ms = duration_cast<milliseconds>(t2 - t1).count();
        std::cout << "[BENCH] zstl::vector + zstl::sort 1M:  " << ms
                  << " ms" << std::endl;
        EXPECT_TRUE(zstl::is_sorted(v.begin(), v.end()));
    }

    // std::vector + std::sort
    {
        std::vector<int> v(N);
        for (int i = 0; i < N; ++i) v[i] = rng();
        auto t1 = high_resolution_clock::now();
        std::sort(v.begin(), v.end());
        auto t2 = high_resolution_clock::now();
        auto ms = duration_cast<milliseconds>(t2 - t1).count();
        std::cout << "[BENCH] std::vector  + std::sort  1M:  " << ms
                  << " ms" << std::endl;
        EXPECT_TRUE(std::is_sorted(v.begin(), v.end()));
    }
}

TEST(BenchContainer, VectorIteration) {
    const int N = 10000000;
    zstl::vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = i;

    // Iterator-based
    {
        volatile long long sum = 0;
        auto t1 = high_resolution_clock::now();
        for (auto it = v.begin(); it != v.end(); ++it) sum += *it;
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] zstl::vector iterator traversal: " << fmt_qps(qps)
                  << " elements/sec" << std::endl;
        EXPECT_GT(sum, 0);
    }

    // Index-based
    {
        volatile long long sum = 0;
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) sum += v[i];
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] zstl::vector index traversal:    " << fmt_qps(qps)
                  << " elements/sec" << std::endl;
        EXPECT_GT(sum, 0);
    }
}

// ============================================================================
// List benchmarks
// ============================================================================

TEST(BenchContainer, ListPushBack_Int_1M) {
    const int N = 2000000;

    {
        zstl::list<int> lst;
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) lst.push_back(i);
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] zstl::list push_back:          " << fmt_qps(qps)
                  << " ops/sec (" << N << " in " << us / 1000.0 << " ms)" << std::endl;
        EXPECT_GT(qps, 0);
    }

    {
        std::list<int> lst;
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) lst.push_back(i);
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] std::list  push_back:          " << fmt_qps(qps)
                  << " ops/sec (" << N << " in " << us / 1000.0 << " ms)" << std::endl;
        EXPECT_GT(qps, 0);
    }
}

TEST(BenchContainer, ListPushFront_Int_1M) {
    const int N = 2000000;

    {
        zstl::list<int> lst;
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) lst.push_front(i);
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] zstl::list push_front:         " << fmt_qps(qps)
                  << " ops/sec" << std::endl;
        EXPECT_GT(qps, 0);
    }

    {
        std::list<int> lst;
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) lst.push_front(i);
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] std::list  push_front:         " << fmt_qps(qps)
                  << " ops/sec" << std::endl;
        EXPECT_GT(qps, 0);
    }
}

TEST(BenchContainer, ListIteration) {
    const int N = 5000000;
    {
        zstl::list<int> lst;
        for (int i = 0; i < N; ++i) lst.push_back(i);

        volatile long long sum = 0;
        auto t1 = high_resolution_clock::now();
        for (auto it = lst.begin(); it != lst.end(); ++it) sum += *it;
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] zstl::list iteration:          " << fmt_qps(qps)
                  << " elements/sec" << std::endl;
        EXPECT_GT(sum, 0);
    }

    {
        std::list<int> lst;
        for (int i = 0; i < N; ++i) lst.push_back(i);
        volatile long long sum = 0;
        auto t1 = high_resolution_clock::now();
        for (auto it = lst.begin(); it != lst.end(); ++it) sum += *it;
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] std::list  iteration:          " << fmt_qps(qps)
                  << " elements/sec" << std::endl;
        EXPECT_GT(sum, 0);
    }
}

// ============================================================================
// Deque benchmarks
// ============================================================================

TEST(BenchContainer, DequePushFrontBack_Alternating) {
    const int N = 2000000;

    {
        zstl::deque<int> dq;
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            if (i % 2 == 0) dq.push_back(i);
            else dq.push_front(i);
        }
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] zstl::deque push front+back:   " << fmt_qps(qps)
                  << " ops/sec" << std::endl;
        EXPECT_GT(qps, 0);
    }

    {
        std::deque<int> dq;
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            if (i % 2 == 0) dq.push_back(i);
            else dq.push_front(i);
        }
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] std::deque  push front+back:   " << fmt_qps(qps)
                  << " ops/sec" << std::endl;
        EXPECT_GT(qps, 0);
    }
}

TEST(BenchContainer, DequeRandomAccess) {
    const int N = 5000000;
    zstl::deque<int> dq;
    for (int i = 0; i < N; ++i) dq.push_back(i);

    volatile long long sum = 0;
    auto t1 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) sum += dq[i];
    auto t2 = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(t2 - t1).count();
    double qps = (double)N / (us / 1000000.0);
    std::cout << "[BENCH] zstl::deque random access:       " << fmt_qps(qps)
              << " reads/sec" << std::endl;
    EXPECT_GT(sum, 0);
}

// ============================================================================
// Map insert benchmarks — all backends vs std::
// ============================================================================

TEST(BenchContainer, MapInsert_1M_AllBackends) {
    const int N = 1000000;
    std::mt19937 rng(42);
    std::vector<int> keys(N);
    for (int i = 0; i < N; ++i) keys[i] = i;
    std::shuffle(keys.begin(), keys.end(), rng);

    std::cout << "\n[BENCH] Map Insert QPS (" << N << " keys):" << std::endl;
    std::cout << "  " << std::setw(18) << "Backend" << std::setw(12) << "Time(ms)"
              << std::setw(14) << "QPS" << std::endl;
    std::cout << "  " << std::string(44, '-') << std::endl;

    auto bench_insert = [&](auto& m, const std::string& name) {
        auto t1 = high_resolution_clock::now();
        for (auto k : keys) m.insert({k, k * 2});
        auto t2 = high_resolution_clock::now();
        auto ms = duration_cast<milliseconds>(t2 - t1).count();
        double qps = (double)N / (ms / 1000.0);
        std::cout << "  " << std::setw(16) << name << std::setw(10) << ms
                  << " " << std::setw(12) << std::fixed << std::setprecision(0)
                  << qps << std::endl;
    };

    {
        zstl::map<int, int> m;
        bench_insert(m, "zstl::map");
    }
    {
        zstl::unordered_map<int, int> m(1024);
        bench_insert(m, "zstl::unordered_map");
    }
    {
        zstl::bmap<int, int> m;
        bench_insert(m, "zstl::bmap");
    }
    {
        zstl::skip_map<int, int> m;
        bench_insert(m, "zstl::skip_map");
    }
    {
        std::map<int, int> m;
        bench_insert(m, "std::map");
    }
    {
        std::unordered_map<int, int> m(1024);
        bench_insert(m, "std::unordered_map");
    }
}

// ============================================================================
// Map find benchmarks — all backends vs std::
// ============================================================================

TEST(BenchContainer, MapFind_1M_AllBackends) {
    const int N = 1000000;
    std::mt19937 rng(42);
    std::vector<int> keys(N);
    for (int i = 0; i < N; ++i) keys[i] = i;
    std::shuffle(keys.begin(), keys.end(), rng);

    // Pre-build all maps
    zstl::map<int, int>           zmap;
    zstl::unordered_map<int, int> zumap(1024);
    zstl::bmap<int, int>          zbmap;
    zstl::skip_map<int, int>      zskip;
    std::map<int, int>            smap;
    std::unordered_map<int, int>  sumap(1024);

    for (int i = 0; i < N; ++i) {
        zmap.insert({i, i});
        zumap.insert({i, i});
        zbmap.insert({i, i});
        zskip.insert({i, i});
        smap.insert({i, i});
        sumap.insert({i, i});
    }

    // Shuffle find keys
    std::vector<int> find_keys(N);
    for (int i = 0; i < N; ++i) find_keys[i] = rng() % N;

    std::cout << "\n[BENCH] Map Find QPS (" << N << " lookups):" << std::endl;
    std::cout << "  " << std::setw(18) << "Backend" << std::setw(12) << "Time(ms)"
              << std::setw(14) << "QPS" << std::endl;
    std::cout << "  " << std::string(44, '-') << std::endl;

    auto bench_find = [&](auto& m, const std::string& name) {
        volatile long long sum = 0;
        auto t1 = high_resolution_clock::now();
        for (auto k : find_keys) {
            auto it = m.find(k);
            if (it != m.end()) sum += it->second;
        }
        auto t2 = high_resolution_clock::now();
        auto ms = duration_cast<milliseconds>(t2 - t1).count();
        double qps = (double)N / (ms / 1000.0);
        std::cout << "  " << std::setw(16) << name << std::setw(10) << ms
                  << " " << std::setw(12) << std::fixed << std::setprecision(0)
                  << qps << std::endl;
        EXPECT_GT(sum, 0);
    };

    bench_find(zmap,  "zstl::map");
    bench_find(zumap, "zstl::unordered_map");
    bench_find(zbmap, "zstl::bmap");
    bench_find(zskip, "zstl::skip_map");
    bench_find(smap,  "std::map");
    bench_find(sumap, "std::unordered_map");
}

// ============================================================================
// Map erase benchmarks
// ============================================================================

TEST(BenchContainer, MapErase_1M) {
    const int N = 500000;
    std::mt19937 rng(42);
    std::vector<int> keys(N);
    for (int i = 0; i < N; ++i) keys[i] = i;
    std::shuffle(keys.begin(), keys.end(), rng);

    std::cout << "\n[BENCH] Map Erase QPS (" << N << " erasures):" << std::endl;
    std::cout << "  " << std::setw(18) << "Backend" << std::setw(12) << "Time(ms)"
              << std::setw(14) << "QPS" << std::endl;
    std::cout << "  " << std::string(44, '-') << std::endl;

    auto bench_erase = [&](auto make_map, const std::string& name) {
        auto m = make_map();
        for (auto k : keys) m.insert({k, k});
        std::shuffle(keys.begin(), keys.end(), rng);
        auto t1 = high_resolution_clock::now();
        for (auto k : keys) m.erase(k);
        auto t2 = high_resolution_clock::now();
        auto ms = duration_cast<milliseconds>(t2 - t1).count();
        double qps = (double)N / (ms / 1000.0);
        std::cout << "  " << std::setw(16) << name << std::setw(10) << ms
                  << " " << std::setw(12) << std::fixed << std::setprecision(0)
                  << qps << std::endl;
    };

    bench_erase([]() { return zstl::map<int, int>(); },           "zstl::map");
    bench_erase([]() { return zstl::unordered_map<int, int>(1024); }, "zstl::unordered_map");
    bench_erase([]() { return zstl::bmap<int, int>(); },          "zstl::bmap");
    bench_erase([]() { return zstl::skip_map<int, int>(); },      "zstl::skip_map");
    bench_erase([]() { return std::map<int, int>(); },            "std::map");
    bench_erase([]() { return std::unordered_map<int, int>(1024); }, "std::unordered_map");
}

// ============================================================================
// String benchmarks (SSO vs heap)
// ============================================================================

TEST(BenchContainer, StringCopyQPS) {
    const int N = 1000000;

    // SSO (5 chars): fits in stack buffer
    {
        zstl::string s = "hello";
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            volatile zstl::string copy = s;  // prevent optimization
        }
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] zstl::string copy (SSO 5B):   " << fmt_qps(qps)
                  << " copies/sec" << std::endl;
    }
    {
        std::string s = "hello";
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            volatile std::string copy = s;
        }
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] std::string  copy (SSO 5B):   " << fmt_qps(qps)
                  << " copies/sec" << std::endl;
    }

    // Medium (50 chars): heap with small allocation
    {
        zstl::string s = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWX";
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            volatile zstl::string copy = s;
        }
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] zstl::string copy (heap 50B):  " << fmt_qps(qps)
                  << " copies/sec" << std::endl;
    }
    {
        std::string s = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWX";
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) {
            volatile std::string copy = s;
        }
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] std::string  copy (heap 50B):  " << fmt_qps(qps)
                  << " copies/sec" << std::endl;
    }

    // Large (500 chars)
    {
        zstl::string s(500, 'x');
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N / 2; ++i) {
            volatile zstl::string copy = s;
        }
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)(N / 2) / (us / 1000000.0);
        std::cout << "[BENCH] zstl::string copy (heap 500B): " << fmt_qps(qps)
                  << " copies/sec" << std::endl;
    }
    {
        std::string s(500, 'x');
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N / 2; ++i) {
            volatile std::string copy = s;
        }
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)(N / 2) / (us / 1000000.0);
        std::cout << "[BENCH] std::string  copy (heap 500B): " << fmt_qps(qps)
                  << " copies/sec" << std::endl;
    }
}

TEST(BenchContainer, StringAppendQPS) {
    const int N = 500000;

    // zstl::string append
    {
        zstl::string s;
        // Pre-size to avoid realloc during test
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) s.append(".");
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] zstl::string append (1 char):   " << fmt_qps(qps)
                  << " appends/sec (final size=" << s.size() << ")" << std::endl;
    }

    // std::string append
    {
        std::string s;
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) s.append(".");
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] std::string  append (1 char):   " << fmt_qps(qps)
                  << " appends/sec (final size=" << s.size() << ")" << std::endl;
    }
}

TEST(BenchContainer, StringFindQPS) {
    const int N = 500000;
    zstl::string haystack(10000, 'a');
    haystack[5000] = 'X';  // place target in the middle

    // zstl::string find
    {
        volatile size_t pos = 0;
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) pos = haystack.find('X');
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] zstl::string find (10KB str):   " << fmt_qps(qps)
                  << " finds/sec" << std::endl;
        EXPECT_EQ(pos, 5000u);
    }

    // std::string find
    {
        std::string haystack(10000, 'a');
        haystack[5000] = 'X';
        volatile size_t pos = 0;
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) pos = haystack.find('X');
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)N / (us / 1000000.0);
        std::cout << "[BENCH] std::string  find (10KB str):   " << fmt_qps(qps)
                  << " finds/sec" << std::endl;
        EXPECT_EQ(pos, 5000u);
    }
}

// ============================================================================
// Multi-threaded vector read benchmark
// ============================================================================

TEST(BenchContainer, VectorMultiThreadedRead) {
    const int N = 5000000;
    zstl::vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = i;

    for (int nthreads : {2, 4, 8}) {
        std::atomic<bool> start{false};
        std::atomic<long long> sum{0};
        std::vector<std::thread> threads;
        int per_thread = N / nthreads;

        auto t1 = high_resolution_clock::now();
        for (int t = 0; t < nthreads; ++t) {
            threads.emplace_back([&, t]() {
                while (!start.load(std::memory_order_acquire)) {}
                int base = t * per_thread;
                long long local = 0;
                for (int i = base; i < base + per_thread; ++i) local += v[i];
                sum.fetch_add(local, std::memory_order_relaxed);
            });
        }
        start.store(true, std::memory_order_release);
        for (auto& t : threads) t.join();
        auto t2 = high_resolution_clock::now();

        auto us = duration_cast<microseconds>(t2 - t1).count();
        double qps = (double)per_thread * nthreads / (us / 1000000.0);
        double throughput = (qps * sizeof(int)) / (1024.0 * 1024.0);
        std::cout << "[BENCH] zstl::vector " << nthreads << "T read:      " << fmt_qps(qps)
                  << " reads/sec, " << std::setprecision(1) << throughput << " MB/s" << std::endl;
    }
}

// ============================================================================
// Throughput benchmark: vector bulk insert with preallocation
// ============================================================================

TEST(BenchContainer, VectorBulkThroughput) {
    const int N = 10000000;

    {
        zstl::vector<int> v;
        v.reserve(N);
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) v.push_back(i);
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double mbps = ((double)N * sizeof(int)) / (us / 1000000.0) / (1024.0 * 1024.0);
        std::cout << "[BENCH] zstl::vector bulk insert 10M:    "
                  << std::fixed << std::setprecision(1) << mbps << " MB/s ("
                  << us / 1000.0 << " ms)" << std::endl;
        EXPECT_GT(mbps, 0);
    }

    {
        std::vector<int> v;
        v.reserve(N);
        auto t1 = high_resolution_clock::now();
        for (int i = 0; i < N; ++i) v.push_back(i);
        auto t2 = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(t2 - t1).count();
        double mbps = ((double)N * sizeof(int)) / (us / 1000000.0) / (1024.0 * 1024.0);
        std::cout << "[BENCH] std::vector  bulk insert 10M:    "
                  << std::fixed << std::setprecision(1) << mbps << " MB/s ("
                  << us / 1000.0 << " ms)" << std::endl;
        EXPECT_GT(mbps, 0);
    }
}
