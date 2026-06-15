// Multi-size object benchmark: lstl vs std for small/medium/large
#include <lstl/container/vector.h>
#include <lstl/container/deque.h>
#include <lstl/container/list.h>
#include <lstl/container/unordered_map.h>
#include <lstl/container/set.h>
#include <vector>
#include <deque>
#include <list>
#include <unordered_map>
#include <set>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
using namespace std::chrono;
volatile long g_sink = 0;

struct Timer { high_resolution_clock::time_point s; const char* n;
    Timer(const char* nm):s(high_resolution_clock::now()),n(nm){}
    ~Timer(){auto us=duration_cast<microseconds>(high_resolution_clock::now()-s).count();printf("  %-52s %8ld us\n",n,us);}
};

const int N = 500000;
const int SMALL = 100000;

// Test objects
struct Small { int x; Small(int v=0):x(v){} };
struct Medium { char d[64]; Medium(){memset(d,0,64);} Medium(const char*s){strncpy(d,s,63);} };
struct Large { char d[256]; Large(){memset(d,0,256);} Large(const char*s){strncpy(d,s,255);} };

// Pre-generated data
Small  small_data[N];
Medium med_data[SMALL];
Large  large_data[SMALL/4];

void init_data() {
    for(int i=0;i<N;i++) small_data[i]=Small(i);
    for(int i=0;i<SMALL;i++) med_data[i]=Medium("hello");
    for(int i=0;i<SMALL/4;i++) large_data[i]=Large("world");
}

template<typename Vec, typename T>
void bench_vec_push(const char* label, T* data, int n) {
    Timer t(label);
    Vec v;
    for(int i=0;i<n;i++) v.push_back(data[i]);
    g_sink += v.size();
}

template<typename Deq, typename T>
void bench_deq_pushpop(const char* label, T* data, int n) {
    Deq d;
    for(int i=0;i<n;i++) d.push_back(data[i]);
    g_sink += d.size();
    Timer t(label);
    while(!d.empty()) d.pop_front();
    g_sink += d.size();
}

template<typename Lst, typename T>
void bench_list_pushpop(const char* label, T* data, int n) {
    Lst l;
    for(int i=0;i<n;i++) l.push_back(data[i]);
    g_sink += l.size();
    Timer t(label);
    while(!l.empty()) l.pop_front();
    g_sink += l.size();
}

template<typename Map, typename T>
void bench_map_insert_find(const char* label, int n) {
    Map m;
    char k[32];
    { Timer t((std::string(label)+" insert").c_str());
        for(int i=0;i<n;i++){snprintf(k,sizeof(k),"k%d",i); m.insert({std::string(k),T(i)});}
        g_sink+=m.size(); }
    { Timer t((std::string(label)+" find").c_str());
        for(int i=0;i<n;i++){snprintf(k,sizeof(k),"k%d",i); g_sink+=m.count(k);} }
}

// Concurrent benchmark
template<typename Vec>
void concurrent_vec_push(int n, int threads, std::atomic<long>& total) {
    std::vector<std::thread> thrs;
    std::mutex mtx;
    Vec shared_v;
    auto start = high_resolution_clock::now();
    for(int t=0;t<threads;t++) {
        thrs.emplace_back([&,t](){
            Vec local;
            for(int i=0;i<n/threads;i++) local.push_back(i);
            std::lock_guard<std::mutex> lk(mtx);
            for(auto& x : local) shared_v.push_back(x);
        });
    }
    for(auto& th:thrs) th.join();
    auto us = duration_cast<microseconds>(high_resolution_clock::now()-start).count();
    printf("  concurrent vector push %d x %d: %ld us\n", n/threads, threads, us);
    total += shared_v.size();
}

int main() {
    init_data();
    printf("=== Small (4B int) ===\n");
    bench_vec_push<lstl::vector<Small>>("lstl::vector<Small> push", small_data, N);
    bench_vec_push<std::vector<Small>>("std::vector<Small>  push", small_data, N);
    bench_deq_pushpop<lstl::deque<Small>>("lstl::deque<Small> push+pop", small_data, SMALL);
    bench_deq_pushpop<std::deque<Small>>("std::deque<Small>  push+pop", small_data, SMALL);
    bench_list_pushpop<lstl::list<Small>>("lstl::list<Small> push+pop", small_data, SMALL);
    bench_list_pushpop<std::list<Small>>("std::list<Small>  push+pop", small_data, SMALL);

    printf("\n=== Medium (64B) ===\n");
    bench_vec_push<lstl::vector<Medium>>("lstl::vector<Medium> push", med_data, SMALL);
    bench_vec_push<std::vector<Medium>>("std::vector<Medium>  push", med_data, SMALL);
    bench_deq_pushpop<lstl::deque<Medium>>("lstl::deque<Medium> push+pop", med_data, SMALL/2);
    bench_deq_pushpop<std::deque<Medium>>("std::deque<Medium>  push+pop", med_data, SMALL/2);
    bench_list_pushpop<lstl::list<Medium>>("lstl::list<Medium> push+pop", med_data, SMALL/2);
    bench_list_pushpop<std::list<Medium>>("std::list<Medium>  push+pop", med_data, SMALL/2);

    printf("\n=== Large (256B) ===\n");
    int LN = SMALL/4;
    bench_vec_push<lstl::vector<Large>>("lstl::vector<Large> push", large_data, LN);
    bench_vec_push<std::vector<Large>>("std::vector<Large>  push", large_data, LN);
    bench_deq_pushpop<lstl::deque<Large>>("lstl::deque<Large> push+pop", large_data, LN/2);
    bench_deq_pushpop<std::deque<Large>>("std::deque<Large>  push+pop", large_data, LN/2);

    printf("\n=== unordered_map<string,int> ===\n");
    bench_map_insert_find<lstl::unordered_map<std::string,int>,int>("lstl::umap", N/2);
    bench_map_insert_find<std::unordered_map<std::string,int>,int>("std::umap", N/2);

    printf("\n=== Concurrent push_back (4 threads, Small) ===\n");
    std::atomic<long> total{0};
    concurrent_vec_push<lstl::vector<Small>>(N, 4, total);
    concurrent_vec_push<std::vector<Small>>(N, 4, total);

    printf("\n=== done (sink=%ld) ===\n", g_sink+total.load());
}
