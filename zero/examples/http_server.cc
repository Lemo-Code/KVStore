/**
 * @file    http_server.cc
 * @brief   极简 HTTP/1.1 服务器 — 用于 wrk/ab 压测 zero 网络库
 *
 * 特性:
 *   - 基于 zero 协程 + epoll reactor
 *   - 支持 HTTP keep-alive (单连接多请求)
 *   - 仅处理 GET/HEAD，返回固定响应
 *   - 不做 URL 解析、不读 body
 *
 * 用法:
 *   ./http_server [port] [threads]
 *   默认: 8080 4
 */

#include "zero/zero.h"
#include "zero/log/log.h"
#include "zero/net/socket.h"
#include "zero/net/socket_stream.h"
#include "zero/scheduler/scheduler.h"

#include <cstdio>
#include <cstring>
#include <atomic>

static zero::Logger::ptr g_logger = ZERO_LOG_ROOT();

// 预计算的 HTTP 响应 (避免每次拼接)
static const char kResponse[] =
    "HTTP/1.1 200 OK\r\n"
    "Server: zero\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 5\r\n"
    "Connection: keep-alive\r\n"
    "\r\n"
    "hello";
static constexpr size_t kResponseLen = sizeof(kResponse) - 1;

static const char kErrorResponse[] =
    "HTTP/1.1 400 Bad Request\r\n"
    "Server: zero\r\n"
    "Content-Length: 0\r\n"
    "Connection: close\r\n"
    "\r\n";
static constexpr size_t kErrorLen = sizeof(kErrorResponse) - 1;

// 统计
static std::atomic<uint64_t> g_requests{0};
static std::atomic<uint64_t> g_bytes_sent{0};
static std::atomic<uint64_t> g_connections{0};

// ============================================================
// HTTP 请求解析 — 只找 \r\n\r\n 标记头结束
// ============================================================
static bool findHeaderEnd(const char* buf, size_t len, size_t& end_pos) {
    for (size_t i = 0; i + 3 < len; ++i) {
        if (buf[i] == '\r' && buf[i+1] == '\n' &&
            buf[i+2] == '\r' && buf[i+3] == '\n') {
            end_pos = i + 4;
            return true;
        }
    }
    return false;
}

// ============================================================
// 单连接处理 (在 fiber 中运行)
// ============================================================
void handleClient(zero::Socket::ptr sock) {
    g_connections++;
    zero::SocketStream stream(sock);

    char buf[4096];
    size_t buf_len = 0;
    int keep_alive_requests = 0;
    static constexpr int kMaxKeepAlive = 1000;  // 单连接最大请求数

    while (keep_alive_requests < kMaxKeepAlive) {
        // 读取数据
        ssize_t n = stream.read(buf + buf_len, sizeof(buf) - 1 - buf_len);
        if (n <= 0) break;

        buf_len += n;
        buf[buf_len] = '\0';

        // 查找 HTTP 头结束标记
        size_t end_pos = 0;
        if (!findHeaderEnd(buf, buf_len, end_pos)) {
            // 没找到，继续读 (除非缓冲区满了)
            if (buf_len >= sizeof(buf) - 1 - 128) break;
            continue;
        }

        // 快速检查是否是 GET/HEAD
        if (buf_len < 4 || (memcmp(buf, "GET ", 4) != 0 &&
                            memcmp(buf, "HEAD", 4) != 0)) {
            stream.writeFixed(kErrorResponse, kErrorLen);
            break;
        }

        // 发送响应
        stream.writeFixed(kResponse, kResponseLen);
        g_requests++;
        g_bytes_sent += kResponseLen;
        keep_alive_requests++;

        // 将剩余数据移到缓冲区开头 (管道化请求)
        if (end_pos < buf_len) {
            memmove(buf, buf + end_pos, buf_len - end_pos);
            buf_len -= end_pos;
        } else {
            buf_len = 0;
        }
    }

    stream.close();
}

// ============================================================
// Accept 循环
// ============================================================
void acceptLoop(zero::Socket::ptr listen_sock, zero::Scheduler* sched) {
    while (true) {
        auto client = listen_sock->accept();
        if (!client) continue;
        sched->schedule([client]() { handleClient(client); });
    }
}

// ============================================================
int main(int argc, char** argv) {
    int port = 8080;
    int threads = 4;
    if (argc >= 2) port = atoi(argv[1]);
    if (argc >= 3) threads = atoi(argv[2]);

    // 关闭日志输出避免干扰
    zero::LoggerMgr::GetInstance()->getRoot()->setLevel(zero::LogLevel::ERROR);

    printf("╔══════════════════════════════════╗\n");
    printf("║   Zero HTTP Server (wrk-ready)   ║\n");
    printf("║   Port: %-5d  Threads: %-2d       ║\n", port, threads);
    printf("╚══════════════════════════════════╝\n\n");
    fflush(stdout);

    // 创建调度器
    zero::Scheduler scheduler(threads, false, "http");
    scheduler.start();

    // 创建监听 socket
    auto addr = zero::IPv4Address::Create("0.0.0.0", port);
    auto sock = zero::Socket::CreateTCPSocket();
    int val = 1;
    sock->setOption(SOL_SOCKET, SO_REUSEADDR, val);
    sock->setOption(SOL_SOCKET, SO_REUSEPORT, val);

    if (!sock->bind(addr) || !sock->listen(65535)) {
        fprintf(stderr, "bind/listen failed on port %d\n", port);
        scheduler.stop();
        return 1;
    }

    // 启动 accept
    scheduler.schedule([sock, &scheduler]() mutable {
        acceptLoop(sock, &scheduler);
    });

    printf("Server ready: http://0.0.0.0:%d/\n", port);
    printf("wrk:  wrk -t%d -c200 -d30s http://127.0.0.1:%d/\n", threads, port);
    printf("ab:   ab -n100000 -c200 http://127.0.0.1:%d/\n", port);
    fflush(stdout);

    // 每秒输出统计
    uint64_t last_req = 0;
    for (int i = 0; i < 600; i++) {  // 最多 10 分钟
        sleep(1);
        uint64_t cur = g_requests.load();
        printf("  requests=%lu  rate=%lu/s  conns=%lu\n",
               cur, cur - last_req, g_connections.load());
        fflush(stdout);
        last_req = cur;
    }

    scheduler.stop();
    return 0;
}
