/**
 * @file    stress_client.cc
 * @brief   高并发 epoll 压力测试客户端 — 800+ 连接，数据完整性校验
 *
 * 特性:
 *   - 纯 epoll (ET) 事件驱动，单线程管理所有连接
 *   - 每连接发送带唯一标记的数据，回显后校验完整性
 *   - 统计: 成功/失败/数据错误/超时，P50/P90/P99/P999 延迟
 *   - 实时输出进度
 *
 * 用法:
 *   ./stress_client <host> <port> <connections> <duration_sec>
 *   默认: 127.0.0.1 8080 800 10
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
#include <vector>
#include <map>

using namespace std::chrono;

// ============================================================
// 全局统计
// ============================================================
struct GlobalStats {
    std::atomic<uint64_t> connect_attempts{0};
    std::atomic<uint64_t> connect_ok{0};
    std::atomic<uint64_t> connect_fail{0};

    std::atomic<uint64_t> requests_sent{0};
    std::atomic<uint64_t> responses_ok{0};        // echo 正确
    std::atomic<uint64_t> responses_data_err{0};  // echo 内容不匹配
    std::atomic<uint64_t> responses_read_err{0};  // read 返回 <=0
    std::atomic<uint64_t> responses_timeout{0};

    std::atomic<uint64_t> total_bytes_sent{0};
    std::atomic<uint64_t> total_bytes_recv{0};

    // 延迟采样 (us)，最多存 500k 条
    static constexpr size_t kMaxLatSamples = 500000;
    std::vector<uint64_t> latencies;
    std::mutex lat_mutex;
    void recordLatency(uint64_t us) {
        std::lock_guard<std::mutex> lk(lat_mutex);
        if (latencies.size() < kMaxLatSamples)
            latencies.push_back(us);
    }
};

// ============================================================
// 连接状态机
// ============================================================
enum ConnState { CONNECTING, SENDING, RECEIVING, DONE, ERROR };

struct Connection {
    int fd = -1;
    ConnState state = CONNECTING;
    steady_clock::time_point t_connect;
    steady_clock::time_point t_send;
    uint32_t conn_id = 0;
    uint64_t seq = 0;           // 当前消息序号

    // 发送缓冲区
    char send_buf[256];
    size_t send_len = 0;
    size_t send_off = 0;

    // 接收缓冲区
    char recv_buf[256];
    size_t recv_off = 0;

    uint64_t total_OK = 0;
    uint64_t total_data_err = 0;
    uint64_t total_read_err = 0;
};

// ============================================================
// 工具
// ============================================================
static int setNonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void buildMessage(Connection* conn) {
    // 格式: "ECHO-<conn_id>-<seq>-<random_token>"
    // random_token 用于校验回显完整性
    uint64_t token = (static_cast<uint64_t>(conn->conn_id) << 32) ^
                     (static_cast<uint64_t>(conn->seq) * 0x9E3779B97F4A7C15ULL);
    int len = snprintf(conn->send_buf, sizeof(conn->send_buf),
                       "ECHO-%u-%lu-%016lx\n",
                       conn->conn_id, conn->seq, token);
    conn->send_len = len;
    conn->send_off = 0;
}

static bool verifyEcho(Connection* conn, size_t len) {
    // 期望格式同 buildMessage
    uint64_t expected_token = (static_cast<uint64_t>(conn->conn_id) << 32) ^
                              (static_cast<uint64_t>(conn->seq) * 0x9E3779B97F4A7C15ULL);
    char expected[256];
    int elen = snprintf(expected, sizeof(expected),
                        "ECHO-%u-%lu-%016lx\n",
                        conn->conn_id, conn->seq, expected_token);
    if (static_cast<size_t>(elen) != len) return false;
    return memcmp(conn->recv_buf, expected, len) == 0;
}

// ============================================================
// 主函数
// ============================================================
int main(int argc, char** argv) {
    const char* host   = (argc > 1) ? argv[1] : "127.0.0.1";
    int port           = (argc > 2) ? atoi(argv[2]) : 8080;
    int total_conns    = (argc > 3) ? atoi(argv[3]) : 800;
    int duration_sec   = (argc > 4) ? atoi(argv[4]) : 10;

    printf("╔══════════════════════════════════════╗\n");
    printf("║   Zero Stress Client (epoll)         ║\n");
    printf("╠══════════════════════════════════════╣\n");
    printf("║ Target:   %s:%d\n", host, port);
    printf("║ Conns:    %d\n", total_conns);
    printf("║ Duration: %d s\n", duration_sec);
    printf("║ Verify:   full echo integrity check\n");
    printf("╚══════════════════════════════════════╝\n\n");
    setbuf(stdout, NULL);

    // ---- 预解析地址 ----
    sockaddr_in saddr{};
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &saddr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid host: %s\n", host);
        return 1;
    }

    // ---- epoll ----
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) { perror("epoll_create1"); return 1; }

    // ---- 统计 ----
    GlobalStats stats;

    // ---- 阶段1: 建立所有连接 (非阻塞 connect) ----
    printf("[Phase 1] Connecting %d clients...\n", total_conns);
    auto t_phase1 = steady_clock::now();

    std::vector<Connection> conns(total_conns);
    int connected = 0;
    int in_progress = 0;

    for (int i = 0; i < total_conns; ++i) {
        stats.connect_attempts++;

        int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (fd < 0) { stats.connect_fail++; continue; }

        int val = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val));

        conns[i].fd = fd;
        conns[i].conn_id = i;
        conns[i].t_connect = steady_clock::now();

        int ret = connect(fd, (sockaddr*)&saddr, sizeof(saddr));
        if (ret == 0) {
            // 立即连接成功 (localhost)
            conns[i].state = SENDING;
            connected++;
            stats.connect_ok++;
        } else if (errno == EINPROGRESS) {
            conns[i].state = CONNECTING;
            in_progress++;

            epoll_event ev{};
            ev.events = EPOLLOUT | EPOLLET;
            ev.data.ptr = &conns[i];
            epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
        } else {
            conns[i].state = ERROR;
            stats.connect_fail++;
            close(fd);
            conns[i].fd = -1;
        }
    }

    // 等待所有 in-progress 连接完成
    if (in_progress > 0) {
        int timeout_ms = 2000;
        auto deadline = steady_clock::now() + milliseconds(timeout_ms);
        epoll_event events[256];

        while (in_progress > 0 && steady_clock::now() < deadline) {
            int remaining = duration_cast<milliseconds>(deadline - steady_clock::now()).count();
            if (remaining < 1) break;
            int n = epoll_wait(epfd, events, 256, std::min(remaining, 100));
            if (n < 0) { if (errno == EINTR) continue; break; }

            for (int i = 0; i < n; ++i) {
                Connection* c = static_cast<Connection*>(events[i].data.ptr);
                if (c->state != CONNECTING) continue;

                int err = 0;
                socklen_t len = sizeof(err);
                getsockopt(c->fd, SOL_SOCKET, SO_ERROR, &err, &len);

                if (err == 0) {
                    c->state = SENDING;
                    connected++;
                    stats.connect_ok++;
                } else {
                    c->state = ERROR;
                    stats.connect_fail++;
                    close(c->fd); c->fd = -1;
                }
                in_progress--;
            }
        }

        // 超时未完成的标记为失败
        if (in_progress > 0) {
            for (auto& c : conns) {
                if (c.state == CONNECTING) {
                    c.state = ERROR;
                    stats.connect_fail++;
                    close(c.fd); c.fd = -1;
                }
            }
            in_progress = 0;
        }
    }

    auto t_phase1_end = steady_clock::now();
    double phase1_sec = duration_cast<microseconds>(t_phase1_end - t_phase1).count() / 1e6;
    printf("  Connected: %d/%d (%.1f ms)  Failed: %lu\n",
           connected, total_conns, phase1_sec * 1000, stats.connect_fail.load());

    if (connected == 0) {
        fprintf(stderr, "No connections. Server running?\n");
        close(epfd);
        return 1;
    }

    // ---- 阶段2: 压力测试 ----
    printf("\n[Phase 2] Stress test (%d s) with full data integrity check...\n", duration_sec);

    // 将所有已连接 fd 注册到 epoll (EPOLLOUT 发首条消息, EPOLLIN 接收)
    // 重新注册: 先添加 EPOLLOUT（用于发送首条），后续在事件循环中切换
    for (auto& c : conns) {
        if (c.state != SENDING) continue;

        // 准备首条消息
        buildMessage(&c);

        epoll_event ev{};
        ev.events = EPOLLOUT | EPOLLET;
        ev.data.ptr = &c;

        // 如果之前已添加 (CONNECTING 阶段的), 用 MOD; 否则 ADD
        // 这里统一用 ADD (对于已经在 epoll 中的会用 EEXIST, 但我们可以先 DEL)
        // 简化: 全部 DEL 再 ADD
        epoll_ctl(epfd, EPOLL_CTL_DEL, c.fd, nullptr);  // ignore error
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, c.fd, &ev) < 0) {
            c.state = ERROR;
            close(c.fd); c.fd = -1;
        }
    }

    auto t_start = steady_clock::now();
    auto t_deadline = t_start + seconds(duration_sec);
    auto t_next_report = t_start + seconds(1);

    epoll_event events[512];
    uint64_t last_ok = 0;

    while (true) {
        auto now = steady_clock::now();
        if (now >= t_deadline) break;

        int remaining = duration_cast<milliseconds>(t_deadline - now).count();
        if (remaining < 0) remaining = 0;
        if (remaining > 100) remaining = 100;

        int n = epoll_wait(epfd, events, 512, remaining);
        now = steady_clock::now();

        if (n < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait"); break;
        }

        for (int i = 0; i < n; ++i) {
            Connection* c = static_cast<Connection*>(events[i].data.ptr);
            uint32_t revents = events[i].events;

            if (revents & (EPOLLERR | EPOLLHUP)) {
                c->state = ERROR;
                epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, nullptr);
                close(c->fd); c->fd = -1;
                continue;
            }

            if (revents & EPOLLOUT) {
                // 发送数据
                while (c->send_off < c->send_len) {
                    ssize_t nw = ::send(c->fd, c->send_buf + c->send_off,
                                       c->send_len - c->send_off, MSG_NOSIGNAL);
                    if (nw < 0) {
                        if (errno == EAGAIN) break;
                        c->state = ERROR; c->total_read_err++;
                        epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, nullptr);
                        close(c->fd); c->fd = -1;
                        break;
                    }
                    c->send_off += nw;
                    stats.total_bytes_sent += nw;
                }

                if (c->state == ERROR) continue;

                if (c->send_off >= c->send_len) {
                    // 发送完毕，切换到接收
                    c->state = RECEIVING;
                    c->t_send = now;
                    c->recv_off = 0;
                    stats.requests_sent++;

                    epoll_event ev{};
                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.ptr = c;
                    epoll_ctl(epfd, EPOLL_CTL_MOD, c->fd, &ev);
                }
            }

            if (revents & EPOLLIN) {
                // 接收数据
                while (true) {
                    ssize_t nr = ::recv(c->fd,
                                        c->recv_buf + c->recv_off,
                                        sizeof(c->recv_buf) - 1 - c->recv_off, 0);
                    if (nr < 0) {
                        if (errno == EAGAIN) break;
                        c->total_read_err++;
                        stats.responses_read_err++;
                        c->state = ERROR;
                        epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, nullptr);
                        close(c->fd); c->fd = -1;
                        break;
                    }
                    if (nr == 0) {
                        // EOF
                        c->total_read_err++;
                        stats.responses_read_err++;
                        c->state = ERROR;
                        epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, nullptr);
                        close(c->fd); c->fd = -1;
                        break;
                    }

                    c->recv_off += nr;
                    stats.total_bytes_recv += nr;

                    // 检查是否收到完整消息 (以 \n 结尾)
                    if (c->recv_off > 0 && c->recv_buf[c->recv_off - 1] == '\n') {
                        auto t_now = steady_clock::now();
                        auto lat_us = duration_cast<microseconds>(t_now - c->t_send).count();
                        stats.recordLatency(lat_us);

                        // 校验数据
                        if (verifyEcho(c, c->recv_off)) {
                            stats.responses_ok++;
                            c->total_OK++;
                        } else {
                            stats.responses_data_err++;
                            c->total_data_err++;
                        }

                        // 准备下一条消息
                        c->seq++;
                        buildMessage(c);
                        c->state = SENDING;
                        c->recv_off = 0;

                        epoll_event ev{};
                        ev.events = EPOLLOUT | EPOLLET;
                        ev.data.ptr = c;
                        epoll_ctl(epfd, EPOLL_CTL_MOD, c->fd, &ev);
                        break;
                    }
                }
            }
        }

        // 进度报告 (每秒)
        if (now >= t_next_report) {
            uint64_t cur_ok = stats.responses_ok.load();
            double elapsed = duration_cast<microseconds>(now - t_start).count() / 1e6;
            printf("  [%3.0fs] OK:%lu  data_err:%lu  read_err:%lu  rate:%.0f req/s\n",
                   elapsed, cur_ok,
                   stats.responses_data_err.load(),
                   stats.responses_read_err.load(),
                   (cur_ok - last_ok) / 1.0);
            last_ok = cur_ok;
            t_next_report = now + seconds(1);
        }
    }

    auto t_end = steady_clock::now();
    double elapsed = duration_cast<microseconds>(t_end - t_start).count() / 1e6;

    // ---- 关闭所有连接 ----
    for (auto& c : conns) {
        if (c.fd >= 0) {
            epoll_ctl(epfd, EPOLL_CTL_DEL, c.fd, nullptr);
            close(c.fd);
        }
    }
    close(epfd);

    // ---- 报告 ----
    uint64_t ok    = stats.responses_ok.load();
    uint64_t d_err = stats.responses_data_err.load();
    uint64_t r_err = stats.responses_read_err.load();
    uint64_t total = ok + d_err + r_err;

    printf("\n");
    printf("╔══════════════════════════════════════════╗\n");
    printf("║        Stress Test Results               ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║ Duration:     %8.2f s                  ║\n", elapsed);
    printf("║ Conns:        %8d / %-6d             ║\n", connected, total_conns);
    printf("║ Connect fail: %8lu                     ║\n", stats.connect_fail.load());
    printf("╠══════════════════════════════════════════╣\n");
    printf("║ Total reqs:   %10lu                   ║\n", total);
    printf("║ OK:           %10lu  (%.4f%%)         ║\n", ok, total > 0 ? 100.0 * ok / total : 0);
    printf("║ Data errors:  %10lu  (%.4f%%)         ║\n", d_err, total > 0 ? 100.0 * d_err / total : 0);
    printf("║ Read errors:  %10lu  (%.4f%%)         ║\n", r_err, total > 0 ? 100.0 * r_err / total : 0);
    printf("╠══════════════════════════════════════════╣\n");
    printf("║ Throughput:   %8.0f req/s              ║\n", ok / elapsed);
    printf("║ Bandwidth:    %8.2f MB/s               ║\n",
           (stats.total_bytes_sent + stats.total_bytes_recv) / elapsed / 1048576);
    printf("╠══════════════════════════════════════════╣\n");

    // 延迟统计
    if (!stats.latencies.empty()) {
        std::sort(stats.latencies.begin(), stats.latencies.end());
        size_t n = stats.latencies.size();
        uint64_t sum = 0;
        for (auto l : stats.latencies) sum += l;
        printf("║ Latency samples: %zu\n", n);
        printf("║ Avg:  %7.0f us                        ║\n", (double)sum / n);
        printf("║ P50:  %7lu us                        ║\n", stats.latencies[n/2]);
        printf("║ P90:  %7lu us                        ║\n", stats.latencies[n*90/100]);
        printf("║ P99:  %7lu us                        ║\n", stats.latencies[n*99/100]);
        printf("║ P999: %7lu us                        ║\n", stats.latencies[n*999/1000]);
        printf("║ Min:  %7lu us                        ║\n", stats.latencies[0]);
        printf("║ Max:  %7lu us                        ║\n", stats.latencies[n-1]);
    }

    // 每连接统计摘要
    uint64_t min_per_conn = ~0ull, max_per_conn = 0;
    double avg_per_conn = 0;
    int alive = 0;
    for (auto& c : conns) {
        if (c.fd >= 0 || c.total_OK > 0) {
            alive++;
            uint64_t c_total = c.total_OK + c.total_data_err + c.total_read_err;
            if (c_total < min_per_conn) min_per_conn = c_total;
            if (c_total > max_per_conn) max_per_conn = c_total;
            avg_per_conn += c_total;
        }
    }
    if (alive > 0) avg_per_conn /= alive;
    printf("╠══════════════════════════════════════════╣\n");
    printf("║ Per-connection (active: %d):\n", alive);
    printf("║   Avg: %.0f reqs\n", avg_per_conn);
    printf("║   Min: %lu reqs\n", min_per_conn);
    printf("║   Max: %lu reqs\n", max_per_conn);
    printf("╚══════════════════════════════════════════╝\n");

    // 数据完整性最终判断
    if (d_err == 0 && r_err == 0) {
        printf("\n✓ ALL CHECKS PASSED — zero data errors, zero read errors\n");
    } else {
        printf("\n✗ ERRORS DETECTED — data_err=%lu read_err=%lu\n", d_err, r_err);
    }

    return (d_err + r_err) > 0 ? 1 : 0;
}
