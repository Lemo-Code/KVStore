// Ledis Server — 启动入口
//
// 用法: ./ledis-server [--port 6379] [--bind 0.0.0.0]
//

#include "ledis/server/ledis_server.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

static std::atomic<bool> g_running{true};

static void signalHandler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        g_running = false;
    }
}

static void printBanner() {
    printf(R"(
  _          _ _
 | |    ___ |_|_|___
 | |__ | -_|| | |_ -|
 |____||___||_|_|___|

 Ledis v0.1.0 — Redis-compatible KV Cache
 Built on Zero Network Library
)");
    fflush(stdout);
}

static void printUsage(const char* prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --port <port>     Listen port (default: 6379)\n");
    printf("  --bind <addr>     Bind address (default: 0.0.0.0)\n");
    printf("  --threads <n>     IO threads (default: 3)\n");
    printf("  --aof <path>      AOF file path (enables AOF persistence)\n");
    printf("  --requirepass <p>  Password for AUTH\n");
    printf("  --help            Show this help\n");
}

int main(int argc, char* argv[]) {
    ledis::LedisServer::Config cfg;

    // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            cfg.port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--bind") == 0 && i + 1 < argc) {
            cfg.bind_addr = argv[++i];
        } else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            cfg.io_threads = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--aof") == 0 && i + 1 < argc) {
            cfg.aof_path = argv[++i];
        } else if (strcmp(argv[i], "--requirepass") == 0 && i + 1 < argc) {
            cfg.requirepass = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            return 0;
        }
    }

    printBanner();

    // 注册信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGPIPE, SIG_IGN);

    // 创建并启动服务器
    auto server = std::make_shared<ledis::LedisServer>(std::move(cfg));

    if (!server->start()) {
        fprintf(stderr, "Failed to start Ledis server\n");
        return 1;
    }

    printf("Ready to accept connections on %s:%d\n",
           server->config().bind_addr.c_str(), server->config().port);
    if (!cfg.aof_path.empty()) {
        printf("AOF: %s\n", cfg.aof_path.c_str());
    }
    printf("Connect with: redis-cli -p %d\n", server->config().port);
    fflush(stdout);

    // 主线程等待停止信号
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    printf("\nShutting down...\n");
    server->stop();
    printf("Ledis server stopped. Goodbye.\n");

    return 0;
}
