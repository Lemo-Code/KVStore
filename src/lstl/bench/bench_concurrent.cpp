/**
 * @file    bench_concurrent.cpp
 * @brief   Concurrency benchmark: multi-threaded pool allocation.
 *
 * Measures thread safety and contention of default_alloc under
 * concurrent allocation/deallocation from multiple threads.
 */

#include <lstl/memory/pool.h>
#include <lstl/memory/alloc.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <thread>
#include <atomic>

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

// =========================================================================
// Worker: allocate n blocks, return them, repeat rounds times
// =========================================================================
void pool_worker(size_t n, size_t rounds, std::atomic<size_t>& ops) {
    for (size_t r = 0; r < rounds; ++r) {
        std::vector<void*> ptrs; ptrs.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            ptrs.push_back(lstl::default_alloc::allocate(64));
        }
        // Use the memory
        for (auto p : ptrs) {
            *static_cast<char*>(p) = 0x42;
        }
        for (auto p : ptrs) {
            lstl::default_alloc::deallocate(p, 64);
        }
        ops += n;
    }
}

void malloc_worker(size_t n, size_t rounds, std::atomic<size_t>& ops) {
    for (size_t r = 0; r < rounds; ++r) {
        std::vector<void*> ptrs; ptrs.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            ptrs.push_back(std::malloc(64));
        }
        for (auto p : ptrs) {
            *static_cast<char*>(p) = 0x42;
        }
        for (auto p : ptrs) {
            std::free(p);
        }
        ops += n;
    }
}

int main(int argc, char** argv) {
    size_t n = (argc > 1) ? size_t(atoll(argv[1])) : 100000;
    size_t num_threads = (argc > 2) ? size_t(atoll(argv[2])) : 4;
    size_t rounds = 10;

    printf("=== concurrent benchmark ===\n");
    printf("  blocks/thread: %zu\n", n);
    printf("  threads:       %zu\n", num_threads);
    printf("  rounds:        %zu\n\n", rounds);

    // ---- default_alloc (multi-threaded) ----
    {
        Timer t("default_alloc multi-threaded");
        std::atomic<size_t> ops{0};
        std::vector<std::thread> threads;
        for (size_t i = 0; i < num_threads; ++i) {
            threads.emplace_back(pool_worker, n, rounds, std::ref(ops));
        }
        for (auto& t : threads) t.join();
        printf("    total ops: %zu\n", ops.load());
    }

    // ---- malloc (multi-threaded) ----
    {
        Timer t("malloc       multi-threaded");
        std::atomic<size_t> ops{0};
        std::vector<std::thread> threads;
        for (size_t i = 0; i < num_threads; ++i) {
            threads.emplace_back(malloc_worker, n, rounds, std::ref(ops));
        }
        for (auto& t : threads) t.join();
        printf("    total ops: %zu\n", ops.load());
    }

    // =========================================================================
    // Contention test: many small allocations from many threads
    // =========================================================================
    printf("\n--- contention test (small blocks, many threads) ---\n");
    size_t small_n = n / 10;
    size_t many_threads = num_threads * 2;

    {
        Timer t("default_alloc contention ");
        std::atomic<size_t> ops{0};
        std::vector<std::thread> threads;
        for (size_t i = 0; i < many_threads; ++i) {
            threads.emplace_back(pool_worker, small_n, rounds, std::ref(ops));
        }
        for (auto& t : threads) t.join();
        printf("    total ops: %zu\n", ops.load());
    }

    printf("\n=== done ===\n");
    return 0;
}
