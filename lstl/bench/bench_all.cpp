#include <cstring>
#include <lstl/container/vector.h>
#include <lstl/container/list.h>
#include <lstl/container/deque.h>
#include <lstl/container/set.h>
#include <lstl/container/unordered_map.h>
#include <lstl/container/unordered_set.h>

#include <vector>
#include <list>
#include <deque>
#include <set>
#include <unordered_map>
#include <unordered_set>


#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace std::chrono;

static const size_t N = 500000;
static const size_t SMALL_N = 100000;

struct Timer { high_resolution_clock::time_point start; const char* name;
    Timer(const char* n):start(high_resolution_clock::now()),name(n){}
    ~Timer(){auto us=duration_cast<microseconds>(high_resolution_clock::now()-start).count();printf("  %-50s %8ld us\n",name,us);}
};
volatile int sink;

// ======== vector<const char*>/string_view ========
void bench_vector_sv() {
    printf("\n--- vector<string_view> push_back (n=%zu) ---\n", N);
    std::vector<const char*> data;
    for(size_t i=0;i<N;i++) data.push_back("hello_world_12345");

    { Timer t("  lstl::vector<string_view> push_back");
        lstl::vector<const char*> v;
        for(size_t i=0;i<N;i++) v.push_back(data[i]);
        sink=1; }

    { Timer t("  std::vector<string_view>  push_back");
        std::vector<const char*> v;
        for(size_t i=0;i<N;i++) v.push_back(data[i]);
        sink=1; }

    // clear + re-push (parser hot path)
    { Timer t("  lstl::vector<string_view> clear+push 100x");
        lstl::vector<const char*> v;
        for(int r=0;r<100;r++){ v.clear(); for(size_t i=0;i<N/100;i++) v.push_back(data[i]); }
        sink=1; }

    { Timer t("  std::vector<string_view>  clear+push 100x");
        std::vector<const char*> v;
        for(int r=0;r<100;r++){ v.clear(); for(size_t i=0;i<N/100;i++) v.push_back(data[i]); }
        sink=1; }
}

// ======== vector<string> ========
void bench_vector_string() {
    printf("\n--- vector<string> push_back (n=%zu) ---\n", SMALL_N);
    { Timer t("  lstl::vector<string> push_back");
        lstl::vector<std::string> v;
        for(size_t i=0;i<SMALL_N;i++) v.push_back("hello_world_12345");
        sink=1; }

    { Timer t("  std::vector<string>  push_back");
        std::vector<std::string> v;
        for(size_t i=0;i<SMALL_N;i++) v.push_back("hello_world_12345");
        sink=1; }
}

// ======== unordered_map ========
void bench_unordered_map() {
    printf("\n--- unordered_map<string,string> (n=%zu) ---\n", N);
    lstl::unordered_map<std::string,std::string> lm;
    std::unordered_map<std::string,std::string> sm;
    for(size_t i=0;i<N;i++){char k[32];snprintf(k,sizeof(k),"key_%zu",i);lm.insert({k,"v"});sm.insert({k,"v"});}

    { Timer t("  lstl::unordered_map insert");
        lstl::unordered_map<std::string,std::string> m;
        for(size_t i=0;i<N;i++){char k[32];snprintf(k,sizeof(k),"k%zu",i);m.insert({k,"v"});}
        sink=m.size(); }

    { Timer t("  std::unordered_map  insert");
        std::unordered_map<std::string,std::string> m;
        for(size_t i=0;i<N;i++){char k[32];snprintf(k,sizeof(k),"k%zu",i);m.insert({k,"v"});}
        sink=m.size(); }

    { Timer t("  lstl::unordered_map find (hit)");
        for(size_t i=0;i<N;i++){char k[32];snprintf(k,sizeof(k),"key_%zu",i);sink+=lm.count(k);} }

    { Timer t("  std::unordered_map  find (hit)");
        for(size_t i=0;i<N;i++){char k[32];snprintf(k,sizeof(k),"key_%zu",i);sink+=sm.count(k);} }

    { Timer t("  lstl::unordered_map find (miss)");
        for(size_t i=0;i<N;i++) sink+=lm.count("nonexistent"); }

    { Timer t("  std::unordered_map  find (miss)");
        for(size_t i=0;i<N;i++) sink+=sm.count("nonexistent"); }

    { Timer t("  lstl::unordered_map erase");
        for(size_t i=0;i<N/2;i++){char k[32];snprintf(k,sizeof(k),"key_%zu",i);lm.erase(k);}
        sink=lm.size(); }

    { Timer t("  std::unordered_map  erase");
        for(size_t i=0;i<N/2;i++){char k[32];snprintf(k,sizeof(k),"key_%zu",i);sm.erase(k);}
        sink=sm.size(); }
}

// ======== unordered_set ========
void bench_unordered_set() {
    printf("\n--- unordered_set<string> (n=%zu) ---\n", N);
    lstl::unordered_set<std::string> ls;
    std::unordered_set<std::string> ss;
    for(size_t i=0;i<N;i++){char k[32];snprintf(k,sizeof(k),"m_%zu",i);ls.insert(k);ss.insert(k);}

    { Timer t("  lstl::unordered_set insert");
        lstl::unordered_set<std::string> s;
        for(size_t i=0;i<N;i++){char k[32];snprintf(k,sizeof(k),"s%zu",i);s.insert(k);}
        sink=s.size(); }

    { Timer t("  std::unordered_set  insert");
        std::unordered_set<std::string> s;
        for(size_t i=0;i<N;i++){char k[32];snprintf(k,sizeof(k),"s%zu",i);s.insert(k);}
        sink=s.size(); }

    { Timer t("  lstl::unordered_set count (hit)");
        for(size_t i=0;i<N;i++){char k[32];snprintf(k,sizeof(k),"m_%zu",i);sink+=ls.count(k);} }

    { Timer t("  std::unordered_set  count (hit)");
        for(size_t i=0;i<N;i++){char k[32];snprintf(k,sizeof(k),"m_%zu",i);sink+=ss.count(k);} }
}

// ======== list ========
void bench_list() {
    printf("\n--- list<string> (n=%zu) ---\n", SMALL_N);
    { Timer t("  lstl::list push_back");
        lstl::list<std::string> l;
        for(size_t i=0;i<SMALL_N;i++) l.push_back("hello");
        sink=l.size(); }

    { Timer t("  std::list  push_back");
        std::list<std::string> l;
        for(size_t i=0;i<SMALL_N;i++) l.push_back("hello");
        sink=l.size(); }

    lstl::list<std::string> ll; std::list<std::string> sl;
    for(size_t i=0;i<SMALL_N;i++){ll.push_back("x");sl.push_back("x");}

    { Timer t("  lstl::list iterate"); for(auto&x:ll) sink+=x.size(); }
    { Timer t("  std::list  iterate"); for(auto&x:sl) sink+=x.size(); }

    { Timer t("  lstl::list push_front+pop_front");
        lstl::list<std::string> l;
        for(size_t i=0;i<SMALL_N;i++) l.push_front("h");
        while(!l.empty()) l.pop_front(); }

    { Timer t("  std::list  push_front+pop_front");
        std::list<std::string> l;
        for(size_t i=0;i<SMALL_N;i++) l.push_front("h");
        while(!l.empty()) l.pop_front(); }
}

// ======== set (rb-tree, ZSet) ========
void bench_set() {
    printf("\n--- set<Entry> (n=%zu) ---\n", N/2);
    struct Entry { std::string s; double d;
        bool operator<(const Entry&o)const{return d!=o.d?d<o.d:s<o.s;}
    };
    lstl::set<Entry> lset; std::set<Entry> sset;
    for(size_t i=0;i<N/2;i++){char k[32];snprintf(k,sizeof(k),"z_%zu",i);double d=i*1.5;lset.insert({k,d});sset.insert({k,d});}

    { Timer t("  lstl::set insert");
        lstl::set<Entry> s;
        for(size_t i=0;i<N/2;i++){char k[32];snprintf(k,sizeof(k),"n%zu",i);s.insert({k,i*1.5});}
        sink=s.size(); }

    { Timer t("  std::set  insert");
        std::set<Entry> s;
        for(size_t i=0;i<N/2;i++){char k[32];snprintf(k,sizeof(k),"n%zu",i);s.insert({k,i*1.5});}
        sink=s.size(); }

    { Timer t("  lstl::set find");
        for(size_t i=0;i<N/2;i++){char k[32];snprintf(k,sizeof(k),"z_%zu",i);sink+=lset.count({k,i*1.5});} }

    { Timer t("  std::set  find");
        for(size_t i=0;i<N/2;i++){char k[32];snprintf(k,sizeof(k),"z_%zu",i);sink+=sset.count({k,i*1.5});} }

    { Timer t("  lstl::set iterate"); for(auto&x:lset) sink+=x.s.size(); }
    { Timer t("  std::set  iterate"); for(auto&x:sset) sink+=x.s.size(); }
}

// ======== deque ========
void bench_deque() {
    printf("\n--- deque<string> (n=%zu) ---\n", SMALL_N);
    { Timer t("  lstl::deque push_back+pop_front");
        lstl::deque<std::string> d;
        for(size_t i=0;i<SMALL_N;i++) d.push_back("hello");
        while(!d.empty()) d.pop_front(); }

    { Timer t("  std::deque  push_back+pop_front");
        std::deque<std::string> d;
        for(size_t i=0;i<SMALL_N;i++) d.push_back("hello");
        while(!d.empty()) d.pop_front(); }

    lstl::deque<int> ld; std::deque<int> sd;
    for(int i=0;i<100000;i++){ld.push_back(i);sd.push_back(i);}
    { Timer t("  lstl::deque random access");
        for(int i=0;i<100000;i++) sink+=ld[i]; }
    { Timer t("  std::deque  random access");
        for(int i=0;i<100000;i++) sink+=sd[i]; }
}

int main() {
    printf("=== Full Container Benchmark ===\n");
    bench_vector_sv();
    bench_vector_string();
    bench_unordered_map();
    bench_unordered_set();
    bench_list();
    bench_set();
    bench_deque();
    printf("\n=== done (sink=%d) ===\n", sink);
}
