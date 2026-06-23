#include "zero/zero.h"
#include "zero/config/config.h"
#include "zero/log/log.h"
#include "zero/log/config.h"

namespace zero {

// ============================================================
// ConfigVar 注册 — zero 库的所有可配置项
// ============================================================
// 约定优于配置:
//   - 每个子系统使用自己的命名空间 (log.*, fiber.*, scheduler.*, ...)
//   - ledis 应用使用 ledis.* 命名空间
//   - YAML 文件中的顶层 key 自动匹配
// ============================================================

// ---- Log ----
static ConfigVar<std::string>::ptr g_log_root_level;
static ConfigVar<bool>::ptr        g_log_console_enabled;
static ConfigVar<std::string>::ptr g_log_console_pattern;
static ConfigVar<bool>::ptr        g_log_console_color;
static ConfigVar<std::string>::ptr g_log_file_path;
static ConfigVar<uint64_t>::ptr    g_log_file_max_size;
static ConfigVar<uint32_t>::ptr    g_log_file_max_files;
static ConfigVar<std::string>::ptr g_log_file_pattern;
static ConfigVar<std::string>::ptr g_log_async_policy;

// ---- Fiber ----
static ConfigVar<uint32_t>::ptr g_fiber_stack_size;
static ConfigVar<uint32_t>::ptr g_fiber_pool_size;

// ---- Scheduler ----
static ConfigVar<int>::ptr  g_scheduler_threads;
static ConfigVar<bool>::ptr g_scheduler_use_caller;
static ConfigVar<std::string>::ptr g_scheduler_name;

// ---- Socket ----
static ConfigVar<int64_t>::ptr g_socket_connect_timeout_ms;
static ConfigVar<int64_t>::ptr g_socket_recv_timeout_ms;
static ConfigVar<int64_t>::ptr g_socket_send_timeout_ms;
static ConfigVar<bool>::ptr    g_socket_tcp_no_delay;
static ConfigVar<bool>::ptr    g_socket_so_reuseaddr;
static ConfigVar<int>::ptr     g_socket_listen_backlog;

// ---- Reactor ----
static ConfigVar<int>::ptr  g_reactor_poll_timeout_ms;
static ConfigVar<size_t>::ptr g_reactor_max_events;

// ---- TcpServer ----
static ConfigVar<int>::ptr g_tcpserver_keepalive;
static ConfigVar<int>::ptr g_tcpserver_timeout_ms;

// ============================================================
// InitConfig — 注册所有 zero 库的 ConfigVar (默认值)
// ============================================================
void InitConfig() {
    // Log
    g_log_root_level = Config::Lookup<std::string>(
        "log.root_level", "INFO", "Root logger level: TRACE|DEBUG|INFO|WARN|ERROR|FATAL");

    g_log_console_enabled = Config::Lookup<bool>(
        "log.appenders.console.enabled", true, "Enable console log appender");

    g_log_console_pattern = Config::Lookup<std::string>(
        "log.appenders.console.pattern",
        "%d{%Y-%m-%d %H:%M:%S} [%p] [%t] [%F:%l] %m%n",
        "Console log format pattern");

    g_log_console_color = Config::Lookup<bool>(
        "log.appenders.console.color", true, "Console log ANSI color");

    g_log_file_path = Config::Lookup<std::string>(
        "log.appenders.file.path", "", "File log path (empty=disabled)");

    g_log_file_max_size = Config::Lookup<uint64_t>(
        "log.appenders.file.max_size", 104857600, "Max file size before rotation (bytes, 0=none)");

    g_log_file_max_files = Config::Lookup<uint32_t>(
        "log.appenders.file.max_files", 10, "Max rotated files to keep");

    g_log_file_pattern = Config::Lookup<std::string>(
        "log.appenders.file.pattern",
        "%d{%Y-%m-%d %H:%M:%S} [%p] [%t] [%F:%l] %m%n",
        "File log format pattern");

    g_log_async_policy = Config::Lookup<std::string>(
        "log.async.policy", "off", "Async log policy: off|thread|coroutine");

    // Fiber
    g_fiber_stack_size = Config::Lookup<uint32_t>(
        "fiber.stack_size", 131072, "Default fiber stack size (bytes)");

    g_fiber_pool_size = Config::Lookup<uint32_t>(
        "fiber.pool_size", 1024, "Fiber pool prealloc count");

    // Scheduler
    g_scheduler_threads = Config::Lookup<int>(
        "scheduler.threads", 4, "Default scheduler thread count");

    g_scheduler_use_caller = Config::Lookup<bool>(
        "scheduler.use_caller", false, "Use caller thread as worker");

    g_scheduler_name = Config::Lookup<std::string>(
        "scheduler.name", "zero", "Scheduler name");

    // Socket
    g_socket_connect_timeout_ms = Config::Lookup<int64_t>(
        "socket.connect_timeout_ms", 5000, "TCP connect timeout (ms)");

    g_socket_recv_timeout_ms = Config::Lookup<int64_t>(
        "socket.recv_timeout_ms", 5000, "TCP recv timeout (ms, 0=none)");

    g_socket_send_timeout_ms = Config::Lookup<int64_t>(
        "socket.send_timeout_ms", 5000, "TCP send timeout (ms, 0=none)");

    g_socket_tcp_no_delay = Config::Lookup<bool>(
        "socket.tcp_no_delay", true, "Enable TCP_NODELAY (disable Nagle)");

    g_socket_so_reuseaddr = Config::Lookup<bool>(
        "socket.so_reuseaddr", true, "Enable SO_REUSEADDR");

    g_socket_listen_backlog = Config::Lookup<int>(
        "socket.listen_backlog", SOMAXCONN, "listen() backlog");

    // Reactor
    g_reactor_poll_timeout_ms = Config::Lookup<int>(
        "reactor.poll_timeout_ms", 10, "epoll_wait timeout (ms, -1=infinite)");

    g_reactor_max_events = Config::Lookup<size_t>(
        "reactor.max_events", 256, "Max events per epoll_wait call");

    // TcpServer
    g_tcpserver_keepalive = Config::Lookup<int>(
        "tcpserver.keepalive", 1, "SO_KEEPALIVE (0=off, 1=on)");

    g_tcpserver_timeout_ms = Config::Lookup<int>(
        "tcpserver.timeout_ms", 120000, "Connection idle timeout (ms, 0=none)");
}

// ============================================================
// InitZero — 完整初始化: 注册 ConfigVar + 加载配置 + 设置日志
// ============================================================
void InitZero(int argc, char** argv) {
    InitConfig();
    if (argc > 1) {
        std::string config_path = argv[1];
        Config::LoadFromYamlFile(config_path);
    }
    // 根据 ConfigVar 设置日志
    LogConfig::SetupFromConfig();
}

// ============================================================
// LoadConfig — 加载单个 YAML 文件
// ============================================================
void LoadConfig(const std::string& path) {
    Config::LoadFromYamlFile(path);
    LogConfig::SetupFromConfig();
}

// ============================================================
// config:: 便捷访问器
// ============================================================
namespace config {

int LogLevel() {
    return g_log_root_level
        ? static_cast<int>(LogLevel::FromString(g_log_root_level->getValue()))
        : static_cast<int>(LogLevel::INFO);
}

std::string LogFile() {
    return g_log_file_path ? g_log_file_path->getValue() : "";
}

std::string LogPattern() {
    return g_log_console_pattern ? g_log_console_pattern->getValue() : "";
}

bool LogAsync() {
    return g_log_async_policy ? g_log_async_policy->getValue() != "off" : false;
}

int LogAsyncThreads() { return 1; }

uint32_t FiberStackSize() {
    return g_fiber_stack_size ? g_fiber_stack_size->getValue() : 131072;
}

uint32_t FiberPoolSize() {
    return g_fiber_pool_size ? g_fiber_pool_size->getValue() : 1024;
}

int SchedulerThreads() {
    return g_scheduler_threads ? g_scheduler_threads->getValue() : 4;
}

bool SchedulerUseCaller() {
    return g_scheduler_use_caller ? g_scheduler_use_caller->getValue() : false;
}

std::string SchedulerName() {
    return g_scheduler_name ? g_scheduler_name->getValue() : "zero";
}

int64_t SocketConnectTimeoutMs() {
    return g_socket_connect_timeout_ms ? g_socket_connect_timeout_ms->getValue() : 5000;
}

int64_t SocketRecvTimeoutMs() {
    return g_socket_recv_timeout_ms ? g_socket_recv_timeout_ms->getValue() : 5000;
}

int64_t SocketSendTimeoutMs() {
    return g_socket_send_timeout_ms ? g_socket_send_timeout_ms->getValue() : 5000;
}

bool SocketTcpNoDelay() {
    return g_socket_tcp_no_delay ? g_socket_tcp_no_delay->getValue() : true;
}

bool SocketSoReuseaddr() {
    return g_socket_so_reuseaddr ? g_socket_so_reuseaddr->getValue() : true;
}

int SocketListenBacklog() {
    return g_socket_listen_backlog ? g_socket_listen_backlog->getValue() : SOMAXCONN;
}

int ReactorPollTimeoutMs() {
    return g_reactor_poll_timeout_ms ? g_reactor_poll_timeout_ms->getValue() : 10;
}

size_t ReactorMaxEvents() {
    return g_reactor_max_events ? g_reactor_max_events->getValue() : 256;
}

int TcpServerKeepalive() {
    return g_tcpserver_keepalive ? g_tcpserver_keepalive->getValue() : 1;
}

int TcpServerTimeoutMs() {
    return g_tcpserver_timeout_ms ? g_tcpserver_timeout_ms->getValue() : 120000;
}

} // namespace config
} // namespace zero
