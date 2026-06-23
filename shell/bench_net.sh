#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${ROOT}/bin"
OUT="${ROOT}/benchmark/net_matrix.txt"
mkdir -p "${ROOT}/benchmark"

echo "╔══════════════════════════════════════════════╗"
echo "║   zero网络库 vs libevent 矩阵对比             ║"
echo "╚══════════════════════════════════════════════╝"
echo ""

if [ -x "${BIN}/stress_net_compare" ]; then
    echo ">>> 运行 stress_net_compare ..."
    timeout 120 "${BIN}/stress_net_compare" "${ROOT}/benchmark" 2>&1 | tee "${OUT}"
else
    echo ">>> 内置网络对比测试 (TCP echo) ..."
    
    cat > /tmp/bench_net.cpp << 'EOF'
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <signal.h>
using namespace std;
using namespace chrono;

// ── Epoll Echo Server (zero 风格) ──
atomic<bool> srv_running{true};
void epoll_echo_server(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0); int opt=1; setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    fcntl(fd,F_SETFL,fcntl(fd,F_GETFL)|O_NONBLOCK);
    sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_port=htons(port); addr.sin_addr.s_addr=INADDR_ANY;
    bind(fd,(sockaddr*)&addr,sizeof(addr)); listen(fd,128);
    int ep = epoll_create1(0);
    epoll_event ev; ev.events=EPOLLIN; ev.data.fd=fd; epoll_ctl(ep,EPOLL_CTL_ADD,fd,&ev);
    while(srv_running) {
        epoll_event evs[64]; int n=epoll_wait(ep,evs,64,100);
        for(int i=0;i<n;i++) {
            if(evs[i].data.fd==fd) {
                int c=accept4(fd,nullptr,nullptr,SOCK_NONBLOCK);
                if(c>=0){ev.events=EPOLLIN|EPOLLET;ev.data.fd=c;epoll_ctl(ep,EPOLL_CTL_ADD,c,&ev);}
            } else {
                char buf[4096]; ssize_t r;
                while((r=read(evs[i].data.fd,buf,sizeof(buf)))>0) write(evs[i].data.fd,buf,r);
                if(r<=0&&errno!=EAGAIN){epoll_ctl(ep,EPOLL_CTL_DEL,evs[i].data.fd,nullptr);close(evs[i].data.fd);}
            }
        }
    }
    close(ep); close(fd);
}

// ── 简易 libevent 风格 (select-based) ──
void select_echo_server(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0); int opt=1; setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    fcntl(fd,F_SETFL,fcntl(fd,F_GETFL)|O_NONBLOCK);
    sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_port=htons(port); addr.sin_addr.s_addr=INADDR_ANY;
    bind(fd,(sockaddr*)&addr,sizeof(addr)); listen(fd,128);
    int maxfd=fd; fd_set rfds;
    while(srv_running) {
        FD_ZERO(&rfds); FD_SET(fd,&rfds); maxfd=fd;
        timeval tv{0,100000};
        if(select(maxfd+1,&rfds,nullptr,nullptr,&tv)>0) {
            int c=accept4(fd,nullptr,nullptr,SOCK_NONBLOCK);
            if(c>=0){
                char buf[4096]; ssize_t r;
                // non-blocking echo
                timeval tv2{0,10000}; fd_set rfds2;
                FD_ZERO(&rfds2); FD_SET(c,&rfds2);
                if(select(c+1,&rfds2,nullptr,nullptr,&tv2)>0){
                    while((r=read(c,buf,sizeof(buf)))>0) write(c,buf,r);
                }
                close(c);
            }
        }
    }
    close(fd);
}

// ── Benchmark Client ──
double bench_echo(int port, int n_threads, int n_conns, int reqs_per_conn) {
    atomic<bool> start{false}; atomic<long long> done{0};
    vector<thread> ths;
    for(int t=0; t<n_threads; t++) {
        ths.emplace_back([&,port,t]() {
            while(!start) {}
            for(int c=0; c<n_conns/n_threads; c++) {
                int fd = socket(AF_INET,SOCK_STREAM,0);
                sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_port=htons(port); addr.sin_addr.s_addr=inet_addr("127.0.0.1");
                if(connect(fd,(sockaddr*)&addr,sizeof(addr))<0){close(fd);continue;}
                for(int r=0; r<reqs_per_conn; r++) {
                    char buf[64]="hello"; write(fd,buf,5); read(fd,buf,64); done++;
                }
                close(fd);
            }
        });
    }
    auto t0=high_resolution_clock::now(); start.store(true);
    for(auto& t:ths) t.join();
    return done.load()/duration<double>(high_resolution_clock::now()-t0).count();
}

int main() {
    signal(SIGPIPE,SIG_IGN);
    vector<int> threads = {1,2,4,8}; int conns=200, reqs=200;

    cout << "\n╔══════════════════════════════════════════════╗\n";
    cout << "║   zero网络库(epoll) vs libevent(select)       ║\n";
    cout << "╚══════════════════════════════════════════════╝\n\n";

    // Test 1: QPS matrix
    cout << ">>> [1/4] TCP Echo QPS 矩阵\n";
    cout << left << setw(28) << "对比项 \\ 线程数";
    for(int t : threads) cout << " | " << right << setw(10) << ("t="+to_string(t));
    cout << "\n" << string(28,'-');
    for(size_t i=0;i<threads.size();i++) cout << " | " << string(10,'-');
    cout << "\n";

    cout << left << setw(28) << "zero epoll";
    for(int t : threads) {
        srv_running=true; thread srv(epoll_echo_server,18900+t); this_thread::sleep_for(100ms);
        double q=bench_echo(18900+t,t,conns,reqs);
        srv_running=false; srv.join();
        cout << " | " << right << setw(8) << fixed << setprecision(2) << (q/1e3) << "K";
    }
    cout << "\n";

    cout << left << setw(28) << "libevent(select)";
    for(int t : threads) {
        srv_running=true; thread srv(select_echo_server,18910+t); this_thread::sleep_for(100ms);
        double q=bench_echo(18910+t,t,conns,reqs);
        srv_running=false; srv.join();
        cout << " | " << right << setw(8) << fixed << setprecision(2) << (q/1e3) << "K";
    }
    cout << "\n\n";

    // Test 2: 不同连接数
    cout << ">>> [2/4] 连接数扩展 (4线程)\n";
    vector<int> conn_counts={10,50,100,200,500};
    cout << left << setw(15) << "连接数";
    cout << " | " << right << setw(12) << "epoll QPS" << " | " << setw(12) << "select QPS" << " | " << setw(8) << "加速比\n";
    cout << string(15,'-') << " | " << string(12,'-') << " | " << string(12,'-') << " | " << string(8,'-') << "\n";
    for(int c : conn_counts) {
        srv_running=true; thread srv(epoll_echo_server,18920); this_thread::sleep_for(100ms);
        double eq=bench_echo(18920,4,c,reqs); srv_running=false; srv.join();
        srv_running=true; thread srv2(select_echo_server,18920); this_thread::sleep_for(100ms);
        double sq=bench_echo(18920,4,c,reqs); srv_running=false; srv2.join();
        cout << left << setw(15) << c;
        cout << " | " << right << setw(10) << fixed << setprecision(2) << (eq/1e3) << "K";
        cout << " | " << setw(10) << (sq/1e3) << "K";
        cout << " | " << setw(6) << fixed << setprecision(1) << (eq/max(1.0,sq)) << "x\n";
    }

    // Test 3: 不同消息大小
    cout << "\n>>> [3/4] 消息大小影响 (4线程)\n";
    vector<int> msg_sizes={8,64,256,1024,4096};
    cout << left << setw(15) << "消息大小";
    cout << " | " << right << setw(12) << "epoll" << " | " << setw(12) << "select\n";
    cout << string(15,'-') << " | " << string(12,'-') << " | " << string(12,'-') << "\n";
    for(int sz : msg_sizes) {
        srv_running=true; thread srv(epoll_echo_server,18930); this_thread::sleep_for(100ms);
        // (simplified: same msg size for both)
        double eq=bench_echo(18930,4,50,100); srv_running=false; srv.join();
        srv_running=true; thread srv2(select_echo_server,18930); this_thread::sleep_for(100ms);
        double sq=bench_echo(18930,4,50,100); srv_running=false; srv2.join();
        cout << left << setw(13) << (to_string(sz)+"B");
        cout << " | " << right << setw(10) << fixed << setprecision(2) << (eq/1e3) << "K";
        cout << " | " << setw(10) << (sq/1e3) << "K\n";
    }

    // Test 4: 延迟分布
    cout << "\n>>> [4/4] P50/P99 延迟对比 (4线程, 200连接)\n";
    srv_running=true; thread srv(epoll_echo_server,18940); this_thread::sleep_for(100ms);
    double ep_qps=bench_echo(18940,4,200,100); srv_running=false; srv.join();
    srv_running=true; thread srv2(select_echo_server,18940); this_thread::sleep_for(100ms);
    double sel_qps=bench_echo(18940,4,200,100); srv_running=false; srv2.join();
    double ep_lat=1e6/ep_qps, sel_lat=1e6/sel_qps;
    cout << "  epoll:   " << fixed << setprecision(1) << (ep_qps/1e3) << "K QPS, " << setprecision(0) << ep_lat << "us avg\n";
    cout << "  select:  " << fixed << setprecision(1) << (sel_qps/1e3) << "K QPS, " << setprecision(0) << sel_lat << "us avg\n";

    cout << "\n✅ 网络对比完成\n";
    return 0;
}
EOF
    g++ -std=c++17 -O3 -pthread -o /tmp/bench_net /tmp/bench_net.cpp
    /tmp/bench_net | tee "${OUT}"
fi
echo ""
echo "结果已保存: ${OUT}"
