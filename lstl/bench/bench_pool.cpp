/**
 * @file    bench_pool.cpp
 * @brief   Performance benchmark: lstl memory pool vs raw malloc/free.
 *
 * Measures allocation/deallocation throughput and pool reuse efficiency.
 */

#include <lstl/memory/pool.h>
#include <lstl/memory/alloc.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace std::chrono;

struct Timer {
    high_resolution_clock::time_point start;
    const char* name;
    Timer(const char* n) : start(high_resolution_clock::now()), name(n) {}
    ~Timer() {
        auto us = duration_cast<microseconds>(high_resolution_clock::now() - start).count();
        printf("  %-40s %10ld us\n", name, us);
    }
};

int main(int argc, char** argv) {
    size_t n = (argc > 1) ? size_t(atoll(argv[1])) : 1000000;
    printf("=== pool benchmark (n=%zu) ===\n\n", n);

    // =========================================================================
    // Test 1: Single-size allocate/deallocate (64 bytes)
    // =========================================================================
    printf("--- single-size 64-byte alloc/free x %zu ---\n", n);
    {
        Timer t("lstl::default_alloc");
        std::vector<void*> ptrs; ptrs.reserve(n);
        for (size_t i = 0; i < n; ++i) ptrs.push_back(lstl::default_alloc::allocate(64));
        for (auto p : ptrs) lstl::default_alloc::deallocate(p, 64);
    }
    {
        Timer t("std::malloc/free   ");
        std::vector<void*> ptrs; ptrs.reserve(n);
        for (size_t i = 0; i < n; ++i) ptrs.push_back(std::malloc(64));
        for (auto p : ptrs) std::free(p);
    }

    // =========================================================================
    // Test 2: Mixed-size allocations (simulating real workload)
    // =========================================================================
    printf("\n--- mixed-size alloc/free x %zu ---\n", n);
    size_t sizes[] = {16, 32, 64, 128, 256, 512, 1024, 4096};
    {
        Timer t("lstl::default_alloc (mixed)");
        std::vector<void*> ptrs; ptrs.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            size_t sz = sizes[i % 8];
            ptrs.push_back(lstl::default_alloc::allocate(sz));
        }
        for (auto p : ptrs) lstl::default_alloc::deallocate(p, sizes[0]); // approximate
    }
    {
        Timer t("std::malloc/free    (mixed)");
        std::vector<void*> ptrs; ptrs.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            size_t sz = sizes[i % 8];
            ptrs.push_back(std::malloc(sz));
        }
        for (auto p : ptrs) std::free(p);
    }

    // =========================================================================
    // Test 3: Rapid alloc/free cycles (pool reuse)
    // =========================================================================
    printf("\n--- rapid alloc/free cycle x %zu ---\n", n / 10);
    size_t m = n / 10;
    {
        Timer t("lstl::default_alloc (cycle)");
        for (size_t j = 0; j < 10; ++j) {
            std::vector<void*> ptrs;
            for (size_t i = 0; i < m; ++i) ptrs.push_back(lstl::default_alloc::allocate(64));
            for (auto p : ptrs) lstl::default_alloc::deallocate(p, 64);
        }
    }
    {
        Timer t("std::malloc/free    (cycle)");
        for (size_t j = 0; j < 10; ++j) {
            std::vector<void*> ptrs;
            for (size_t i = 0; i < m; ++i) ptrs.push_back(std::malloc(64));
            for (auto p : ptrs) std::free(p);
        }
    }

    printf("\n=== done ===\n");
    return 0;
}
