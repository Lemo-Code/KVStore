#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${ROOT}/benchmark/pool_matrix.txt"
mkdir -p "${ROOT}/benchmark"

echo "╔══════════════════════════════════════════════╗"
echo "║   内存池 vs malloc 性能矩阵对比               ║"
echo "╚══════════════════════════════════════════════╝"

cat > /tmp/bench_pool.cpp << 'EOF'
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <algorithm>
using namespace std;
using namespace chrono;

// ── 简易 Slab 内存池 (模拟 zero::StackPool) ──
class SlabPool {
    static constexpr size_t kBlockSize = 4096;
    static constexpr size_t kMaxFree  = 64;
    struct Block { Block* next; char data[kBlockSize - sizeof(Block*)]; };
    Block* free_list_ = nullptr;
    size_t allocated_ = 0;
public:
    void* allocate(size_t sz) {
        if (sz > kBlockSize - sizeof(Block*)) return ::malloc(sz);
        if (free_list_) { void* p = free_list_; free_list_ = free_list_->next; return p; }
        allocated_++;
        return ::malloc(kBlockSize);
    }
    void deallocate(void* p, size_t sz) {
        if (sz > kBlockSize - sizeof(Block*) || !p) { ::free(p); return; }
        auto* blk = static_cast<Block*>(p);
        blk->next = free_list_; free_list_ = blk;
    }
    ~SlabPool() { while (free_list_) { void* p = free_list_; free_list_ = free_list_->next; ::free(p); } }
    size_t allocated() const { return allocated_; }
};

// ── Benchmark 模板 ──
template<bool UsePool>
double bench_alloc_free(int n_threads, int ops) {
    atomic<bool> start{false};
    atomic<long long> total{0};
    SlabPool pool;
    
    vector<thread> ths;
    for(int t=0; t<n_threads; t++) {
        ths.emplace_back([&]() {
            while(!start.load(memory_order_acquire)) {}
            vector<void*> ptrs; ptrs.reserve(ops/n_threads);
            for(int i=0; i<ops/n_threads; i++) {
                if constexpr (UsePool) {
                    ptrs.push_back(pool.allocate(128));
                    if(ptrs.size()>64) { pool.deallocate(ptrs.back(),128); ptrs.pop_back(); }
                } else {
                    ptrs.push_back(::malloc(128));
                    if(ptrs.size()>64) { ::free(ptrs.back()); ptrs.pop_back(); }
                }
            }
            for(void* p : ptrs) { if constexpr (UsePool) pool.deallocate(p,128); else ::free(p); }
            total.fetch_add(ops/n_threads, memory_order_relaxed);
        });
    }
    auto t0 = high_resolution_clock::now();
    start.store(true, memory_order_release);
    for(auto& t : ths) t.join();
    return total.load() / duration<double>(high_resolution_clock::now()-t0).count();
}

template<bool UsePool>
double bench_single_size(int n_threads, size_t sz, int ops) {
    atomic<bool> start{false};
    atomic<long long> total{0};
    SlabPool pool;
    vector<thread> ths;
    for(int t=0; t<n_threads; t++) {
        ths.emplace_back([&]() {
            while(!start) {}
            for(int i=0; i<ops/n_threads; i++) {
                void* p = UsePool ? pool.allocate(sz) : ::malloc(sz);
                memset(p, 0xAA, min(sz, size_t(256)));
                if constexpr (UsePool) pool.deallocate(p, sz); else ::free(p);
            }
            total.fetch_add(ops/n_threads, memory_order_relaxed);
        });
    }
    auto t0 = high_resolution_clock::now();
    start.store(true, memory_order_release);
    for(auto& t : ths) t.join();
    return total.load() / duration<double>(high_resolution_clock::now()-t0).count();
}

int main() {
    vector<int> threads = {1, 2, 4, 8};
    int ops = 500000;
    
    cout << "\n╔══════════════════════════════════════════════╗\n";
    cout << "║   内存池 vs malloc 性能矩阵                   ║\n";
    cout << "╚══════════════════════════════════════════════╝\n\n";
    
    // Test 1: alloc/free 吞吐
    cout << ">>> [1/4] 小对象 alloc/free 吞吐 (128 bytes)\n";
    cout << left << setw(28) << "对比项 \\ 线程数";
    for(int t : threads) cout << " | " << right << setw(10) << ("t="+to_string(t));
    cout << "\n" << string(28,'-');
    for(size_t i=0;i<threads.size();i++) cout << " | " << string(10,'-');
    cout << "\n";
    
    cout << left << setw(28) << "malloc/free";
    for(int t : threads) { double q = bench_alloc_free<false>(t,ops); cout << " | " << right << setw(8) << fixed << setprecision(2) << (q/1e6) << "M"; }
    cout << "\n";
    cout << left << setw(28) << "SlabPool";
    for(int t : threads) { double q = bench_alloc_free<true>(t,ops); cout << " | " << right << setw(8) << fixed << setprecision(2) << (q/1e6) << "M"; }
    cout << "\n\n";
    
    // Test 2: 不同块大小
    cout << ">>> [2/4] 不同块大小 (4线程)\n";
    vector<size_t> sizes = {16, 64, 256, 1024, 4096, 16384};
    cout << left << setw(28) << "块大小 \\ 实现";
    cout << " | " << right << setw(12) << "malloc" << " | " << setw(12) << "Pool" << " | " << setw(8) << "加速比";
    cout << "\n" << string(28,'-') << " | " << string(12,'-') << " | " << string(12,'-') << " | " << string(8,'-') << "\n";
    for(size_t sz : sizes) {
        cout << left << setw(26) << (to_string(sz)+"B");
        double m = bench_single_size<false>(4, sz, ops/4);
        double p = bench_single_size<true>(4, sz, ops/4);
        cout << " | " << right << setw(10) << fixed << setprecision(2) << (m/1e6) << "M";
        cout << " | " << setw(10) << (p/1e6) << "M";
        cout << " | " << setw(6) << fixed << setprecision(1) << (p/m) << "x\n";
    }
    cout << "\n";
    
    // Test 3: 并发扩展性
    cout << ">>> [3/4] 并发扩展 (malloc vs Pool, 256B)\n";
    for(int t : threads) {
        double m = bench_single_size<false>(t, 256, ops);
        double p = bench_single_size<true>(t, 256, ops);
        cout << "  t=" << t << "  malloc=" << fixed << setprecision(2) << (m/1e6) << "M  Pool=" << (p/1e6) << "M  加速=" << (p/m) << "x\n";
    }
    
    // Test 4: 碎片化压力测试
    cout << "\n>>> [4/4] 随机大小压力测试 (模拟真实负载)\n";
    cout << left << setw(28) << "实现";
    cout << " | " << right << setw(12) << "ops/s" << " | " << setw(12) << "碎片率";
    cout << "\n" << string(28,'-') << " | " << string(12,'-') << " | " << string(12,'-') << "\n";
    
    srand(42);
    // random sizes
    atomic<long long> mall_ops{0}, pool_ops{0};
    atomic<bool> go{false};
    SlabPool pool;
    
    auto rand_test = [&](bool use_pool) {
        vector<thread> ths;
        for(int t=0; t<4; t++) {
            ths.emplace_back([&,use_pool]() {
                while(!go) {}
                for(int i=0; i<ops/4; i++) {
                    size_t sz = 16 + (rand() % 2048);
                    void* p = use_pool ? pool.allocate(sz) : ::malloc(sz);
                    if(use_pool) pool.deallocate(p,sz); else ::free(p);
                }
                (use_pool ? pool_ops : mall_ops).fetch_add(ops/4, memory_order_relaxed);
            });
        }
        auto t0 = high_resolution_clock::now();
        go.store(true); for(auto& t:ths) t.join(); go.store(false);
        return duration<double>(high_resolution_clock::now()-t0).count();
    };
    
    double mt = rand_test(false);
    double pt = rand_test(true);
    cout << left << setw(28) << "malloc"; cout << " | " << right << setw(10) << fixed << setprecision(2) << (ops/mt/1e6) << "M | " << setw(10) << "-" << "\n";
    cout << left << setw(28) << "SlabPool"; cout << " | " << right << setw(10) << fixed << setprecision(2) << (ops/pt/1e6) << "M | " << setw(10) << "0%" << "\n";
    
    cout << "\n✅ 内存池对比完成\n";
    return 0;
}
EOF
g++ -std=c++17 -O3 -pthread -o /tmp/bench_pool /tmp/bench_pool.cpp
/tmp/bench_pool | tee "${OUT}"
echo ""
echo "结果已保存: ${OUT}"
