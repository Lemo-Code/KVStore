/**
 * stress_echo_server.cpp — Echo 服务器矩阵压测
 *
 * 纵轴: 客户端连接数 [10, 50, 100, 200]
 * 横轴: threads [1, 2, 4, 6, 8]  (Scheduler 线程数)
 *
 * Usage: stress_echo_server [port] [output_csv_prefix]
 */
#include "bench_utils.h"
#include "matrix.h"
using namespace stress;
#include "zero/zero.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

static int createClient(const char* host, int port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    ::inet_pton(AF_INET, host, &addr.sin_addr);
    struct timeval tv{5, 0};
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    if (::connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ::close(fd); return -1;
    }
    return fd;
}

// ============================================================
// Echo client — 发送 msg, 收完全部回声
// ============================================================
static void echoWorker(const char* host, int port, int num_rounds,
                       int msg_size, std::atomic<uint64_t>& ops) {
    int fd = createClient(host, port);
    if (fd < 0) return;

    std::string msg(msg_size, 'X');
    std::string buf(msg_size, '\0');
    uint64_t local = 0;

    for (int r = 0; r < num_rounds; ++r) {
        // send
        size_t sent = 0;
        while (sent < msg.size()) {
            ssize_t n = ::send(fd, msg.data() + sent, msg.size() - sent, MSG_NOSIGNAL);
            if (n <= 0) goto done;
            sent += (size_t)n;
        }
        // recv echo
        size_t got = 0;
        while (got < msg.size()) {
            ssize_t n = ::recv(fd, &buf[0] + got, msg.size() - got, 0);
            if (n <= 0) goto done;
            got += (size_t)n;
        }
        local++;
    }
done:
    ::close(fd);
    ops.fetch_add(local, std::memory_order_relaxed);
}

// ============================================================
// main
// ============================================================
int main(int argc, char** argv) {
    int port = (argc > 1) ? atoi(argv[1]) : 18000;
    std::string prefix = (argc > 2) ? argv[2] : "benchInfo/echo";

    // 注意: 需要先手动启动 echo server (zero/examples/echo_server 或 echo_minimal)
    // 这个压测只做客户端

    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  Echo Client Matrix  (需要 echo_server 已启动)        ║\n");
    printf("║  Port: %-45d ║\n", port);
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    // 检查连通性
    {
        int probe = createClient("127.0.0.1", port);
        if (probe < 0) {
            fprintf(stderr, "ERROR: cannot connect to 127.0.0.1:%d\n", port);
            fprintf(stderr, "Please start: build/bin/echo_server or build/bin/echo_minimal\n");
            return 1;
        }
        ::close(probe);
        printf("Connected to echo server at 127.0.0.1:%d\n\n", port);
    }

    // 纵轴: msg 大小 × rounds
    struct EchoRow { const char* label; int msg_size; int rounds; };
    EchoRow rows[] = {
        {"64B echo ×100",      64,   100},
        {"64B echo ×1000",     64,   1000},
        {"1KB echo ×100",      1024, 100},
        {"1KB echo ×1000",     1024, 1000},
        {"64KB echo ×10",      65536, 10},
        {"64KB echo ×100",     65536, 100},
    };

    // 横轴: 并发线程 (客户端线程, 不是服务器线程)
    for (auto& er : rows) {
        std::string name = std::string("echo_") + er.label;
        MatrixRunner runner(name);

        // 单连接串行吞吐
        runner.addRow("单连接串行", [&](int threads, uint64_t& ops, double& sec) {
            std::atomic<uint64_t> counter{0};
            auto t0 = stress::nowUs();
            std::vector<std::thread> thrs;
            for (int t = 0; t < threads; ++t)
                thrs.emplace_back(echoWorker, "127.0.0.1", port,
                                  er.rounds, er.msg_size, std::ref(counter));
            for (auto& t : thrs) t.join();
            sec = stress::elapsedSec(t0);
            ops = counter.load();
        });

        // 多连接并发
        runner.addRow("多连接并发", [&](int threads, uint64_t& ops, double& sec) {
            // 每个线程只发一次往返, 但创建 threads*10 个连接
            int total_conns = threads * 10;
            std::atomic<uint64_t> counter{0};
            auto t0 = stress::nowUs();
            std::vector<std::thread> thrs;
            for (int t = 0; t < total_conns; ++t)
                thrs.emplace_back(echoWorker, "127.0.0.1", port,
                                  1, er.msg_size, std::ref(counter));
            for (auto& t : thrs) t.join();
            sec = stress::elapsedSec(t0);
            ops = counter.load();
        });

        runner.run();
        runner.printMatrix();
        runner.saveCsv(prefix + "_" + std::to_string(er.msg_size) + ".csv");
        runner.saveMd(prefix + "_" + std::to_string(er.msg_size) + ".md");
    }

    return 0;
}
