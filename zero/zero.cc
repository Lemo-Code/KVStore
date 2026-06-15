#include "zero/zero.h"
#include "zero/config/config.h"
#include "zero/log/log.h"

namespace zero {

static zero::ConfigVar<int>::ptr g_log_level = nullptr;
static zero::ConfigVar<std::string>::ptr g_log_file = nullptr;
static zero::ConfigVar<std::string>::ptr g_log_pattern = nullptr;
static zero::ConfigVar<uint32_t>::ptr g_fiber_stack_size = nullptr;
static zero::ConfigVar<uint32_t>::ptr g_fiber_pool_size = nullptr;
static zero::ConfigVar<int>::ptr g_scheduler_threads = nullptr;
static zero::ConfigVar<bool>::ptr g_scheduler_use_caller = nullptr;
static zero::ConfigVar<int64_t>::ptr g_socket_connect_timeout_ms = nullptr;
static zero::ConfigVar<int64_t>::ptr g_socket_recv_timeout_ms = nullptr;
static zero::ConfigVar<int64_t>::ptr g_socket_send_timeout_ms = nullptr;
static zero::ConfigVar<bool>::ptr g_socket_tcp_no_delay = nullptr;
static zero::ConfigVar<int>::ptr g_reactor_poll_timeout_ms = nullptr;

void InitZero(int argc, char** argv) {
    InitConfig();
    if (argc > 1) {
        std::string config_path = argv[1];
        Config::LoadFromYamlFile(config_path);
    }
}

void InitConfig() {
    g_log_level = Config::Lookup<int>("log.level", 1);
    g_log_file = Config::Lookup<std::string>("log.file", "stdout");
    g_log_pattern = Config::Lookup<std::string>("log.pattern", "[%d{%Y-%m-%d %H:%M:%S}] [%p] [%t] %m%n");
    g_fiber_stack_size = Config::Lookup<uint32_t>("fiber.stack_size", 128 * 1024);
    g_fiber_pool_size = Config::Lookup<uint32_t>("fiber.pool_size", 1024);
    g_scheduler_threads = Config::Lookup<int>("scheduler.threads", 4);
    g_scheduler_use_caller = Config::Lookup<bool>("scheduler.use_caller", false);
    g_socket_connect_timeout_ms = Config::Lookup<int64_t>("socket.connect_timeout_ms", 5000);
    g_socket_recv_timeout_ms = Config::Lookup<int64_t>("socket.recv_timeout_ms", 5000);
    g_socket_send_timeout_ms = Config::Lookup<int64_t>("socket.send_timeout_ms", 5000);
    g_socket_tcp_no_delay = Config::Lookup<bool>("socket.tcp_no_delay", true);
    g_reactor_poll_timeout_ms = Config::Lookup<int>("reactor.poll_timeout_ms", 10);
}

void LoadConfig(const std::string& path) { Config::LoadFromYamlFile(path); }
namespace config {
int LogLevel() { return g_log_level ? g_log_level->getValue() : 1; }
std::string LogFile() { return g_log_file ? g_log_file->getValue() : ""; }
std::string LogPattern() { return g_log_pattern ? g_log_pattern->getValue() : ""; }
bool LogAsync() { return true; }
int LogAsyncThreads() { return 1; }
uint32_t FiberStackSize() { return g_fiber_stack_size ? g_fiber_stack_size->getValue() : 128*1024; }
uint32_t FiberPoolSize() { return g_fiber_pool_size ? g_fiber_pool_size->getValue() : 1024; }
int SchedulerThreads() { return g_scheduler_threads ? g_scheduler_threads->getValue() : 4; }
bool SchedulerUseCaller() { return g_scheduler_use_caller ? g_scheduler_use_caller->getValue() : false; }
int64_t SocketConnectTimeoutMs() { return g_socket_connect_timeout_ms ? g_socket_connect_timeout_ms->getValue() : 5000; }
int64_t SocketRecvTimeoutMs() { return g_socket_recv_timeout_ms ? g_socket_recv_timeout_ms->getValue() : 5000; }
int64_t SocketSendTimeoutMs() { return g_socket_send_timeout_ms ? g_socket_send_timeout_ms->getValue() : 5000; }
bool SocketTcpNoDelay() { return g_socket_tcp_no_delay ? g_socket_tcp_no_delay->getValue() : true; }
int ReactorPollTimeoutMs() { return g_reactor_poll_timeout_ms ? g_reactor_poll_timeout_ms->getValue() : 10; }
}
}
