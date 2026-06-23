// 压力测试: 多连接并发 echo
#include "zero/zero.h"
#include "zero/scheduler/reactor.h"
#include "zero/log/log.h"
#include "zero/net/socket.h"
#include "zero/net/socket_stream.h"

#include <cstdio>
#include <cstring>
#include <atomic>

static std::atomic<int> g_connects{0};
static std::atomic<int> g_disconnects{0};
static std::atomic<int> g_bytes{0};

void handleClient(zero::Socket::ptr sock) {
    g_connects++;
    zero::SocketStream stream(sock);
    char buf[4096];

    while (true) {
        ssize_t n = stream.read(buf, sizeof(buf));
        if (n <= 0) break;
        g_bytes += n;
        stream.writeFixed(buf, n);
    }
    stream.close();
    g_disconnects++;
}

void acceptLoop(zero::Socket::ptr sock, zero::Scheduler* sched) {
    while (true) {
        auto client = sock->accept();
        if (client) {
            sched->schedule([client]() { handleClient(client); });
        }
    }
}

int main() {
    printf("=== Stress Test ===\n");
    setbuf(stdout, NULL);

    auto addr = zero::IPv4Address::Create("0.0.0.0", 18080);
    auto listen_sock = zero::Socket::CreateTCPSocket();
    int val = 1;
    listen_sock->setOption(SOL_SOCKET, SO_REUSEADDR, val);
    listen_sock->bind(addr);
    listen_sock->listen(128);
    printf("Ready on :18080\n");

    zero::Scheduler sched(4, false, "stress");
    sched.start();
    sched.schedule([&listen_sock, &sched]() { acceptLoop(listen_sock, &sched); });

    // Wait for test
    printf("Run in another terminal:\n");
    printf("  for i in $(seq 100); do echo msg$i | nc -w1 localhost 18080 & done; wait\n");
    printf("Press Ctrl+C to stop\n");

    for (int i = 0; i < 30; i++) {
        sleep(1);
        printf("conn=%d disc=%d bytes=%d\n",
               g_connects.load(), g_disconnects.load(), g_bytes.load());
    }

    sched.stop();
    return 0;
}
