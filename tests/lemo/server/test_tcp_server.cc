/**
 * @file test_tcp_server.cc
 * @brief TcpServer + Runtime 集成测试。
 */
#include "../io/test_io_common.h"

#include "lemo/io/runtime.h"
#include "lemo/server/tcp_server.h"
#include "lemo/socket/address.h"
#include "lemo/socket/socket.h"

#include <atomic>
#include <csignal>
#include <cstring>

namespace {

using lemo_io_test::wait_eq;

void EchoHandler(const lemo::socket::Socket::ptr& sock) {
  char buf[256];
  while (true) {
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

void test_tcp_server_local_port() {
  lemo::io::Runtime rt(2, false, "test_tcp_port");
  lemo::server::TcpServer server("test", &rt);

  lemo::socket::Socket::ptr listen = lemo::socket::Socket::CreateTCPSocket();
  LEMO_CHECK(listen->bind(lemo::socket::IPv4Address::Create("127.0.0.1", 0)));
  LEMO_CHECK(listen->listen());
  lemo::socket::Address::ptr la = listen->getLocalAddress();
  lemo::socket::IPv4Address::ptr v4 =
      std::dynamic_pointer_cast<lemo::socket::IPv4Address>(la);
  LEMO_CHECK(v4 != nullptr);
  const uint16_t port = v4->getPort();
  listen->close();

  LEMO_CHECK(server.bind("127.0.0.1", port));
  server.setConnectionHandler(EchoHandler);
  LEMO_CHECK(server.start());

  std::atomic<int> done{0};
  rt.iom().schedule([port, &done]() {
    lemo::socket::Socket::ptr sock = lemo::socket::Socket::CreateTCPSocket();
    LEMO_CHECK(sock->connect(lemo::socket::IPv4Address::Create("127.0.0.1", port)));
    const char* msg = "tcp-server-echo";
    const size_t len = std::strlen(msg);
    LEMO_CHECK(sock->send(msg, len) == static_cast<int>(len));
    char buf[64] = {};
    LEMO_CHECK(sock->recv(buf, len) == static_cast<int>(len));
    LEMO_CHECK(std::memcmp(buf, msg, len) == 0);
    sock->close();
    done.store(1);
  });

  LEMO_CHECK(wait_eq(done, 1, 5000));
  server.stop();
  rt.stop();
}

}  // namespace

int main() {
  signal(SIGPIPE, SIG_IGN);
  test_tcp_server_local_port();
  std::printf("PASS test_tcp_server\n");
  return 0;
}
