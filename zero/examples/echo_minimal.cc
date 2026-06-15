// 最小化 echo 测试: 直接 Socket + Scheduler + Hook
#include "zero/zero.h"
#include "zero/scheduler/reactor.h"
#include "zero/log/log.h"
#include "zero/net/socket.h"
#include "zero/net/socket_stream.h"

#include <cstdio>
#include <cstring>

static zero::Logger::ptr g_logger = ZERO_LOG_ROOT();

void handleClient(zero::Socket::ptr sock) {
    zero::SocketStream stream(sock);
    char buf[4096];

    // 简单高效的 echo: 读到什么回显什么
    while (true) {
        ssize_t n = stream.read(buf, sizeof(buf));
        if (n <= 0) break;
        if (stream.writeFixed(buf, n) <= 0) break;
    }
    stream.close();
}

void acceptLoop(zero::Socket::ptr listen_sock, zero::Scheduler* sched) {
    ZERO_LOG_INFO(g_logger) << "Accept loop started on fd=" << listen_sock->getSocket();
    while (true) {
        auto client = listen_sock->accept();
        if (client) {
            auto remote = client->getRemoteAddress();
            ZERO_LOG_INFO(g_logger) << "New client: " << (remote ? remote->toString() : "?");
            sched->schedule([client]() { handleClient(client); });
        }
    }
}

int main(int argc, char** argv) {
    printf("=== Echo Server (SO_REUSEPORT) ===\n");
    setbuf(stdout, NULL);
    zero::LoggerMgr::GetInstance()->getRoot()->setLevel(zero::LogLevel::ERROR);

    int port = 8080;
    int threads = 4;
    if (argc >= 2) port = atoi(argv[1]);
    if (argc >= 3) threads = atoi(argv[2]);

    // 创建调度器
    zero::Scheduler scheduler(threads, false, "echo");
    scheduler.start();
    printf("Scheduler started (%d threads)\n", threads);

    auto addr = zero::IPv4Address::Create("0.0.0.0", port);

    // 单 listen socket + work-stealing (隔离 SO_REUSEPORT 问题)
    auto sock = zero::Socket::CreateTCPSocket();
    int val = 1;
    sock->setOption(SOL_SOCKET, SO_REUSEADDR, val);
    if (!sock->bind(addr) || !sock->listen(65535)) {
        printf("bind/listen failed\n");
        scheduler.stop();
        return 1;
    }
    scheduler.schedule([sock, &scheduler]() mutable {
        acceptLoop(sock, &scheduler);
    });
    printf("Listening on :%d (1 accept + %d workers)\n", port, threads);

    sleep(10);
    scheduler.stop();
    return 0;
}
