/**
 * @file    bench_map.cpp
 * @brief   Performance benchmark: lstl::map vs std::map.
 *
 * Measures: insert, find (hit/miss), erase, iteration.
 */

#include <lstl/container/map.h>
#include <map>
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
    size_t n = (argc > 1) ? size_t(atoll(argv[1])) : 500000;
    printf("=== map benchmark (n=%zu) ===\n\n", n);

    // Pre-generate keys (shuffled)
    std::vector<int> keys(n);
    for (size_t i = 0; i < n; ++i) keys[i] = static_cast<int>(i);

    // ---- insert ----
    printf("--- insert %zu elements ---\n", n);
    {
        Timer t("lstl::map insert");
        lstl::map<int, int> m;
        for (size_t i = 0; i < n; ++i) m.insert(lstl::make_pair(keys[i], static_cast<int>(i)));
        volatile int sink = m.size(); (void)sink;
    }
    {
        Timer t("std::map  insert");
        std::map<int, int> m;
        for (size_t i = 0; i < n; ++i) m.insert(std::make_pair(keys[i], static_cast<int>(i)));
        volatile int sink = m.size(); (void)sink;
    }

    // Build maps for lookup tests
    lstl::map<int, int> lm;
    std::map<int, int> sm;
    for (size_t i = 0; i < n; ++i) {
        lm.insert(lstl::make_pair(keys[i], static_cast<int>(i)));
        sm.insert(std::make_pair(keys[i], static_cast<int>(i)));
    }

    // ---- find (hit) ----
    printf("\n--- find (hit) %zu lookups ---\n", n);
    {
        Timer t("lstl::map find-hit");
        volatile int sum = 0;
        for (size_t i = 0; i < n; ++i) {
            auto it = lm.find(static_cast<int>(i));
            if (it != lm.end()) sum += it->second;
        }
    }
    {
        Timer t("std::map  find-hit");
        volatile int sum = 0;
        for (size_t i = 0; i < n; ++i) {
            auto it = sm.find(static_cast<int>(i));
            if (it != sm.end()) sum += it->second;
        }
    }

    // ---- find (miss) ----
    printf("\n--- find (miss) %zu lookups ---\n", n);
    {
        Timer t("lstl::map find-miss");
        for (size_t i = 0; i < n; ++i) lm.find(static_cast<int>(n + i));
    }
    {
        Timer t("std::map  find-miss");
        for (size_t i = 0; i < n; ++i) sm.find(static_cast<int>(n + i));
    }

    // ---- iteration ----
    printf("\n--- iteration ---\n");
    {
        Timer t("lstl::map iterate");
        volatile int sum = 0;
        for (auto& p : lm) sum += p.second;
    }
    {
        Timer t("std::map  iterate");
        volatile int sum = 0;
        for (auto& p : sm) sum += p.second;
    }

    // ---- erase half ----
    printf("\n--- erase half (%zu) ---\n", n/2);
    {
        Timer t("lstl::map erase");
        for (size_t i = 0; i < n/2; ++i) lm.erase(static_cast<int>(i * 2));
    }
    {
        Timer t("std::map  erase");
        for (size_t i = 0; i < n/2; ++i) sm.erase(static_cast<int>(i * 2));
    }

    printf("\n=== done ===\n");
    return 0;
}
