/**
 * stress_lstl_bench.cpp — LSTL vs STL 性能矩阵
 *
 * 横轴: 1, 2, 4, … 2×CPU 核心
 * 纵轴: lstl vs std 对比项
 * 单位: ops/s (QPS)
 *
 * Usage: stress_lstl_bench [output_dir]
 */
#include "bench_utils.h"
#include "matrix.h"
using namespace stress;

#include <lstl/container/vector.h>
#include <lstl/container/unordered_map.h>
#include <lstl/memory/pool.h>
#include <lstl/memory/alloc.h>

#include <vector>
#include <unordered_map>
#include <cstdio>
#include <cstdlib>

static volatile int g_sink = 0;

static void vecLstl(int, std::atomic<uint64_t>& ops, size_t n) {
    lstl::vector<int> v;
    for (size_t i = 0; i < n; ++i) v.push_back((int)i);
    g_sink += (int)v.size();
    ops.fetch_add(n, std::memory_order_relaxed);
}
static void vecStd(int, std::atomic<uint64_t>& ops, size_t n) {
    std::vector<int> v;
    for (size_t i = 0; i < n; ++i) v.push_back((int)i);
    g_sink += (int)v.size();
    ops.fetch_add(n, std::memory_order_relaxed);
}

static void mapLstl(int, std::atomic<uint64_t>& ops, size_t n) {
    lstl::unordered_map<int, int> m;
    for (size_t i = 0; i < n; ++i) m.insert({(int)i, (int)i});
    uint64_t local = n;
    for (size_t i = 0; i < n; ++i) local += (m.find((int)i) != m.end());
    g_sink += (int)m.size();
    ops.fetch_add(local, std::memory_order_relaxed);
}
static void mapStd(int, std::atomic<uint64_t>& ops, size_t n) {
    std::unordered_map<int, int> m;
    for (size_t i = 0; i < n; ++i) m.insert({(int)i, (int)i});
    uint64_t local = n;
    for (size_t i = 0; i < n; ++i) local += (m.find((int)i) != m.end());
    g_sink += (int)m.size();
    ops.fetch_add(local, std::memory_order_relaxed);
}

static void poolLstl(int, std::atomic<uint64_t>& ops, size_t n) {
    lstl::pool_single pool(64, 256);
    std::vector<void*> ptrs;
    ptrs.reserve(n);
    for (size_t i = 0; i < n; ++i) ptrs.push_back(pool.allocate());
    for (auto p : ptrs) pool.deallocate(p);
    ops.fetch_add(n, std::memory_order_relaxed);
}
static void poolMalloc(int, std::atomic<uint64_t>& ops, size_t n) {
    std::vector<void*> ptrs;
    ptrs.reserve(n);
    for (size_t i = 0; i < n; ++i) ptrs.push_back(std::malloc(64));
    for (auto p : ptrs) std::free(p);
    ops.fetch_add(n, std::memory_order_relaxed);
}

int main(int argc, char** argv) {
    std::string out_dir = resolveBenchOutDir(argc, argv);
    ensureDir(out_dir);
    printf("输出目录: %s\n\n", out_dir.c_str());

    const size_t vec_n  = 500000;
    const size_t map_n  = 125000;
    const size_t pool_n = 500000;

    MatrixRunner runner("LSTL vs STL 性能对比", "ops/s (QPS)");

    runner.addRow("vector lstl", [=](int th, uint64_t& ops, double& sec) {
        runMultiThread(th, [=](int tid, std::atomic<uint64_t>& c){ vecLstl(tid,c,vec_n); }, ops, sec);
    });
    runner.addRow("vector std ", [=](int th, uint64_t& ops, double& sec) {
        runMultiThread(th, [=](int tid, std::atomic<uint64_t>& c){ vecStd(tid,c,vec_n); }, ops, sec);
    });
    runner.addRow("map lstl   ", [=](int th, uint64_t& ops, double& sec) {
        runMultiThread(th, [=](int tid, std::atomic<uint64_t>& c){ mapLstl(tid,c,map_n); }, ops, sec);
    });
    runner.addRow("map std    ", [=](int th, uint64_t& ops, double& sec) {
        runMultiThread(th, [=](int tid, std::atomic<uint64_t>& c){ mapStd(tid,c,map_n); }, ops, sec);
    });
    runner.addRow("pool lstl  ", [=](int th, uint64_t& ops, double& sec) {
        runMultiThread(th, [=](int tid, std::atomic<uint64_t>& c){ poolLstl(tid,c,pool_n); }, ops, sec);
    });
    runner.addRow("pool malloc", [=](int th, uint64_t& ops, double& sec) {
        runMultiThread(th, [=](int tid, std::atomic<uint64_t>& c){ poolMalloc(tid,c,pool_n); }, ops, sec);
    });

    runner.run();
    runner.printMatrix();
    runner.saveLog(out_dir + "/lstl_compare.log");
    return 0;
}
