/**
 * @file nettycore_config_demo.cc
 * @brief LemoNettyCore 配置 + 日志 + 网络栈完整示例。
 */
#include "lemo/nettycore_config.h"
#include "lemo/nettycore.h"
#include "lemo/socket/socket.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#ifndef NETTYCORE_DEFAULT_CONF
#define NETTYCORE_DEFAULT_CONF "tests/lemo/config/fixtures/nettycore_test.yaml"
#endif

namespace {

std::atomic<bool> g_stop{false};

void OnSignal(int) { g_stop.store(true, std::memory_order_release); }

void EchoHandler(const lemo::socket::Socket::ptr& sock) {
  char buf[4096];
  while (!g_stop.load(std::memory_order_acquire)) {
    const int n = sock->recv(buf, sizeof(buf));
    if (n <= 0) {
      break;
    }
    int sent = 0;
    while (sent < n) {
      const int w = sock->send(buf + sent, static_cast<size_t>(n - sent));
      if (w <= 0) {
        sock->close();
        return;
      }
      sent += w;
    }
  }
  sock->close();
}

}  // namespace

int main(int argc, char* argv[]) {
  const char* path = (argc > 1) ? argv[1] : NETTYCORE_DEFAULT_CONF;

  if (argc > 1 &&
      (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
    std::printf("用法: nettycore_config_demo [conf_path]\n");
    std::printf("默认配置: %s\n", NETTYCORE_DEFAULT_CONF);
    return 0;
  }

  lemo::config::ConfigCenter::Clear();
  if (!lemo::InitVerbose(path, stdout)) {
    return 1;
  }

  const lemo::config::NettySettings settings = lemo::config::GetNettySettings();
  lemo::config::PrintNettySettings(stdout);

  lemo::io::Runtime::ptr rt = lemo::config::CreateRuntimeFromConfig();
  lemo::server::TcpServer server(settings.server_name, rt.get());
  if (!server.bind(settings.server_host, settings.server_port)) {
    std::fprintf(stderr, "bind %s:%u failed\n", settings.server_host.c_str(),
                 static_cast<unsigned>(settings.server_port));
    return 1;
  }

  server.setConnectionHandler(EchoHandler);
  if (!server.start()) {
    std::fprintf(stderr, "TcpServer start failed\n");
    return 1;
  }

  LEMO_LOG_INFO(LEMO_LOG_NAME("server"))
      << "TcpServer listening " << settings.server_host << ":"
      << static_cast<unsigned>(settings.server_port) << " (echo, Ctrl+C to stop)";

  std::printf("\nTcpServer 已启动（echo），监听 %s:%u\n",
              settings.server_host.c_str(),
              static_cast<unsigned>(settings.server_port));
  std::printf("请用 nc/telnet 测试，例如: nc 192.168.162.136 %u\n",
              static_cast<unsigned>(settings.server_port));
  std::printf("注意：这是原始 TCP echo，不能用浏览器访问。\n\n");

  std::signal(SIGINT, OnSignal);
  std::signal(SIGTERM, OnSignal);
  while (!g_stop.load(std::memory_order_acquire)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  server.stop();
  rt->stop();
  return 0;
}
