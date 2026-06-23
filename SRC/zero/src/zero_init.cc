// zero_init.cc — zero library bootstrap and initialization
//
// InitZero() must be called once at process start before any other zero API.
// It creates the global Scheduler, initializes logging, loads configuration,
// pre-allocates resource pools, and returns the Scheduler ready for start().
//
// Provides:
//   Scheduler& InitZero(int argc, char** argv)
//   Config&    GetConfig()
//   Scheduler& GetScheduler()
//   void       ShutdownZero()

#include <utility>   // must come before zero.h (noncopyable.h uses std::move)

#include "zero/zero.h"

#include "zero/log/log.h"
#include "zero/config/config.h"
#include "zero/fiber/fiber.h"
#include "zero/fiber/fiber_pool.h"
#include "zero/fiber/stack_pool.h"
#include "zero/scheduler/scheduler.h"

#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace zero {

namespace {

// ============================================================
// Hardware detection
// ============================================================

int detect_cpu_count() {
    int n = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    return (n > 0) ? n : 4;
}

// ============================================================
// Parse log level from string
// ============================================================

LogLevel parse_log_level(const std::string& s) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower == "trace") return LogLevel::TRACE;
    if (lower == "debug") return LogLevel::DEBUG;
    if (lower == "info")  return LogLevel::INFO;
    if (lower == "warn" || lower == "warning") return LogLevel::WARN;
    if (lower == "error") return LogLevel::ERROR;
    if (lower == "fatal") return LogLevel::FATAL;
    return LogLevel::INFO;
}

// ============================================================
// Command-line argument parsing
// ============================================================

struct StartupOptions {
    std::string config_file;
    std::string config_dir;
    std::string log_level;
    int         threads = 0;
    bool        show_help = false;
};

StartupOptions parse_startup_args(int argc, char** argv) {
    StartupOptions opts;
    if (!argv) return opts;

    for (int i = 1; i < argc; ++i) {
        if (!argv[i]) continue;
        std::string arg(argv[i]);

        if (arg == "--help" || arg == "-h") {
            opts.show_help = true;
        } else if (arg == "--config" || arg == "-c") {
            if (i + 1 < argc && argv[i + 1]) opts.config_file = argv[++i];
        } else if (arg.rfind("--config=", 0) == 0) {
            opts.config_file = arg.substr(9);
        } else if (arg.rfind("-c=", 0) == 0) {
            opts.config_file = arg.substr(3);
        } else if (arg == "--config-dir" || arg == "-C") {
            if (i + 1 < argc && argv[i + 1]) opts.config_dir = argv[++i];
        } else if (arg.rfind("--config-dir=", 0) == 0) {
            opts.config_dir = arg.substr(13);
        } else if (arg == "--log-level") {
            if (i + 1 < argc && argv[i + 1]) opts.log_level = argv[++i];
        } else if (arg.rfind("--log-level=", 0) == 0) {
            opts.log_level = arg.substr(12);
        } else if (arg == "--threads" || arg == "-t") {
            if (i + 1 < argc && argv[i + 1])
                opts.threads = std::atoi(argv[++i]);
        } else if (arg.rfind("--threads=", 0) == 0) {
            opts.threads = std::atoi(arg.substr(10).c_str());
        }
    }
    return opts;
}

void print_startup_help(const char* prog) {
    std::cout
        << "zero — High-performance C++ network library\n"
        << "\n"
        << "Usage: " << (prog ? prog : "app") << " [options]\n"
        << "\n"
        << "Options:\n"
        << "  -c, --config <file>      Load config from YAML file\n"
        << "  -C, --config-dir <dir>   Load config from YAML directory\n"
        << "  --log-level <level>      Override log level\n"
        << "                           (trace/debug/info/warn/error/fatal)\n"
        << "  -t, --threads <N>        Number of worker threads (0=auto)\n"
        << "  -h, --help               Show this help message\n"
        << "\n"
        << "Environment:\n"
        << "  ZERO_CONFIG              Config file path\n"
        << "  ZERO_*                   All ZERO_* env vars are loaded as config\n"
        << std::endl;
}

// ============================================================
// Register default configuration values
// ============================================================

void register_defaults() {
    auto& cfg = Config::instance();

    cfg.set_int("zero.threads",              detect_cpu_count());
    cfg.set_bool("zero.enable_cpu_affinity", true);
    cfg.set_string("zero.cores",             "");

    cfg.set_int("zero.stack_size",    static_cast<int>(Fiber::kDefaultStackSize));
    cfg.set_int("zero.fiber_pool_size",      512);
    cfg.set_int("zero.stack_pool_size",      256);
    cfg.set_int("zero.max_fibers",           100000);

    cfg.set_string("zero.log_level",  "info");
    cfg.set_string("zero.log_file",   "");
    cfg.set_bool("zero.log_color",    true);
    cfg.set_bool("zero.log_syslog",   false);

    cfg.set_int("zero.tcp_timeout_ms",       30000);
    cfg.set_int("zero.tcp_recv_buffer",      65536);
    cfg.set_int("zero.tcp_send_buffer",      65536);
    cfg.set_bool("zero.tcp_keepalive",       true);
    cfg.set_bool("zero.tcp_nodelay",         true);
    cfg.set_int("zero.listen_backlog",       4096);
    cfg.set_int("zero.max_connections",      100000);

    cfg.set_string("zero.bind_address",      "0.0.0.0");
    cfg.set_int("zero.bind_port",            0);

    cfg.set_int("zero.scheduler_tick_ms",    5);
    cfg.set_int("zero.timer_wheel_slots",    512);

    cfg.set_string("zero.io_engine",         "epoll");
    cfg.set_int("zero.io_max_events",        1024);
    cfg.set_int("zero.io_timeout_ms",        -1);

    cfg.set_bool("zero.enable_mem_pool",     true);
    cfg.set_bool("zero.enable_core_dump",    true);
    cfg.set_bool("zero.enable_backtrace",    false);
}

// ============================================================
// Apply log configuration from config values
// ============================================================

void apply_log_config() {
    auto* root = Logger::root();
    if (!root) return;

    auto& cfg = Config::instance();

    std::string level_str = cfg.get_string("zero.log_level", "info");
    root->set_level(parse_log_level(level_str));

    std::string log_file = cfg.get_string("zero.log_file", "");
    if (!log_file.empty()) {
        auto file_app = std::make_shared<FileAppender>(log_file);

        int max_size_mb = cfg.get_int("zero.log_rotation_size_mb", 100);
        if (max_size_mb > 0) {
            file_app->enable_rotation(
                static_cast<size_t>(max_size_mb) * 1024 * 1024);
        }
        int max_files = cfg.get_int("zero.log_rotation_files", 10);
        file_app->set_max_files(static_cast<size_t>(max_files));

        if (cfg.get_bool("zero.log_rotation_daily", false)) {
            file_app->enable_daily_rotation();
        }

        root->add_appender(std::move(file_app));
    }

    if (cfg.get_bool("zero.log_syslog", false)) {
        std::string ident = cfg.get_string("zero.log_syslog_ident", "zero");
        root->add_appender(std::make_shared<SyslogAppender>(ident));
    }
}

// ============================================================
// Pre-allocate resource pools
// ============================================================

void preallocate_pools() {
    auto& cfg = Config::instance();

    int stack_size  = cfg.get_int("zero.stack_size",
                                   static_cast<int>(Fiber::kDefaultStackSize));
    int fiber_count = cfg.get_int("zero.fiber_pool_size", 512);
    int stack_count = cfg.get_int("zero.stack_pool_size", 256);

    if (stack_count > 0) {
        StackPool::instance().preallocate(
            static_cast<size_t>(stack_count),
            static_cast<size_t>(stack_size));
    }
    if (fiber_count > 0) {
        FiberPool::instance().preallocate(
            static_cast<size_t>(fiber_count),
            static_cast<size_t>(stack_size));
    }
}

// ============================================================
// Log startup summary
// ============================================================

void log_summary() {
    auto* root = Logger::root();
    if (!root) return;

    auto& cfg = Config::instance();

    std::ostringstream oss;
    oss << "\n============================================\n"
        << "  zero library v2.0 initialized\n"
        << "============================================\n"
        << "  Threads:        " << cfg.get_int("zero.threads", 0) << "\n"
        << "  Fiber stack:    "
        << (cfg.get_int("zero.stack_size", 131072) / 1024) << " KB\n"
        << "  Fiber pool:     " << cfg.get_int("zero.fiber_pool_size", 512)
        << " pre-allocated\n"
        << "  Stack pool:     " << cfg.get_int("zero.stack_pool_size", 256)
        << " pre-allocated\n"
        << "  I/O engine:     "
        << cfg.get_string("zero.io_engine", "epoll") << "\n"
        << "  Log level:      "
        << cfg.get_string("zero.log_level", "info") << "\n"
        << "  Log file:       "
        << (cfg.get_string("zero.log_file", "").empty()
                ? "(console)" : cfg.get_string("zero.log_file", "")) << "\n"
        << "  Max connections: " << cfg.get_int("zero.max_connections", 100000) << "\n"
        << "  TCP nodelay:    "
        << (cfg.get_bool("zero.tcp_nodelay", true) ? "enabled" : "disabled") << "\n"
        << "  TCP timeout:    " << cfg.get_int("zero.tcp_timeout_ms", 30000) << " ms\n"
        << "============================================";

    root->info(__FILE__, __LINE__, oss.str());
}

// ============================================================
// Default config file search
// ============================================================

bool try_load_default_config() {
    const char* candidates[] = {
        "./config.yaml",
        "./conf/config.yaml",
        "./conf/zero.yaml",
        "/etc/zero/config.yaml",
        "/etc/zero/zero.yaml",
        nullptr
    };

    for (int i = 0; candidates[i] != nullptr; ++i) {
        struct stat st;
        if (::stat(candidates[i], &st) == 0 && S_ISREG(st.st_mode)) {
            return Config::instance().load_from_file(candidates[i]);
        }
    }
    return false;
}

// Global state
Scheduler* g_scheduler = nullptr;
bool      g_initialized = false;

} // anonymous namespace

// ============================================================
// Public API
// ============================================================

Scheduler& InitZero(int argc, char** argv) {
    // Guard against double initialization
    if (g_initialized) return *g_scheduler;

    // 1. Initialize root logger
    Logger::root();

    // 2. Register default configuration values
    register_defaults();

    // 3. Parse command-line arguments
    StartupOptions opts = parse_startup_args(argc, argv);

    if (opts.show_help) {
        print_startup_help(argv ? argv[0] : "app");
    }

    // 4. Load config from file/dir/env
    if (!opts.config_file.empty()) {
        Config::instance().load_from_file(opts.config_file);
    }
    if (!opts.config_dir.empty()) {
        Config::instance().load_from_dir(opts.config_dir);
    }
    if (opts.config_file.empty() && opts.config_dir.empty()) {
        try_load_default_config();
    }

    Config::instance().load_from_env("ZERO_");

    // Apply command-line overrides (highest priority)
    if (!opts.log_level.empty()) {
        Config::instance().set_string("zero.log_level", opts.log_level);
    }
    if (opts.threads > 0) {
        Config::instance().set_int("zero.threads", opts.threads);
    }

    // 5. Apply log configuration
    apply_log_config();

    // 6. Pre-allocate resource pools
    preallocate_pools();

    // 7. Create the global scheduler
    int num_threads = Config::instance().get_int("zero.threads",
                                                  detect_cpu_count());
    g_scheduler = new Scheduler(static_cast<size_t>(num_threads));

    // 8. Log startup summary
    log_summary();

    g_initialized = true;
    return *g_scheduler;
}

Config& GetConfig() {
    return Config::instance();
}

Scheduler& GetScheduler() {
    // If not yet initialized, create a default scheduler with auto threads
    if (!g_scheduler) {
        g_scheduler = new Scheduler(static_cast<size_t>(detect_cpu_count()));
        g_initialized = true;
    }
    return *g_scheduler;
}

void ShutdownZero() {
    auto* root = Logger::root();
    if (root) {
        root->info(__FILE__, __LINE__, "zero shutting down");
    }

    // Stop and destroy scheduler
    if (g_scheduler) {
        g_scheduler->stop();
        delete g_scheduler;
        g_scheduler = nullptr;
    }

    // Clear fiber/stack pools
    FiberPool::instance().clear();
    StackPool::instance().trim();

    g_initialized = false;
}

} // namespace zero
