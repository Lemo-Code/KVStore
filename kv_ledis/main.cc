#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#include "zero/zero.h"
#include "kv_ledis/ledis.h"

// ---- 版本选择 ----
// v2: 单线程 fiber (server/server.h) — 简单可靠，30万 rps
// v5: 多核 epoll (server/server_v5.h) — SpinLock 共享 Dict
// vu: io_uring 单线程 (server/uring_server.h) — 批量 I/O

static ledis::LedisServer::ptr    g_server_v2;
static ledis::LedisServerV5::ptr  g_server_v5;
static ledis::UringServer*        g_server_vu = nullptr;
static int g_version = 0;

void signalHandler(int sig) {
    std::cout << "\nReceived signal " << sig << ", shutting down..." << std::endl;
    if (g_server_vu) { g_server_vu->stop(); delete g_server_vu; g_server_vu = nullptr; }
    if (g_server_v5) g_server_v5->stop();
    if (g_server_v2) g_server_v2->stop();
    g_server_v5.reset();
    g_server_v2.reset();
}

void printUsage(const char* prog) {
    std::cout
        << "Ledis — Redis-Compatible KV Store (lstl + zero)\n"
        << "Usage: " << prog << " [options]\n"
        << "Options:\n"
        << "  --port <port>          Listen port (default: 6379)\n"
        << "  --bind <addr>          Bind address (default: 0.0.0.0)\n"
        << "  --io-threads <n>       Worker threads (v2:1 fiber, v5:N shards)\n"
        << "  --engine <epoll|uring> I/O engine (default: fiber for v2, epoll for v5)\n"
        << "  --requirepass <pwd>    Require AUTH password\n"
        << "  --aof <path>           Enable AOF persistence\n"
        << "  --aof-mode <mode>      always|everysec|no (default: everysec)\n"
        << "  --help                 Show this help\n"
        << "\nExamples:\n"
        << "  " << prog << " --port 6379                         # v2 fiber, single thread\n"
        << "  " << prog << " --port 6379 --io-threads 4            # v5 sharding, 4 workers\n"
        << "  " << prog << " --port 6379 --io-threads 4 --engine uring  # v5 + io_uring\n"
        << std::endl;
}

int main(int argc, char** argv) {
    // 解析参数
    std::string engine;
    int port = 6379;
    std::string bind_addr = "0.0.0.0";
    int io_threads = 1;
    std::string requirepass;
    std::string aof_path;
    auto aof_mode = ledis::AofWriter::EVERYSEC;

    lstl::vector<char*> zero_argv = {argv[0]};

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]); return 0;
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--bind" && i + 1 < argc) {
            bind_addr = argv[++i];
        } else if (arg == "--io-threads" && i + 1 < argc) {
            io_threads = std::stoi(argv[++i]);
        } else if (arg == "--engine" && i + 1 < argc) {
            engine = argv[++i];
        } else if (arg == "--requirepass" && i + 1 < argc) {
            requirepass = argv[++i];
        } else if (arg == "--aof" && i + 1 < argc) {
            aof_path = argv[++i];
        } else if (arg == "--aof-mode" && i + 1 < argc) {
            std::string mode(argv[++i]);
            if (mode == "always") aof_mode = ledis::AofWriter::ALWAYS;
            else if (mode == "no") aof_mode = ledis::AofWriter::NO;
        } else {
            zero_argv.push_back(argv[i]);
        }
    }

    // 自动选择版本
    bool use_uring = (engine == "uring");
    bool use_v5    = (engine == "epoll" && io_threads > 1);
    if (engine.empty()) use_v5 = (io_threads > 1);  // 多线程自动用 v5
    g_version = use_uring ? 0 : (use_v5 ? 5 : 2);

    // 初始化 zero (v2 需要)
    if (!use_uring && !use_v5)
        zero::InitZero(static_cast<int>(zero_argv.size()), zero_argv.data());

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "Ledis — Redis-Compatible KV Store (lstl + zero)" << std::endl;
    std::cout << "Engine: "
              << (use_uring ? "io_uring" : (use_v5 ? "v5 epoll" : "v2 fiber"))
              << ", threads: " << io_threads << std::endl;

    bool ok = false;

    if (use_uring) {
        // io_uring 单线程 (I/O 批量)
        g_server_vu = new ledis::UringServer(
            port, bind_addr, requirepass, aof_path, aof_mode);
        ok = g_server_vu->start();
    } else if (use_v5) {
        // v5: 多线程 epoll + SpinLock
        ledis::LedisServerV5::Config cfg;
        cfg.bind_addr   = bind_addr;
        cfg.port        = port;
        cfg.io_threads  = io_threads;
        cfg.requirepass = requirepass;
        cfg.aof_path    = aof_path;
        cfg.aof_mode    = aof_mode;
        g_server_v5 = std::make_shared<ledis::LedisServerV5>(std::move(cfg));
        ok = g_server_v5->start();
    } else {
        // v2: 单线程 fiber (默认)
        ledis::LedisServer::Config cfg;
        cfg.bind_addr   = bind_addr;
        cfg.port        = port;
        cfg.io_threads  = 1;
        cfg.requirepass = requirepass;
        cfg.aof_path    = aof_path;
        cfg.aof_mode    = aof_mode;
        g_server_v2 = std::make_shared<ledis::LedisServer>(std::move(cfg));
        ok = g_server_v2->start();
    }

    if (!ok) {
        std::cerr << "Failed to start server." << std::endl;
        return 1;
    }

    std::cout << "Server started. Press Ctrl+C to stop." << std::endl;

    while ((g_server_vu) || (g_server_v5) || (g_server_v2))
        std::this_thread::sleep_for(std::chrono::seconds(1));

    return 0;
}
