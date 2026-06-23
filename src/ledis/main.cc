#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <sys/stat.h>

#include "zero/zero.h"
#include "zero/log/log.h"
#include "zero/log/config.h"
#include "zero/config/config.h"
#include "ledis/ledis.h"
#include "ledis/cluster/cluster_config.h"
#include "ledis/cluster/cluster_manager.h"

// ---- 版本选择 ----
// v2: 单线程 fiber (server/server.h) — 简单可靠，30万 rps
// v5: 多核 epoll (server/server_v5.h) — SpinLock 共享 Dict
// vu: io_uring 单线程 (server/uring_server.h) — 批量 I/O

static ledis::LedisServer::ptr    g_server_v2;
static ledis::LedisServerV5::ptr  g_server_v5;
static ledis::UringServer*        g_server_vu = nullptr;
static int g_version = 0;

void signalHandler(int sig) {
    // 信号处理器中不用 logger (非信号安全), 用 write
    const char* msg = "\nReceived signal, shutting down...\n";
    ::write(2, msg, strlen(msg));
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
        << "  --cluster-enabled      Enable cluster mode\n"
        << "  --cluster-port <port>  Cluster bus port (default: client_port + 10000)\n"
        << "  --cluster-seeds <list> Seed nodes: ip:port,ip:port,...\n"
        << "  --cluster-replicas <n> Number of replicas per slot (default: 0)\n"
        << "  --loglevel <level>    Log level: TRACE|DEBUG|INFO|WARN|ERROR (default: INFO)\n"
        << "  --config <file|dir>   Load config from YAML file or conf directory\n"
        << "  --help                 Show this help\n"
        << "\nExamples:\n"
        << "  " << prog << " --port 6379                         # v2 fiber, single thread\n"
        << "  " << prog << " --port 6379 --io-threads 4            # v5 sharding, 4 workers\n"
        << "  " << prog << " --port 6379 --io-threads 4 --engine uring  # v5 + io_uring\n"
        << "  " << prog << " --port 6379 --cluster-enabled --cluster-port 16379  # cluster mode\n"
        << "  " << prog << " --port 6380 --cluster-enabled --cluster-seeds 127.0.0.1:16379  # join cluster\n"
        << "  " << prog << " --config conf/                        # load all YAML from conf dir\n"
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
    std::string aof_mode_str = "everysec";  // for ConfigVar

    // 集群选项
    bool cluster_enabled = false;
    int  cluster_port    = 0;
    std::string cluster_seeds;
    int  cluster_replicas = 0;

    // 日志 & 配置
    std::string loglevel = "INFO";
    std::string config_file;

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
            aof_mode_str = argv[++i];
            if (aof_mode_str == "always") aof_mode = ledis::AofWriter::ALWAYS;
            else if (aof_mode_str == "no") aof_mode = ledis::AofWriter::NO;
        } else if (arg == "--cluster-enabled") {
            cluster_enabled = true;
        } else if (arg == "--cluster-port" && i + 1 < argc) {
            cluster_port = std::stoi(argv[++i]);
        } else if (arg == "--cluster-seeds" && i + 1 < argc) {
            cluster_seeds = argv[++i];
        } else if (arg == "--cluster-replicas" && i + 1 < argc) {
            cluster_replicas = std::stoi(argv[++i]);
        } else if (arg == "--loglevel" && i + 1 < argc) {
            loglevel = argv[++i];
        } else if (arg == "--config" && i + 1 < argc) {
            config_file = argv[++i];
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

    // ---- 加载配置 ----
    // 优先级: CLI 参数 > YAML 文件 > ConfigVar 默认值

    // 1. 注册所有 ConfigVar (默认值, 约定优于配置)
    // 日志: 约定优于配置 — YAML 中 log.* 自动匹配注册的 ConfigVar
    auto cv_loglevel  = zero::Config::Lookup<std::string>("log.root_level", "INFO", "TRACE|DEBUG|INFO|WARN|ERROR");
    auto cv_logfile   = zero::Config::Lookup<std::string>("log.file", "", "Log file path (empty=console only)");
    auto cv_logmaxsize = zero::Config::Lookup<uint64_t>("log.max_size", 104857600, "Log file max size before rolling");
    auto cv_logmaxfiles = zero::Config::Lookup<uint32_t>("log.max_files", 10, "Max rotated log files to keep");
    auto cv_port      = zero::Config::Lookup<int>("ledis.server.port", 6379, "Listen port");
    auto cv_bind      = zero::Config::Lookup<std::string>("ledis.server.bind", "0.0.0.0", "Bind address");
    auto cv_threads   = zero::Config::Lookup<int>("ledis.server.io_threads", 1, "I/O worker threads");
    auto cv_engine    = zero::Config::Lookup<std::string>("ledis.server.engine", "", "I/O engine: epoll|uring (empty=auto)");
    auto cv_password  = zero::Config::Lookup<std::string>("ledis.server.requirepass", "", "Auth password for 'default' user (empty=no auth)");
    auto cv_acl_users = zero::Config::Lookup<std::map<std::string, std::string>>(
        "ledis.acl.users", {}, "ACL users: {username: password, ...}");
    auto cv_aof_path  = zero::Config::Lookup<std::string>("ledis.aof.path", "", "AOF file path (empty=disabled)");
    auto cv_aof_mode  = zero::Config::Lookup<std::string>("ledis.aof.mode", "everysec", "AOF fsync: always|everysec|no");
    auto cv_cluster   = zero::Config::Lookup<bool>("ledis.cluster.enabled", false, "Enable cluster mode");
    auto cv_cl_port   = zero::Config::Lookup<int>("ledis.cluster.port", 0, "Cluster bus port (0=port+10000)");
    auto cv_cl_seeds  = zero::Config::Lookup<std::string>("ledis.cluster.seeds", "", "Seed nodes: ip:port,...");
    auto cv_cl_replicas = zero::Config::Lookup<int>("ledis.cluster.replicas", 0, "Replica count per slot");
    auto cv_cl_gossip_interval = zero::Config::Lookup<int>("ledis.cluster.gossip_interval_ms", 1000, "Gossip heartbeat interval (ms)");
    auto cv_cl_node_timeout    = zero::Config::Lookup<int>("ledis.cluster.node_timeout_ms", 15000, "Node failure timeout (ms)");
    auto cv_cl_port_offset     = zero::Config::Lookup<int>("ledis.cluster.port_offset", 10000, "Cluster port = server.port + offset");
    auto cv_slowlog_max   = zero::Config::Lookup<int>("ledis.slowlog.max_len", 128, "Slowlog max entries");
    auto cv_slowlog_thres = zero::Config::Lookup<int64_t>("ledis.slowlog.threshold_us", 10000, "Slowlog threshold (microseconds)");
    auto cv_evict_mem     = zero::Config::Lookup<size_t>("ledis.eviction.maxmemory", 0, "Max memory in bytes (0=unlimited)");
    auto cv_evict_policy  = zero::Config::Lookup<std::string>("ledis.eviction.policy", "noeviction", "noeviction|allkeys-lru|volatile-lru|allkeys-random|volatile-random|volatile-ttl");
    auto cv_evict_samples = zero::Config::Lookup<int>("ledis.eviction.samples", 20, "Eviction sample size per cycle");
    auto cv_expire_cycles = zero::Config::Lookup<int>("ledis.server.active_expire_cycles", 16, "Active expire scan loops per batch");
    auto cv_recv_buf_size = zero::Config::Lookup<int>("ledis.server.recv_buf_size", 65536, "Client recv buffer size (bytes)");

    // 2. 加载 YAML 配置文件 (约定优于配置: 只设置已注册 ConfigVar 的 key)
    if (!config_file.empty()) {
        struct stat st;
        if (stat(config_file.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            // 目录模式: 加载所有 .yaml/.yml (包括 ledis.yaml + zero.yaml)
            zero::Config::LoadFromConfDir(config_file, false);
        } else {
            // 单文件模式
            if (zero::Config::LoadFromYamlFile(config_file)) {
                ZERO_LOG_INFO(ZERO_LOG_ROOT()) << "Loaded config: " << config_file;
            } else {
                ZERO_LOG_WARN(ZERO_LOG_ROOT()) << "Failed to load config: " << config_file;
            }
        }
    }

    // 3. CLI 参数覆盖 ConfigVar (最高优先级)
    if (!loglevel.empty())      cv_loglevel->setValue(loglevel);
    cv_port->setValue(port);
    if (!bind_addr.empty())     cv_bind->setValue(bind_addr);
    cv_threads->setValue(io_threads);
    if (!engine.empty())        cv_engine->setValue(engine);
    if (!requirepass.empty())   cv_password->setValue(requirepass);
    if (!aof_path.empty())      cv_aof_path->setValue(aof_path);
    if (!aof_mode_str.empty())  cv_aof_mode->setValue(aof_mode_str);
    cv_cluster->setValue(cluster_enabled);
    cv_cl_port->setValue(cluster_port);
    if (!cluster_seeds.empty()) cv_cl_seeds->setValue(cluster_seeds);
    cv_cl_replicas->setValue(cluster_replicas);

    // 4. 从 ConfigVar 回读到局部变量
    loglevel         = cv_loglevel->getValue();
    port             = cv_port->getValue();
    bind_addr        = cv_bind->getValue();
    io_threads       = cv_threads->getValue();
    engine           = cv_engine->getValue();
    requirepass      = cv_password->getValue();
    aof_path         = cv_aof_path->getValue();
    std::string aof_mode_val = cv_aof_mode->getValue();
    cluster_enabled  = cv_cluster->getValue();
    cluster_port     = cv_cl_port->getValue();
    cluster_seeds    = cv_cl_seeds->getValue();
    cluster_replicas = cv_cl_replicas->getValue();

    // 解析 aof_mode 字符串
    if (aof_mode_val == "always") aof_mode = ledis::AofWriter::ALWAYS;
    else if (aof_mode_val == "no") aof_mode = ledis::AofWriter::NO;
    // else keep default (EVERYSEC)

    // 集群专用配置
    int cl_gossip_interval = cv_cl_gossip_interval->getValue();
    int cl_node_timeout    = cv_cl_node_timeout->getValue();
    int cl_port_offset     = cv_cl_port_offset->getValue();
    if (cluster_port == 0) cluster_port = port + cl_port_offset;

    // 慢日志 & 淘汰 & 过期
    int slowlog_max        = cv_slowlog_max->getValue();
    int64_t slowlog_thres  = cv_slowlog_thres->getValue();
    size_t evict_mem       = cv_evict_mem->getValue();
    std::string evict_pol  = cv_evict_policy->getValue();
    int evict_samples      = cv_evict_samples->getValue();
    int expire_cycles      = cv_expire_cycles->getValue();
    int recv_buf_size      = cv_recv_buf_size->getValue();
    auto acl_users_map     = cv_acl_users->getValue();

    // 5. 应用日志配置 (异步, 不阻塞 QPS)
    {
        auto lvl = zero::LogLevel::FromString(cv_loglevel->getValue());
        if (lvl == zero::LogLevel::UNKNOWN) lvl = zero::LogLevel::INFO;

        if (!cv_logfile->getValue().empty()) {
            zero::LogConfig::SetupAsyncLog(
                false, cv_logfile->getValue(), lvl,
                cv_logmaxsize->getValue(),
                static_cast<uint32_t>(cv_logmaxfiles->getValue()));
            ZERO_LOG_INFO(ZERO_LOG_ROOT()) << "Async log file: " << cv_logfile->getValue();
        } else if (lvl == zero::LogLevel::OFF) {
            zero::LogConfig::SetupBenchSilent();
        } else {
            zero::LogConfig::SetupAsyncLog(true, "", lvl);
        }
    }

    ZERO_LOG_INFO(ZERO_LOG_ROOT()) << "Config: log=" << loglevel
        << " port=" << port << " bind=" << bind_addr
        << " engine=" << (engine.empty() ? "auto" : engine)
        << " io_threads=" << io_threads
        << " auth=" << (requirepass.empty() ? "no" : "yes")
        << " aof=" << (aof_path.empty() ? "no" : aof_path + "(" + aof_mode_val + ")")
        << " cluster=" << (cluster_enabled ? "yes(cluster_port=" + std::to_string(cluster_port) + ")" : "no");

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    ZERO_LOG_INFO(ZERO_LOG_ROOT()) << "Ledis — Redis-Compatible KV Store (lstl + zero)"
        << " | Engine: " << (use_uring ? "io_uring" : (use_v5 ? "v5 epoll" : "v2 fiber"))
        << " | threads: " << io_threads
        << " | fiber_pool=" << zero::config::FiberPoolSize()
        << " tcp_nodelay=" << (zero::config::SocketTcpNoDelay() ? "on" : "off");

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
        cfg.cluster_enabled  = cluster_enabled;
        cfg.cluster_port     = cluster_port;
        cfg.cluster_seeds    = cluster_seeds;
        cfg.cluster_replicas = cluster_replicas;
        g_server_v5 = std::make_shared<ledis::LedisServerV5>(std::move(cfg));
        ok = g_server_v5->start();
    } else {
        // v2: 单线程 fiber (默认)
        ledis::LedisServer::Config cfg;
        cfg.bind_addr            = bind_addr;
        cfg.port                 = port;
        cfg.io_threads           = 1;
        cfg.requirepass          = requirepass;
        cfg.aof_path             = aof_path;
        cfg.aof_mode             = aof_mode;
        cfg.cluster_enabled      = cluster_enabled;
        cfg.cluster_port         = cluster_port;
        cfg.cluster_seeds        = cluster_seeds;
        cfg.cluster_replicas     = cluster_replicas;
        cfg.cluster_gossip_ms    = cl_gossip_interval;
        cfg.cluster_timeout_ms   = cl_node_timeout;
        cfg.slowlog_max_len      = slowlog_max;
        cfg.slowlog_threshold_us = slowlog_thres;
        cfg.maxmemory            = evict_mem;
        cfg.eviction_policy      = evict_pol;
        cfg.eviction_samples     = evict_samples;
        cfg.active_expire_cycles = expire_cycles;
        cfg.recv_buf_size        = recv_buf_size;
        cfg.acl_users            = std::move(acl_users_map);
        g_server_v2 = std::make_shared<ledis::LedisServer>(std::move(cfg));
        ok = g_server_v2->start();
    }

    if (!ok) {
        ZERO_LOG_ERROR(ZERO_LOG_ROOT()) << "Failed to start server.";
        return 1;
    }

    ZERO_LOG_INFO(ZERO_LOG_ROOT()) << "Server started. Press Ctrl+C to stop.";

    while ((g_server_vu) || (g_server_v5) || (g_server_v2)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        // 集群维护: 处理入站消息和 gossip
        if (g_server_v2) g_server_v2->clusterTick();
        if (g_server_v5) g_server_v5->clusterTick();
    }

    return 0;
}
