// Echo Server — 端到端测试: Scheduler + Reactor + Hook + Socket + Stream
#include "zero/zero.h"
#include "zero/log/log.h"
#include "zero/net/tcp_server.h"
#include "zero/net/socket_stream.h"

#include <cstdio>
#include <cstring>
#include <atomic>

static zero::Logger::ptr g_logger = ZERO_LOG_ROOT();

// 处理单个客户端连接 (在 fiber 中运行)
void handleClient(zero::Socket::ptr sock) {
    zero::SocketStream stream(sock);
    auto remote = sock->getRemoteAddress();
    ZERO_LOG_INFO(g_logger) << "Client connected: " << (remote ? remote->toString() : "?");

    char buf[4096];

    while (true) {
        // 通过 hook 层异步读 (fiber 自动 yield/yield back)
        ssize_t n = stream.read(buf, sizeof(buf));
        if (n <= 0) {
            ZERO_LOG_INFO(g_logger) << "Client disconnected";
            break;
        }

        // Echo back
        ssize_t w = stream.writeFixed(buf, n);
        if (w <= 0) {
            ZERO_LOG_INFO(g_logger) << "Write failed, closing";
            break;
        }
    }

    stream.close();
}

int main(int argc, char** argv) {
    printf("=== Zero Echo Server ===\n");

    // 创建调度器 + Reactor
    zero::Scheduler scheduler(4, false, "echo_sched");
    scheduler.start();

    // 监听地址: ./echo_server [host] [port]  默认 0.0.0.0:8080
    const char* host = "0.0.0.0";
    int port = 8080;
    if (argc == 2) {
        // 仅端口: ./echo_server 8080
        port = atoi(argv[1]);
    } else if (argc >= 3) {
        host = argv[1];
        port = atoi(argv[2]);
    }

    printf("Binding to %s:%d\n", host, port);
    fflush(stdout);

    auto addr = zero::IPv4Address::Create(host, port);
    if (!addr) {
        fprintf(stderr, "Invalid address: %s:%d\n", host, port);
        scheduler.stop();
        return 1;
    }

    // 创建服务器
    auto server = std::make_shared<zero::TcpServer>(&scheduler, addr, "echo");
    server->setConnectionCallback(handleClient);

    if (!server->start()) {
        fprintf(stderr, "Failed to start server on %s:%d\n", host, port);
        scheduler.stop();
        return 1;
    }

    printf("Echo server listening on %s:%d\n", host, port);
    printf("Try: echo 'hello' | nc localhost %d\n", port);
    printf("Press Ctrl+C to stop\n");

    // 等待信号
    while (true) {
        sleep(1);
    }

    server->stop();
    scheduler.stop();
    return 0;
}
