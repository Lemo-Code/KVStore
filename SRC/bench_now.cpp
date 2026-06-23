// Standalone performance benchmark — gets real QPS data
#include <chrono>
#include <iostream>
#include <iomanip>
#include <thread>
#include <atomic>
#include <vector>
#include <map>
#include <unordered_map>
#include <string>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <mutex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

using namespace std::chrono;
using std::cout;
using std::endl;

struct Timer {
    high_resolution_clock::time_point start;
    Timer() : start(high_resolution_clock::now()) {}
    double ms() { return duration_cast<microseconds>(high_resolution_clock::now() - start).count() / 1000.0; }
};

// Simple benchmark runner
void report(const std::string& name, int N, double ms) {
    double qps = (double)N / (ms / 1000.0);
    cout << "  " << std::left << std::setw(52) << name
         << std::right << std::setw(14) << std::fixed << std::setprecision(0) << qps << " ops/sec"
         << "  (" << std::setprecision(1) << ms << " ms)" << endl;
}

void bench_pool() {
    cout << "\n=== MEMORY POOL QPS ===" << endl;
    int N;

    // Small alloc
    N = 5000000;
    { Timer t; for (int i = 0; i < N; ++i) { volatile char* p = new char[64]; delete[] p; } report("new/delete 64B", N, t.ms()); }

    // Medium
    N = 2000000;
    { Timer t; for (int i = 0; i < N; ++i) { volatile char* p = new char[512]; delete[] p; } report("new/delete 512B", N, t.ms()); }

    // Large
    N = 1000000;
    { Timer t; for (int i = 0; i < N; ++i) { volatile char* p = new char[4096]; delete[] p; } report("new/delete 4KB", N, t.ms()); }

    // Multi-threaded
    for (int threads : {1, 4, 8, 16}) {
        int per = 1000000 / threads;
        std::atomic<bool> start{false};
        std::atomic<long long> ops{0};
        std::vector<std::thread> ths;
        Timer t;
        for (int i = 0; i < threads; ++i) {
            ths.emplace_back([&, per]() {
                while (!start.load()) {}
                for (int j = 0; j < per; ++j) {
                    volatile char* p = new char[(j % 50 + 1) * 16];
                    delete[] p;
                }
                ops.fetch_add(per);
            });
        }
        start.store(true);
        for (auto& th : ths) th.join();
        std::string label = "new/delete mixed " + std::to_string(threads) + " threads";
        report(label, ops.load(), t.ms());
    }
}

void bench_vector() {
    cout << "\n=== VECTOR QPS ===" << endl;
    int N;

    N = 5000000;
    { Timer t; std::vector<int> v; for (int i = 0; i < N; ++i) v.push_back(i); report("std::vector push_back (no reserve)", N, t.ms()); }
    { Timer t; std::vector<int> v; v.reserve(N); for (int i = 0; i < N; ++i) v.push_back(i); report("std::vector push_back (with reserve)", N, t.ms()); }

    N = 1000000;
    std::vector<int> sort_data(N);
    for (int i = 0; i < N; ++i) sort_data[i] = rand();
    auto sort_copy = sort_data;
    { Timer t; auto v = sort_copy; std::sort(v.begin(), v.end()); report("std::sort 1M random ints", N, t.ms()); }

    N = 10000000;
    std::vector<int> iter_data(N, 42);
    { Timer t; long long sum = 0; for (int i = 0; i < N; ++i) sum += iter_data[i]; volatile long long v=sum; (void)v; report("std::vector iteration (index)", N, t.ms()); }
    { Timer t; long long sum = 0; for (auto it = iter_data.begin(); it != iter_data.end(); ++it) sum += *it; volatile long long v=sum; (void)v; report("std::vector iteration (iterator)", N, t.ms()); }
    { Timer t; auto it = std::find(iter_data.begin(), iter_data.end(), 42); volatile bool found = (it != iter_data.end()); (void)found; report("std::find (early match)", N, t.ms()); }
}

void bench_map() {
    cout << "\n=== MAP QPS ===" << endl;
    int N;

    N = 500000;
    { Timer t; std::map<int, int> m; for (int i = 0; i < N; ++i) m[i] = i; report("std::map insert 500K", N, t.ms()); }
    { Timer t; std::unordered_map<int, int> m; for (int i = 0; i < N; ++i) m[i] = i; report("std::unordered_map insert 500K", N, t.ms()); }

    N = 2000000;
    {
        std::map<int, int> m;
        for (int i = 0; i < N; ++i) m[i] = i;
        Timer t;
        long long sum = 0;
        for (int i = 0; i < N; ++i) sum += m.find(i)->second;
        volatile long long v = sum; (void)v;
        report("std::map find (hit) 2M", N, t.ms());
    }
    {
        std::unordered_map<int, int> m;
        for (int i = 0; i < N; ++i) m[i] = i;
        Timer t;
        long long sum = 0;
        for (int i = 0; i < N; ++i) sum += m.find(i)->second;
        volatile long long v = sum; (void)v;
        report("std::unordered_map find (hit) 2M", N, t.ms());
    }

    N = 500000;
    { Timer t; std::map<int, int> m; for(int i=0;i<N;++i)m[i]=i; for(int i=0;i<N;++i)m.erase(i); report("std::map erase 500K", N, t.ms()); }
    { Timer t; std::unordered_map<int,int> m; for(int i=0;i<N;++i)m[i]=i; for(int i=0;i<N;++i)m.erase(i); report("std::unordered_map erase 500K", N, t.ms()); }
}

void bench_string() {
    cout << "\n=== STRING QPS ===" << endl;
    int N = 5000000;

    { Timer t; std::string src="hello"; for(int i=0;i<N;++i){ volatile std::string copy=src; } report("std::string copy (5 chars, SSO)", N, t.ms()); }
    { Timer t; std::string src(50,'x'); for(int i=0;i<N;++i){ volatile std::string copy=src; } report("std::string copy (50 chars, heap)", N, t.ms()); }

    N = 50000;
    { Timer t; for(int i=0;i<N;++i){ std::string s; for(int j=0;j<1000;++j) s+='x'; volatile size_t sz=s.size(); (void)sz; } report("std::string append (1000x1 char)", N, t.ms()); }

    N = 2000000;
    std::string haystack(100000,'x'); haystack[99999]='y';
    { Timer t; for(int i=0;i<N;++i){ volatile size_t pos=haystack.find('y'); (void)pos; } report("std::string::find (last char in 100KB)", N, t.ms()); }

    N = 5000000;
    { Timer t; for(int i=0;i<N;++i){ volatile std::string s=std::to_string(i); } report("std::to_string(int)", N, t.ms()); }
}

void bench_concurrency() {
    cout << "\n=== CONCURRENCY QPS ===" << endl;

    for (int threads : {1, 2, 4, 8, 16}) {
        std::atomic<long long> counter{0};
        std::atomic<bool> start{false};
        std::vector<std::thread> ths;
        int per = 2000000;
        Timer t;
        for (int i = 0; i < threads; ++i) {
            ths.emplace_back([&, per]() {
                while (!start.load()) {}
                for (int j = 0; j < per; ++j) counter.fetch_add(1, std::memory_order_relaxed);
            });
        }
        start.store(true);
        for (auto& th : ths) th.join();
        std::string label = "atomic fetch_add " + std::to_string(threads) + " threads";
        report(label, threads * per, t.ms());
    }

    for (int threads : {1, 2, 4, 8, 16}) {
        std::mutex mtx;
        long long counter = 0;
        std::atomic<bool> start{false};
        std::vector<std::thread> ths;
        int per = 1000000;
        Timer t;
        for (int i = 0; i < threads; ++i) {
            ths.emplace_back([&, per]() {
                while (!start.load()) {}
                for (int j = 0; j < per; ++j) {
                    std::lock_guard<std::mutex> lk(mtx);
                    ++counter;
                }
            });
        }
        start.store(true);
        for (auto& th : ths) th.join();
        std::string label = "mutex lock/unlock " + std::to_string(threads) + " threads";
        report(label, threads * per, t.ms());
    }

    // Thread create/join
    { int N=100000; Timer t; for(int i=0;i<N;++i){std::thread th([]{});
        th.join();} report("thread create+join", N, t.ms()); }
}

void bench_network() {
    cout << "\n=== NETWORK QPS ===" << endl;

    int N = 500000;
    { Timer t; for(int i=0;i<N;++i){int fd=socket(AF_INET,SOCK_STREAM,0);close(fd);}
      report("socket()+close()", N, t.ms()); }

    // TCP echo benchmark - use non-blocking for clean exit
    {
        int sfd = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        fcntl(sfd, F_SETFL, O_NONBLOCK);
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        addr.sin_port = 0;
        bind(sfd, (struct sockaddr*)&addr, sizeof(addr));
        socklen_t alen = sizeof(addr);
        getsockname(sfd, (struct sockaddr*)&addr, &alen);
        listen(sfd, 128);

        std::atomic<bool> srv_run{true};
        int port = ntohs(addr.sin_port);
        std::thread server([sfd, &srv_run]() {
            while (srv_run.load()) {
                int c = accept(sfd, nullptr, nullptr);
                if (c < 0) { usleep(1000); continue; }
                char buf[256];
                ssize_t n = recv(c, buf, sizeof(buf), MSG_DONTWAIT);
                if (n > 0) send(c, buf, n, MSG_DONTWAIT);
                close(c);
            }
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        N = 50000;
        { Timer t;
          for (int i = 0; i < N; ++i) {
              int fd = socket(AF_INET, SOCK_STREAM, 0);
              struct sockaddr_in sa;
              memset(&sa, 0, sizeof(sa));
              sa.sin_family = AF_INET;
              sa.sin_addr.s_addr = inet_addr("127.0.0.1");
              sa.sin_port = htons(port);
              if (connect(fd, (struct sockaddr*)&sa, sizeof(sa)) == 0) {
                  const char* msg = "ping";
                  send(fd, msg, 4, MSG_DONTWAIT);
                  char buf[64];
                  recv(fd, buf, sizeof(buf), MSG_DONTWAIT);
              }
              close(fd);
          }
          report("TCP localhost echo (connect+send+recv)", N, t.ms());
        }

        srv_run.store(false);
        shutdown(sfd, SHUT_RDWR);
        close(sfd);
        server.join();
    }

    // Pipe throughput - simple non-blocking approach
    {
        int fds[2];
        if (pipe(fds) == 0) {
            fcntl(fds[0], F_SETFL, O_NONBLOCK);
            N = 1000000;
            { Timer t;
              char buf[1] = {'x'};
              for(int i=0;i<N;++i) {
                  if (write(fds[1], buf, 1) < 0) ++i; // skip on full
              }
              report("pipe write (1 byte nonblock)", N, t.ms());
            }
            close(fds[0]); close(fds[1]);
        }
    }
}

void bench_log() {
    cout << "\n=== LOG QPS ===" << endl;
    int N;

    N = 100000;
    { Timer t; for(int i=0;i<N;++i){std::cout<<"log message "<<i<<"\n";} report("std::cout with newline", N, t.ms()); }

    FILE* devnull = fopen("/dev/null", "w");
    N = 5000000;
    { Timer t; for(int i=0;i<N;++i){fprintf(devnull,"log message %d\n",i);} report("fprintf to /dev/null", N, t.ms()); }
    fclose(devnull);

    N = 10000000;
    { Timer t; for(int i=0;i<N;++i){char buf[128];snprintf(buf,sizeof(buf),"log message %d",i);volatile char c=buf[0];(void)c;}
      report("snprintf to buffer (no I/O)", N, t.ms()); }

    N = 20000000;
    { Timer t; for(int i=0;i<N;++i){struct timespec ts;clock_gettime(CLOCK_REALTIME,&ts);volatile long ns=ts.tv_nsec;(void)ns;}
      report("clock_gettime(CLOCK_REALTIME)", N, t.ms()); }
}

int main() {
    cout << "RES/ Performance Benchmark — Real QPS Data" << endl;
    cout << "CPU: " << std::thread::hardware_concurrency() << " cores  |  Compiler: " << __VERSION__ << endl;
    cout << "Using std:: as baseline (zstl integrates these patterns)" << endl;

    bench_pool();
    bench_vector();
    bench_map();
    bench_string();
    bench_concurrency();
    bench_network();
    bench_log();

    cout << "\nDONE\n";
    return 0;
}
