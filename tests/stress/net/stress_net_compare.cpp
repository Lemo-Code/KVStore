/**
 * stress_net_compare.cpp — Raw Epoll vs Libevent TCP Echo Matrix
 *
 * 横轴: 客户端线程 [1, 2, 4]
 * 纵轴: echo 实现 (raw epoll / libevent bufferevent)
 * 纯 TCP echo: 64B × 2000 rounds, 客户端校验收发一致
 * 所有 server 内嵌, 无子进程, 无外部依赖
 */
#include "bench_utils.h"
#include "matrix.h"
using namespace stress;

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

// ============================================================
// TCP echo client
// ============================================================
static void echoClient(int port, int msg_size, int rounds,
                       std::atomic<uint64_t>& ops) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return;
    int one = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    struct timeval tv{3, 0};
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    struct sockaddr_in a{};
    a.sin_family = AF_INET; a.sin_port = htons((uint16_t)port);
    a.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (::connect(fd, (struct sockaddr*)&a, sizeof(a)) < 0) {
        ::close(fd); return;
    }
    std::string msg((size_t)msg_size, 'X');
    std::string rbuf((size_t)msg_size, '\0');
    uint64_t local = 0;
    for (int r = 0; r < rounds; ++r) {
        size_t s = 0;
        while (s < msg.size()) {
            ssize_t n = ::send(fd, msg.data()+s, msg.size()-s, MSG_NOSIGNAL);
            if (n <= 0) goto done; s += (size_t)n;
        }
        size_t g = 0;
        while (g < msg.size()) {
            ssize_t n = ::recv(fd, &rbuf[g], msg.size()-g, 0);
            if (n <= 0) goto done; g += (size_t)n;
        }
        if (::memcmp(msg.data(), rbuf.data(), msg.size()) == 0) local++;
    }
done:
    ::close(fd);
    ops.fetch_add(local, std::memory_order_relaxed);
}

// ============================================================
// Raw Epoll echo server
// ============================================================
static volatile bool g_ep_run = false;
static int g_ep_lfd = -1;

static void startEpoll(int port) {
    g_ep_lfd = ::socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0);
    int o = 1; ::setsockopt(g_ep_lfd, SOL_SOCKET, SO_REUSEADDR, &o, sizeof(o));
    struct sockaddr_in a{};
    a.sin_family = AF_INET; a.sin_port = htons((uint16_t)port);
    a.sin_addr.s_addr = INADDR_ANY;
    ::bind(g_ep_lfd, (struct sockaddr*)&a, sizeof(a));
    ::listen(g_ep_lfd, SOMAXCONN);
    g_ep_run = true;
    std::thread([]{
        int ep = ::epoll_create1(0);
        struct epoll_event ev, evs[256];
        ev.events = EPOLLIN; ev.data.fd = g_ep_lfd;
        ::epoll_ctl(ep, EPOLL_CTL_ADD, g_ep_lfd, &ev);
        while (g_ep_run) {
            int n = ::epoll_wait(ep, evs, 256, 50);
            for (int i = 0; i < n; ++i) {
                int fd = evs[i].data.fd;
                if (fd == g_ep_lfd) {
                    while (1) { int c = ::accept4(g_ep_lfd, 0, 0, SOCK_NONBLOCK);
                        if (c < 0) break;
                        ev.events = EPOLLIN | EPOLLET; ev.data.fd = c;
                        ::epoll_ctl(ep, EPOLL_CTL_ADD, c, &ev);
                    }
                } else {
                    char b[65536];
                    while (1) { ssize_t r = ::recv(fd, b, sizeof(b), 0);
                        if (r > 0) ::send(fd, b, (size_t)r, MSG_NOSIGNAL);
                        else { ::epoll_ctl(ep, EPOLL_CTL_DEL, fd, 0); ::close(fd); break; }
                        if (r < (ssize_t)sizeof(b)) break;
                    }
                }
            }
        }
        ::close(ep);
    }).detach();
    usleep(50000);
}
static void stopEpoll() { g_ep_run = false; usleep(30000); if (g_ep_lfd>=0) ::close(g_ep_lfd); }

// ============================================================
// Libevent echo server
// ============================================================
static struct event_base* g_eb = nullptr;
static struct evconnlistener* g_el = nullptr;
static std::thread g_ev_th;

static void evR(struct bufferevent* bev, void*) {
    evbuffer_add_buffer(bufferevent_get_output(bev), bufferevent_get_input(bev));
}
static void evE(struct bufferevent* bev, short, void*) { bufferevent_free(bev); }
static void evA(struct evconnlistener*, evutil_socket_t fd, struct sockaddr*, int, void*) {
    auto* bv = bufferevent_socket_new(g_eb, fd, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bv, evR, nullptr, evE, nullptr);
    bufferevent_enable(bv, EV_READ|EV_WRITE);
}
static void startLib(int port) {
    g_eb = event_base_new();
    struct sockaddr_in s{};
    s.sin_family = AF_INET; s.sin_port = htons((uint16_t)port);
    s.sin_addr.s_addr = INADDR_ANY;
    g_el = evconnlistener_new_bind(g_eb, evA, nullptr,
        LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1, (struct sockaddr*)&s, sizeof(s));
    g_ev_th = std::thread([]{ event_base_dispatch(g_eb); });
    usleep(50000);
}
static void stopLib() {
    if (g_eb) event_base_loopbreak(g_eb);
    if (g_ev_th.joinable()) g_ev_th.join();
    if (g_el) evconnlistener_free(g_el);
    if (g_eb) event_base_free(g_eb);
    g_el = nullptr; g_eb = nullptr;
}

// ============================================================
// main
// ============================================================
int main() {
    ::signal(SIGPIPE, SIG_IGN);
    mkdir("benchInfo", 0755);

    const int EP_PORT = 19120;
    const int LE_PORT = 19121;

    printf("╔════════════════════════════════════════════════════╗\n");
    printf("║  TCP Echo Matrix: Raw Epoll vs Libevent            ║\n");
    printf("╚════════════════════════════════════════════════════╝\n\n");

    // start both servers
    startEpoll(EP_PORT);
    startLib(LE_PORT);
    printf("epoll echo    :%d  |  libevent echo :%d\n\n", EP_PORT, LE_PORT);

    MatrixRunner runner("TCP Echo QPS: Epoll vs Libevent", "req/s",
                         {1, 2, 4});

    const int MSG = 64;
    const int RDS = 2000;

    runner.addRow("01 raw epoll  ", [=](int th, uint64_t& ops, double& sec) {
        runMultiThread(th, [=](int, std::atomic<uint64_t>& c) {
            echoClient(EP_PORT, MSG, RDS, c);
        }, ops, sec);
    });

    runner.addRow("02 libevent    ", [=](int th, uint64_t& ops, double& sec) {
        runMultiThread(th, [=](int, std::atomic<uint64_t>& c) {
            echoClient(LE_PORT, MSG, RDS, c);
        }, ops, sec);
    });

    runner.run();
    runner.printMatrix();
    runner.saveCsv("benchInfo/net_echo_compare.csv");
    runner.saveMd("benchInfo/net_echo_compare.md");

    stopEpoll();
    stopLib();
    return 0;
}
