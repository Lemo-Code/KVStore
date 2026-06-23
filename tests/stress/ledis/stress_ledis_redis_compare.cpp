/**
 * stress_ledis_redis_compare.cpp — Ledis vs Redis 高并发命令 QPS 矩阵
 *
 * 横轴: CPU核心数 … 2×CPU核心数
 * 纵轴: 命令 × (ledis / redis)
 * 单位: req/s (QPS, TCP+RESP 往返)
 *
 * Usage: stress_ledis_redis_compare <ledis_port> <redis_port> [output_dir]
 */
#include "bench_utils.h"
#include "matrix.h"
using namespace stress;

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

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
static std::string buildHsetReq(int id) {
    char k[32], f[16], v[16];
    snprintf(k, sizeof(k), "hk_%d", id);
    snprintf(f, sizeof(f), "f%d", id);
    snprintf(v, sizeof(v), "v%d", id);
    char buf[160];
    int n = snprintf(buf, sizeof(buf),
        "*4\r\n$4\r\nHSET\r\n$%zu\r\n%s\r\n$%zu\r\n%s\r\n$%zu\r\n%s\r\n",
        strlen(k), k, strlen(f), f, strlen(v), v);
    return std::string(buf, n);
}
static std::string buildZaddReq(int id) {
    char k[32], m[16], s[8];
    snprintf(k, sizeof(k), "zk_%d", id);
    snprintf(m, sizeof(m), "m%d", id);
    snprintf(s, sizeof(s), "%d", id);
    char buf[160];
    int n = snprintf(buf, sizeof(buf),
        "*4\r\n$4\r\nZADD\r\n$%zu\r\n%s\r\n$%zu\r\n%s\r\n$%zu\r\n%s\r\n",
        strlen(k), k, strlen(s), s, strlen(m), m);
    return std::string(buf, n);
}

static int createConn(int port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
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
    if (hdr.empty()) return false;
    char t = hdr[0];
    if (t == '+' || t == ':' || t == '-') return true;
    if (t == '$') {
        int64_t len = std::stoll(hdr.substr(1, hdr.size() - 3));
        if (len < 0) return true;
        std::string body((size_t)len + 2, '\0');
        size_t got = 0;
        while (got < body.size()) {
            ssize_t n = ::recv(fd, &body[got], body.size() - got, 0);
            if (n <= 0) return false;
            got += (size_t)n;
        }
        return true;
    }
    if (t == '*') {
        int64_t cnt = std::stoll(hdr.substr(1, hdr.size() - 3));
        for (int64_t i = 0; i < cnt; ++i)
            if (!recvResp(fd)) return false;
        return true;
    }
    return false;
}

static void clientWorker(int port, const std::string& req, int ops_per_conn,
                         std::atomic<uint64_t>& total_ops) {
    int fd = createConn(port);
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

static void benchServer(int port, int threads, int total_conns, int ops_per_conn,
                        std::string (*build)(int),
                        uint64_t& ops, double& sec) {
    std::atomic<uint64_t> counter{0};
    auto t0 = nowUs();
    std::vector<std::thread> thrs;
    for (int t = 0; t < threads; ++t) {
        int start = t * total_conns / threads;
        int end   = (t + 1) * total_conns / threads;
        thrs.emplace_back([&, start, end]() {
            for (int c = start; c < end; ++c)
                clientWorker(port, build(c), ops_per_conn, counter);
        });
    }
    for (auto& t : thrs) t.join();
    sec = elapsedSec(t0);
    ops = counter.load();
}

static void prepKeys(int port, int n) {
    int fd = createConn(port);
    if (fd < 0) return;
    for (int i = 0; i < n; ++i) {
        std::string req = buildSetReq(i);
        sendAll(fd, req.data(), req.size());
        recvResp(fd);
    }
    ::close(fd);
}

struct CmdDef {
    const char* name;
    std::string (*build)(int);
    bool need_prep;
};

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <ledis_port> <redis_port> [output_dir]\n", argv[0]);
        return 1;
    }

    int ledis_port = std::atoi(argv[1]);
    int redis_port = std::atoi(argv[2]);
    std::string out_dir = (argc > 3) ? std::string(argv[3]) : defaultBenchOutDir();
    ensureDir(out_dir);
    printf("输出目录: %s\n\n", out_dir.c_str());

    if (createConn(ledis_port) < 0) {
        fprintf(stderr, "ERROR: 无法连接 Ledis :%d\n", ledis_port);
        return 1;
    }
    if (createConn(redis_port) < 0) {
        fprintf(stderr, "ERROR: 无法连接 Redis :%d\n", redis_port);
        return 1;
    }

    CmdDef cmds[] = {
        {"SET",   buildSetReq,   false},
        {"GET",   buildGetReq,   true},
        {"INCR",  buildIncrReq,  false},
        {"LPUSH", buildLpushReq, false},
        {"HSET",  buildHsetReq,  false},
        {"ZADD",  buildZaddReq,  false},
    };

    const int total_conns  = 50;
    const int ops_per_conn = 2000;

    prepKeys(ledis_port, 1000);
    prepKeys(redis_port, 1000);

    auto axis = threadAxisCoreTo2Core();
    MatrixRunner runner("Ledis vs Redis 高并发命令 QPS", "req/s (QPS)", axis);

    for (auto& cmd : cmds) {
        runner.addRow(std::string("ledis ") + cmd.name, [=](int th, uint64_t& ops, double& sec) {
            benchServer(ledis_port, th, total_conns, ops_per_conn, cmd.build, ops, sec);
        });
        runner.addRow(std::string("redis ") + cmd.name, [=](int th, uint64_t& ops, double& sec) {
            benchServer(redis_port, th, total_conns, ops_per_conn, cmd.build, ops, sec);
        });
    }

    runner.run();
    runner.printMatrix();
    runner.saveLog(out_dir + "/ledis_redis_compare.log");
    return 0;
}
