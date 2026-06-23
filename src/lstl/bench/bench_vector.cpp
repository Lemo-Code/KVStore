/**
 * @file    bench_vector.cpp
 * @brief   Performance benchmark: lstl::vector vs std::vector.
 *
 * Measures: push_back throughput, random access, iteration, insert/erase.
 * Run with: bench_vector [iterations]
 */

#include <lstl/container/vector.h>
#include <vector>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace std::chrono;

static const size_t DEFAULT_N = 1000000;

struct Timer {
    high_resolution_clock::time_point start;
    const char* name;
    Timer(const char* n) : start(high_resolution_clock::now()), name(n) {}
    ~Timer() {
        auto end = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(end - start).count();
        printf("  %-40s %10ld us\n", name, us);
    }
};

#define RUN(expr) do { Timer _t(__FUNCTION__ ":" #expr); (void)(expr); } while(0)

// =========================================================================
// Test 1: push_back (sequential, with reserve)
// =========================================================================
template<typename Vec>
void bench_push_back_reserved(size_t n, const char* label) {
    Timer t(label);
    Vec v; v.reserve(n);
    for (size_t i = 0; i < n; ++i) v.push_back(static_cast<int>(i));
    volatile int sink = v[0]; (void)sink;
}

// =========================================================================
// Test 2: push_back (sequential, without reserve)
// =========================================================================
template<typename Vec>
void bench_push_back_no_reserve(size_t n, const char* label) {
    Timer t(label);
    Vec v;
    for (size_t i = 0; i < n; ++i) v.push_back(static_cast<int>(i));
    volatile int sink = v[0]; (void)sink;
}

// =========================================================================
// Test 3: random access (read)
// =========================================================================
template<typename Vec>
void bench_random_read(const Vec& v, size_t n, const char* label) {
    Timer t(label);
    volatile int sum = 0;
    for (size_t i = 0; i < n; ++i) {
        sum += v[i % v.size()];
    }
}

// =========================================================================
// Test 4: iteration
// =========================================================================
template<typename Vec>
void bench_iteration(const Vec& v, const char* label) {
    Timer t(label);
    volatile int sum = 0;
    for (auto& x : v) sum += x;
}

// =========================================================================
// Test 5: insert at front (worst case)
// =========================================================================
template<typename Vec>
void bench_insert_front(Vec& v, size_t n, const char* label) {
    Timer t(label);
    for (size_t i = 0; i < n; ++i) {
        v.insert(v.begin(), static_cast<int>(i));
    }
}

int main(int argc, char** argv) {
    size_t n = (argc > 1) ? size_t(atoll(argv[1])) : DEFAULT_N;
    printf("=== vector benchmark (n=%zu) ===\n\n", n);

    // ---- push_back with reserve ----
    printf("--- push_back with reserve ---\n");
    bench_push_back_reserved<lstl::vector<int>>(n, "lstl::vector");
    bench_push_back_reserved<std::vector<int>>(n, "std::vector ");

    // ---- push_back without reserve ----
    printf("\n--- push_back without reserve ---\n");
    bench_push_back_no_reserve<lstl::vector<int>>(n, "lstl::vector");
    bench_push_back_no_reserve<std::vector<int>>(n, "std::vector ");

    // ---- pre-built vectors for read tests ----
    lstl::vector<int> lv; lv.reserve(n);
    std::vector<int> sv; sv.reserve(n);
    for (size_t i = 0; i < n; ++i) { lv.push_back(i); sv.push_back(i); }

    // ---- random access read ----
    printf("\n--- random access read (n=%zu) ---\n", n);
    bench_random_read(lv, n, "lstl::vector");
    bench_random_read(sv, n, "std::vector ");

    // ---- iteration ----
    printf("\n--- iteration ---\n");
    bench_iteration(lv, "lstl::vector");
    bench_iteration(sv, "std::vector ");

    // ---- insert at front (smaller n) ----
    size_t n_insert = n > 10000 ? 10000 : n;
    printf("\n--- insert at front (n=%zu) ---\n", n_insert);
    {
        lstl::vector<int> lv2;
        bench_insert_front(lv2, n_insert, "lstl::vector");
    }
    {
        std::vector<int> sv2;
        bench_insert_front(sv2, n_insert, "std::vector ");
    }

    printf("\n=== done ===\n");
    return 0;
}
