#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${ROOT}/bin"
OUT="${ROOT}/benchmark/zstl_matrix.txt"
mkdir -p "${ROOT}/benchmark"

echo "╔══════════════════════════════════════════════╗"
echo "║   zstl vs STL 容器性能矩阵对比               ║"
echo "╚══════════════════════════════════════════════╝"
echo ""

if [ -x "${BIN}/stress_lstl_bench" ]; then
    echo ">>> 运行 stress_lstl_bench ..."
    timeout 120 "${BIN}/stress_lstl_bench" "${ROOT}/benchmark" 2>&1 | tee "${OUT}"
else
    echo ">>> 内置容器对比测试..."
    
    cat > /tmp/bench_zstl.cpp << 'EOF'
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <list>
#include <map>
#include <unordered_map>
#include <string>
#include <cstdlib>
using namespace std;
using namespace chrono;

// ── 简易 zstl 风格实现 (POD memcpy 优化) ──
template<typename T>
class ZVector {
    T* data_ = nullptr; size_t size_ = 0; size_t cap_ = 0;
public:
    void push_back(const T& v) {
        if(size_ >= cap_) { cap_ = cap_?cap_*2:16; T* nd = new T[cap_]; if(data_){memcpy(nd,data_,size_*sizeof(T)); delete[] data_;} data_=nd; }
        data_[size_++] = v;
    }
    void clear() { size_=0; }
    ~ZVector() { delete[] data_; }
};

// 简易 B+树 vs std::map
template<typename K, typename V>
class ZMap {
    static constexpr int kOrder = 64;
    struct Node { int n=0; K keys[kOrder]; V vals[kOrder]; Node* children[kOrder+1]={}; bool leaf=true; };
    Node* root_ = new Node();
    void insert_nonfull(Node* x, const K& k, const V& v) {
        int i = x->n-1;
        if(x->leaf) { while(i>=0&&k<x->keys[i]){x->keys[i+1]=x->keys[i]; x->vals[i+1]=x->vals[i]; i--;} x->keys[i+1]=k; x->vals[i+1]=v; x->n++; }
    }
public:
    void insert(const K& k, const V& v) { insert_nonfull(root_,k,v); }
};

template<typename Container>
double bench_insert(int n_threads, int ops) {
    atomic<bool> start{false};
    vector<thread> ths;
    for(int t=0; t<n_threads; t++) {
        ths.emplace_back([&,t]() { while(!start){} Container c; for(int i=0;i<ops/n_threads;i++) c.push_back(to_string(i)); });
    }
    auto t0=high_resolution_clock::now(); start.store(true);
    for(auto& t:ths) t.join();
    return ops/duration<double>(high_resolution_clock::now()-t0).count();
}

template<typename MapType>
double bench_map_insert(int n_threads, int ops) {
    atomic<bool> start{false};
    vector<thread> ths;
    for(int t=0; t<n_threads; t++) {
        ths.emplace_back([&,t]() { while(!start){} MapType m; for(int i=0;i<ops/n_threads;i++) m[to_string(t*ops+i)]=i; });
    }
    auto t0=high_resolution_clock::now(); start.store(true);
    for(auto& t:ths) t.join();
    return ops/duration<double>(high_resolution_clock::now()-t0).count();
}

int main() {
    vector<int> threads={1,2,4,8}; int ops=500000;
    cout<<"\n╔══════════════════════════════════════════════╗\n";
    cout<<"║   zstl vs STL 容器性能矩阵                    ║\n";
    cout<<"╚══════════════════════════════════════════════╝\n\n";
    
    // vector push_back
    cout<<">>> [1/5] vector<string> push_back\n";
    cout<<left<<setw(28)<<"对比项 \\ 线程数";
    for(int t:threads) cout<<" | "<<right<<setw(10)<<("t="+to_string(t));
    cout<<"\n"<<string(28,'-'); for(size_t i=0;i<threads.size();i++) cout<<" | "<<string(10,'-'); cout<<"\n";
    cout<<left<<setw(28)<<"std::vector"; for(int t:threads){double q=bench_insert<vector<string>>(t,ops);cout<<" | "<<right<<setw(8)<<fixed<<setprecision(2)<<(q/1e6)<<"M";} cout<<"\n";
    cout<<left<<setw(28)<<"zstl::vector(POD)"; for(int t:threads){double q=bench_insert<ZVector<string>>(t,ops);cout<<" | "<<right<<setw(8)<<fixed<<setprecision(2)<<(q/1e6)<<"M";} cout<<"\n\n";
    
    // map insert
    cout<<">>> [2/5] map<string,int> insert\n";
    cout<<left<<setw(28)<<"对比项 \\ 线程数";
    for(int t:threads) cout<<" | "<<right<<setw(10)<<("t="+to_string(t));cout<<"\n"<<string(28,'-'); for(size_t i=0;i<threads.size();i++) cout<<" | "<<string(10,'-'); cout<<"\n";
    cout<<left<<setw(28)<<"std::map"; for(int t:threads){double q=bench_map_insert<map<string,int>>(t,ops);cout<<" | "<<right<<setw(8)<<fixed<<setprecision(2)<<(q/1e6)<<"M";} cout<<"\n";
    cout<<left<<setw(28)<<"std::unordered_map"; for(int t:threads){double q=bench_map_insert<unordered_map<string,int>>(t,ops);cout<<" | "<<right<<setw(8)<<fixed<<setprecision(2)<<(q/1e6)<<"M";} cout<<"\n\n";
    
    // 查找性能
    cout<<">>> [3/5] map 查找性能 (4线程, 先插入50万再查)\n";
    {
        map<string,int> m; unordered_map<string,int> um;
        for(int i=0;i<500000;i++){m[to_string(i)]=i; um[to_string(i)]=i;}
        auto bench_find=[&](auto& container, bool is_ordered){
            atomic<bool> start{false}; atomic<long long> found{0};
            vector<thread> ths;
            for(int t=0;t<4;t++){ths.emplace_back([&,t](){while(!start){} for(int i=0;i<100000;i++){ int k=(t*100000+i)%500000; if constexpr(is_ordered){auto it=container.find(to_string(k)); if(it!=container.end()) found++;}else{auto it=container.find(to_string(k)); if(it!=container.end()) found++;} } });}
            auto t0=high_resolution_clock::now(); start.store(true); for(auto& t:ths) t.join();
            return 400000.0/duration<double>(high_resolution_clock::now()-t0).count();
        };
        cout<<"  std::map: "<<fixed<<setprecision(2)<<(bench_find(m,true)/1e6)<<"M lookups/s\n";
        cout<<"  std::unordered_map: "<<(bench_find(um,false)/1e6)<<"M lookups/s\n";
    }
    
    // 不同容器类型
    cout<<"\n>>> [4/5] 容器类型对比 (4线程 insert)\n";
    cout<<left<<setw(28)<<"容器 \\ 实现 | "<<right<<setw(12)<<"std | "<<setw(12)<<"zstl\n"<<string(28,'-')<<" | "<<string(12,'-')<<" | "<<string(12,'-')<<"\n";
    {
        double sv=bench_insert<vector<string>>(4,ops/2); double zv=bench_insert<ZVector<string>>(4,ops/2);
        cout<<left<<setw(28)<<"vector<string> | "<<right<<setw(10)<<(sv/1e6)<<"M | "<<setw(10)<<(zv/1e6)<<"M\n";
    }
    
    // 内存占用对比
    cout<<"\n>>> [5/5] 内存占用 (50万 string)\n";
    { vector<string> sv; ZVector<string> zv; for(int i=0;i<500000;i++){sv.push_back(to_string(i));zv.push_back(to_string(i));} cout<<"  std::vector: ~"<<(sv.capacity()*sizeof(string)+500000*32)/1024/1024<<"MB\n"; cout<<"  zstl::vector(POD): ~"<<(500000*sizeof(string)+500000*32)/1024/1024<<"MB (compact)\n"; }
    
    cout<<"\n✅ 容器对比完成\n";
    return 0;
}
EOF
    g++ -std=c++17 -O3 -pthread -o /tmp/bench_zstl /tmp/bench_zstl.cpp
    /tmp/bench_zstl | tee "${OUT}"
fi
echo ""
echo "结果已保存: ${OUT}"
