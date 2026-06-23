/**
 * stress_ledis_net.cpp — Ledis 网络矩阵压测 (TCP RESP)
 *
 * 纵轴: 并发连接数 [10, 50, 100, 200]
 * 横轴: threads [1, 2, 4, 6, 8]  (压测客户端线程)
 *
 * 要求: ledis-server 已启动在指定端口
 *
 * Usage: stress_ledis_net <host> <port> [output_csv_prefix]
 */
#include "bench_utils.h"
#include "matrix.h"
using namespace stress;

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <string>

// ============================================================
// 构建 RESP 请求
// ============================================================
static std::string buildSetReq(int id) {
    char k[32], v[32];
    snprintf(k, sizeof(k), "bk_%d", id);
    snprintf(v, sizeof(v), "bv_%d", id);
    char buf[128];
    int n = snprintf(buf, sizeof(buf),
        "*3\r\n$3\r\nSET\r\n$%zu\r\n%s\r\n$%zu\r\n%s\r\n",
        strlen(k), k, strlen(v), v);
    return std::string(buf, n);
}

static std::string buildGetReq(int id) {
    char k[32];
    snprintf(k, sizeof(k), "bk_%d", id);
    char buf[64];
    int n = snprintf(buf, sizeof(buf),
        "*2\r\n$3\r\nGET\r\n$%zu\r\n%s\r\n", strlen(k), k);
    return std::string(buf, n);
}

static std::string buildIncrReq(int id) {
    char k[32];
    snprintf(k, sizeof(k), "cnt_%d", id);
    char buf[64];
    int n = snprintf(buf, sizeof(buf),
        "*2\r\n$4\r\nINCR\r\n$%zu\r\n%s\r\n", strlen(k), k);
    return std::string(buf, n);
}

static std::string buildLpushReq(int id) {
    char k[32], v[16];
    snprintf(k, sizeof(k), "lk_%d", id);
    snprintf(v, sizeof(v), "v%d", id);
    char buf[128];
    int n = snprintf(buf, sizeof(buf),
        "*3\r\n$5\r\nLPUSH\r\n$%zu\r\n%s\r\n$%zu\r\n%s\r\n",
        strlen(k), k, strlen(v), v);
    return std::string(buf, n);
}

// ============================================================
// TCP 客户端 — 发请求, 收响应
// ============================================================
static int createConn(const char* host, int port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (::inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        ::close(fd); return -1;
    }

    struct timeval tv{5, 0};
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (::connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ::close(fd); return -1;
    }
    return fd;
}

static bool sendAll(int fd, const void* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = ::send(fd, (const char*)data + sent, len - sent, MSG_NOSIGNAL);
        if (n <= 0) return false;
        sent += (size_t)n;
    }
    return true;
}

static bool recvAll(int fd, void* buf, size_t len) {
    size_t got = 0;
    while (got < len) {
        ssize_t n = ::recv(fd, (char*)buf + got, len - got, 0);
        if (n <= 0) return false;
        got += (size_t)n;
    }
    return true;
}

static bool recvLine(int fd, std::string& line) {
    line.clear();
    char c;
    while (true) {
        if (::recv(fd, &c, 1, 0) != 1) return false;
        line += c;
        if (line.size() >= 2 && line[line.size()-2] == '\r' && line.back() == '\n')
            break;
    }
    return true;
}

static bool recvResp(int fd) {
    std::string hdr;
    if (!recvLine(fd, hdr)) return false;
    if (hdr.empty() || hdr[0] != '$') return (hdr[0] == '+' || hdr[0] == ':');

    // bulk string: $len\r\n<data>\r\n
    int64_t len = std::stoll(hdr.substr(1, hdr.size() - 3));
    if (len < 0) return true;
    std::string body;
    body.resize((size_t)len + 2);
    return recvAll(fd, &body[0], body.size());
}

// ============================================================
// 并发客户端 worker
// ============================================================
static void clientWorker(const char* host, int port,
                         const std::string& req, int ops_per_conn,
                         std::atomic<uint64_t>& total_ops) {
    int fd = createConn(host, port);
    if (fd < 0) return;

    uint64_t local = 0;
    for (int i = 0; i < ops_per_conn; ++i) {
        if (!sendAll(fd, req.data(), req.size())) break;
        if (!recvResp(fd)) break;
        local++;
    }
    ::close(fd);
    total_ops.fetch_add(local, std::memory_order_relaxed);
}

// ============================================================
// main
// ============================================================
int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <host> <port> [csv_prefix]\n", argv[0]);
        return 1;
    }

    const char* host = argv[1];
    int port = atoi(argv[2]);
    std::string prefix = (argc > 3) ? argv[3] : "benchInfo/ledis_net";

    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  Ledis Network Matrix  (%s:%d)                       ║\n", host, port);
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    // 预热连接
    {
        int warm = createConn(host, port);
        if (warm < 0) {
            fprintf(stderr, "ERROR: cannot connect to %s:%d\n", host, port);
            return 1;
        }
        ::close(warm);
    }

    // 纵轴: 并发连接数
    struct ConnRow { const char* label; int conns; int ops_per_conn; };
    ConnRow conn_rows[] = {
        {"10 conns × 1K op",   10,   1000},
        {"50 conns × 1K op",   50,   1000},
        {"100 conns × 1K op",  100,  1000},
        {"200 conns × 1K op",  200,  1000},
    };

    struct Cmd { const char* name; std::string (*builder)(int); };
    Cmd cmds[] = {
        {"SET",   buildSetReq},
        {"GET",   buildGetReq},
        {"INCR",  buildIncrReq},
        {"LPUSH", buildLpushReq},
    };

    for (auto& cmd : cmds) {
        std::string bench_name = std::string("ledis_net_") + cmd.name;
        MatrixRunner runner(bench_name);

        // Pre-populate keys for GET
        if (cmd.name[0] == 'G') {
            int pop = createConn(host, port);
            if (pop >= 0) {
                for (int i = 0; i < 1000; ++i) {
                    std::string set_req = buildSetReq(i);
                    sendAll(pop, set_req.data(), set_req.size());
                    recvResp(pop);
                }
                ::close(pop);
            }
        }

        for (auto& cr : conn_rows) {
            std::string label = std::string(cmd.name) + " " + cr.label;
            std::string req   = cmd.builder(0);

            runner.addRow(label, [&](int threads, uint64_t& ops, double& sec) {
                // 每个连接独立发送 ops_per_conn 个请求
                int total_conns = cr.conns;
                std::vector<std::string> requests;
                // 为不同连接使用不同 key (避免热点)
                for (int c = 0; c < total_conns; ++c)
                    requests.push_back(cmd.builder(c));

                std::atomic<uint64_t> counter{0};

                auto t0 = stress::nowUs();
                std::vector<std::thread> thrs;
                // 均匀分配连接数到各线程
                for (int t = 0; t < threads; ++t) {
                    int start = t * total_conns / threads;
                    int end   = (t + 1) * total_conns / threads;
                    thrs.emplace_back([&, start, end]() {
                        for (int c = start; c < end; ++c) {
                            clientWorker(host, port, requests[c],
                                        cr.ops_per_conn, counter);
                        }
                    });
                }
                for (auto& t : thrs) t.join();
                sec = stress::elapsedSec(t0);
                ops = counter.load();
            });
        }

        runner.run();
        runner.printMatrix();

        std::string csv = prefix + "_" + cmd.name + ".csv";
        std::string md  = prefix + "_" + cmd.name + ".md";
        runner.saveCsv(csv);
        runner.saveMd(md);
    }

    return 0;
}
