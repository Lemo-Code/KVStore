/**
 * @file    stress_client_mt.cc
 * @brief   多线程 epoll 压测客户端 — 每个线程独立 epoll + 连接子集
 *
 * 消除单线程 epoll 瓶颈，逼近服务端真实 QPS 上限。
 *
 * 用法:
 *   ./stress_client_mt <host> <port> <total_conns> <client_threads> <duration_sec>
 *   默认: 127.0.0.1 8080 800 4 10
 */

#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#include <atomic>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

using namespace std::chrono;

// ============================================================
struct alignas(64) ThreadStats {
    std::atomic<uint64_t> requests_sent{0};
    std::atomic<uint64_t> ok{0};
    std::atomic<uint64_t> data_err{0};
    std::atomic<uint64_t> read_err{0};
    std::atomic<uint64_t> connect_ok{0};
    std::atomic<uint64_t> connect_fail{0};
    std::vector<uint64_t> latencies;
    std::mutex lat_mutex;

    void recordLatency(uint64_t us) {
        std::lock_guard<std::mutex> lk(lat_mutex);
        if (latencies.size() < 100000) latencies.push_back(us);
    }
};

// ============================================================
enum ConnState { CONNECTING, SENDING, RECEIVING, DONE, ERROR };

struct Connection {
    int fd = -1;
    ConnState state = CONNECTING;
    steady_clock::time_point t_send;
    uint32_t conn_id;
    uint64_t seq = 0;
    char send_buf[256];
    size_t send_len = 0;
    size_t send_off = 0;
    char recv_buf[256];
    size_t recv_off = 0;
};

// ============================================================
static void buildMessage(Connection* conn) {
    uint64_t token = (static_cast<uint64_t>(conn->conn_id) << 32) ^
                     (static_cast<uint64_t>(conn->seq) * 0x9E3779B97F4A7C15ULL);
    int len = snprintf(conn->send_buf, sizeof(conn->send_buf),
                       "ECHO-%u-%lu-%016lx\n", conn->conn_id, conn->seq, token);
    conn->send_len = len;
    conn->send_off = 0;
}

static bool verifyEcho(Connection* conn, size_t len) {
    uint64_t expected_token = (static_cast<uint64_t>(conn->conn_id) << 32) ^
                              (static_cast<uint64_t>(conn->seq) * 0x9E3779B97F4A7C15ULL);
    char expected[256];
    int elen = snprintf(expected, sizeof(expected),
                        "ECHO-%u-%lu-%016lx\n", conn->conn_id, conn->seq, expected_token);
    return static_cast<size_t>(elen) == len &&
           memcmp(conn->recv_buf, expected, len) == 0;
}

// ============================================================
void workerThread(int thread_id, const char* host, int port,
                  int conns_per_thread, int duration_sec,
                  int conn_id_start, ThreadStats* stats) {
    // 设置 CPU 亲和性 (轮流绑定)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(thread_id % 4, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    sockaddr_in saddr{};
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    inet_pton(AF_INET, host, &saddr.sin_addr);

    int epfd = epoll_create1(EPOLL_CLOEXEC);
    std::vector<Connection> conns(conns_per_thread);

    // ---- Phase 1: Connect ----
    auto t0 = steady_clock::now();
    int pending = 0;

    for (int i = 0; i < conns_per_thread; ++i) {
        int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (fd < 0) { stats->connect_fail++; continue; }

        int val = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val));

        auto& c = conns[i];
        c.fd = fd;
        c.conn_id = conn_id_start + i;
        c.t_send = t0;

        int ret = connect(fd, (sockaddr*)&saddr, sizeof(saddr));
        if (ret == 0) {
            c.state = SENDING;
            stats->connect_ok++;
            // Prepare and register
            buildMessage(&c);
            epoll_event ev{};
            ev.events = EPOLLOUT | EPOLLET;
            ev.data.ptr = &c;
            epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
        } else if (errno == EINPROGRESS) {
            c.state = CONNECTING;
            pending++;
            epoll_event ev{};
            ev.events = EPOLLOUT | EPOLLET;
            ev.data.ptr = &c;
            epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
        } else {
            c.state = ERROR;
            stats->connect_fail++;
            close(fd); c.fd = -1;
        }
    }

    // 等待连接完成
    if (pending > 0) {
        epoll_event events[256];
        auto deadline = t0 + milliseconds(2000);
        while (pending > 0 && steady_clock::now() < deadline) {
            int remaining = duration_cast<milliseconds>(deadline - steady_clock::now()).count();
            int n = epoll_wait(epfd, events, 256, std::min(remaining, 100));
            if (n <= 0) break;
            for (int i = 0; i < n; ++i) {
                Connection* c = static_cast<Connection*>(events[i].data.ptr);
                if (c->state != CONNECTING) continue;
                int err = 0; socklen_t elen = sizeof(err);
                getsockopt(c->fd, SOL_SOCKET, SO_ERROR, &err, &elen);
                if (err == 0) {
                    c->state = SENDING;
                    stats->connect_ok++;
                    pending--;
                    buildMessage(c);
                    epoll_event ev{};
                    ev.events = EPOLLOUT | EPOLLET;
                    ev.data.ptr = c;
                    epoll_ctl(epfd, EPOLL_CTL_MOD, c->fd, &ev);
                } else {
                    c->state = ERROR;
                    stats->connect_fail++;
                    pending--;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, nullptr);
                    close(c->fd); c->fd = -1;
                }
            }
        }
    }

    // ---- Phase 2: Stress ----
    epoll_event events[512];
    auto t_start = steady_clock::now();
    auto t_deadline = t_start + seconds(duration_sec);

    while (true) {
        auto now = steady_clock::now();
        if (now >= t_deadline) break;
        int rem = duration_cast<milliseconds>(t_deadline - now).count();
        int n = epoll_wait(epfd, events, 512, std::min(rem, 50));
        if (n < 0) { if (errno == EINTR) continue; break; }

        for (int i = 0; i < n; ++i) {
            Connection* c = static_cast<Connection*>(events[i].data.ptr);
            uint32_t rev = events[i].events;

            if (rev & (EPOLLERR | EPOLLHUP)) {
                c->state = ERROR;
                epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, nullptr);
                close(c->fd); c->fd = -1;
                continue;
            }

            if (rev & EPOLLOUT) {
                while (c->send_off < c->send_len) {
                    ssize_t nw = ::send(c->fd, c->send_buf + c->send_off,
                                       c->send_len - c->send_off, MSG_NOSIGNAL);
                    if (nw < 0) { if (errno == EAGAIN) break; goto conn_err; }
                    c->send_off += nw;
                }
                if (c->send_off >= c->send_len) {
                    c->state = RECEIVING;
                    c->t_send = steady_clock::now();
                    c->recv_off = 0;
                    stats->requests_sent++;
                    epoll_event ev{};
                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.ptr = c;
                    epoll_ctl(epfd, EPOLL_CTL_MOD, c->fd, &ev);
                }
            }

            if (rev & EPOLLIN) {
                while (true) {
                    ssize_t nr = ::recv(c->fd, c->recv_buf + c->recv_off,
                                       sizeof(c->recv_buf) - 1 - c->recv_off, 0);
                    if (nr < 0) { if (errno == EAGAIN) break; goto conn_err; }
                    if (nr == 0) goto conn_err;
                    c->recv_off += nr;
                    if (c->recv_off > 0 && c->recv_buf[c->recv_off - 1] == '\n') {
                        auto lat = duration_cast<microseconds>(steady_clock::now() - c->t_send).count();
                        stats->recordLatency(lat);
                        if (verifyEcho(c, c->recv_off)) stats->ok++;
                        else stats->data_err++;
                        c->seq++; c->recv_off = 0; c->state = SENDING;
                        buildMessage(c);
                        epoll_event ev{};
                        ev.events = EPOLLOUT | EPOLLET;
                        ev.data.ptr = c;
                        epoll_ctl(epfd, EPOLL_CTL_MOD, c->fd, &ev);
                        break;
                    }
                }
            }
            continue;

        conn_err:
            c->state = ERROR; stats->read_err++;
            epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, nullptr);
            close(c->fd); c->fd = -1;
        }
    }

    // Cleanup
    for (auto& c : conns) {
        if (c.fd >= 0) { epoll_ctl(epfd, EPOLL_CTL_DEL, c.fd, nullptr); close(c.fd); }
    }
    close(epfd);
}

// ============================================================
int main(int argc, char** argv) {
    const char* host   = (argc > 1) ? argv[1] : "127.0.0.1";
    int port           = (argc > 2) ? atoi(argv[2]) : 8080;
    int total_conns    = (argc > 3) ? atoi(argv[3]) : 800;
    int client_threads = (argc > 4) ? atoi(argv[4]) : 4;
    int duration_sec   = (argc > 5) ? atoi(argv[5]) : 10;

    printf("╔══════════════════════════════════════╗\n");
    printf("║  Zero Stress Client (Multi-Thread)   ║\n");
    printf("╠══════════════════════════════════════╣\n");
    printf("║ Target:   %s:%d\n", host, port);
    printf("║ Conns:    %d\n", total_conns);
    printf("║ Clients:  %d threads\n", client_threads);
    printf("║ Duration: %d s\n", duration_sec);
    printf("╚══════════════════════════════════════╝\n\n");
    fflush(stdout);

    int conns_per = total_conns / client_threads;

    std::vector<std::thread> threads;
    std::vector<ThreadStats> tstats(client_threads);

    auto t0 = steady_clock::now();

    for (int t = 0; t < client_threads; ++t) {
        threads.emplace_back(workerThread, t, host, port,
                             conns_per, duration_sec,
                             t * conns_per, &tstats[t]);
    }
    for (auto& th : threads) th.join();

    auto t1 = steady_clock::now();
    double elapsed = duration_cast<microseconds>(t1 - t0).count() / 1e6;

    // ---- Aggregate ----
    uint64_t total_ok = 0, total_data_err = 0, total_read_err = 0;
    uint64_t total_conn_ok = 0, total_conn_fail = 0, total_sent = 0;
    std::vector<uint64_t> all_lats;

    for (auto& ts : tstats) {
        total_ok += ts.ok.load();
        total_data_err += ts.data_err.load();
        total_read_err += ts.read_err.load();
        total_conn_ok += ts.connect_ok.load();
        total_conn_fail += ts.connect_fail.load();
        total_sent += ts.requests_sent.load();
        all_lats.insert(all_lats.end(), ts.latencies.begin(), ts.latencies.end());
    }
    uint64_t total_req = total_ok + total_data_err + total_read_err;

    printf("\n");
    printf("╔══════════════════════════════════════════╗\n");
    printf("║       Multi-Thread Stress Results        ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║ Duration:     %8.2f s                  ║\n", elapsed);
    printf("║ Conns:        %8lu / %-6d             ║\n", total_conn_ok, total_conns);
    printf("║ Connect fail: %8lu                     ║\n", total_conn_fail);
    printf("╠══════════════════════════════════════════╣\n");
    printf("║ Total reqs:   %10lu                   ║\n", total_req);
    printf("║ OK:           %10lu  (%.4f%%)         ║\n", total_ok,
           total_req > 0 ? 100.0*total_ok/total_req : 0);
    printf("║ Data errors:  %10lu                   ║\n", total_data_err);
    printf("║ Read errors:  %10lu                   ║\n", total_read_err);
    printf("╠══════════════════════════════════════════╣\n");
    printf("║ Throughput:   %8.0f req/s              ║\n", total_ok / elapsed);
    printf("║ Bandwidth:    %8.2f MB/s               ║\n",
           (total_ok * 38.0) / elapsed / 1048576);
    printf("╠══════════════════════════════════════════╣\n");

    if (!all_lats.empty()) {
        std::sort(all_lats.begin(), all_lats.end());
        size_t n = all_lats.size();
        uint64_t sum = 0;
        for (auto l : all_lats) sum += l;
        printf("║ Latency samples: %zu\n", n);
        printf("║ Avg:  %7.0f us                        ║\n", (double)sum/n);
        printf("║ P50:  %7lu us                        ║\n", all_lats[n/2]);
        printf("║ P90:  %7lu us                        ║\n", all_lats[n*90/100]);
        printf("║ P99:  %7lu us                        ║\n", all_lats[n*99/100]);
        printf("║ P999: %7lu us                        ║\n", all_lats[n*999/1000]);
        printf("║ Min:  %7lu us                        ║\n", all_lats[0]);
        printf("║ Max:  %7lu us                        ║\n", all_lats[n-1]);
    }

    printf("╚══════════════════════════════════════════╝\n");

    if (total_data_err || total_read_err)
        printf("\n✗ ERRORS: data=%lu read=%lu\n", total_data_err, total_read_err);
    else
        printf("\n✓ ALL CHECKS PASSED — %lu requests, zero errors\n", total_req);

    return (total_data_err + total_read_err) ? 1 : 0;
}
