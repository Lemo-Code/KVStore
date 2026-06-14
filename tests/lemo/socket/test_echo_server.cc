/**
 * @file test_echo_server.cc
 * @brief Echo 功能与数据完整性测试（Socket + RingBuffer + IOManager）。
 */
#include "../io/test_io_common.h"

#include "lemo/fiber/fiber.h"
#include "lemo/buffer/ring_buffer.h"
#include "lemo/io/iomanager.h"
#include "lemo/socket/address.h"
#include "lemo/socket/socket.h"

#include <atomic>
#include <cstdio>
#include <csignal>
#include <cstring>
#include <vector>

namespace {

using lemo_io_test::wait_eq;

lemo::socket::Socket::ptr MakeListen(const std::string& host, uint16_t port) {
  lemo::socket::Socket::ptr listen = lemo::socket::Socket::CreateTCPSocket();
  lemo::socket::IPv4Address::ptr addr =
      lemo::socket::IPv4Address::Create(host.c_str(), port);
  if (!addr || !listen->bind(addr) || !listen->listen(128)) {
    return nullptr;
  }
  return listen;
}

uint16_t BoundPort(const lemo::socket::Socket::ptr& listen) {
  lemo::socket::Address::ptr addr = listen->getLocalAddress();
  lemo::socket::IPv4Address::ptr v4 =
      std::dynamic_pointer_cast<lemo::socket::IPv4Address>(addr);
  return v4 ? v4->getPort() : 0;
}

void EchoSession(const lemo::socket::Socket::ptr& sock) {
  lemo::buffer::RingBuffer buf(4096);
  char tmp[4096];
  while (true) {
    const int n = sock->recv(tmp, sizeof(tmp));
    if (n <= 0) {
      break;
    }
    buf.write(tmp, static_cast<size_t>(n));
    char out[4096];
    LEMO_CHECK(buf.read(out, static_cast<size_t>(n)) == static_cast<size_t>(n));
    LEMO_CHECK(std::memcmp(tmp, out, static_cast<size_t>(n)) == 0);
    buf.clear();
    int sent = 0;
    while (sent < n) {
      const int w =
          sock->send(out + sent, static_cast<size_t>(n - sent));
      LEMO_CHECK(w > 0);
      sent += w;
    }
  }
  sock->close();
}

void AcceptLoop(lemo::io::IOManager* iom, const lemo::socket::Socket::ptr& listen,
                const std::atomic<bool>& stop) {
  while (!stop.load(std::memory_order_acquire)) {
    lemo::socket::Socket::ptr client = listen->accept();
    if (!client) {
      if (stop.load(std::memory_order_acquire)) {
        break;
      }
      continue;
    }
    iom->schedule([client]() { EchoSession(client); });
  }
}

void test_socket_bind_connect() {
  lemo::socket::Socket::ptr listen = MakeListen("127.0.0.1", 0);
  LEMO_CHECK(listen != nullptr);
  lemo::socket::Socket::ptr client = lemo::socket::Socket::CreateTCPSocket();
  lemo::socket::IPv4Address::ptr addr =
      lemo::socket::IPv4Address::Create("127.0.0.1", BoundPort(listen));
  LEMO_CHECK(client->connect(addr));
  lemo::socket::Socket::ptr accepted = listen->accept();
  LEMO_CHECK(accepted != nullptr);
  client->close();
  accepted->close();
  listen->close();
}

void test_echo_local() {
  lemo::io::IOManager iom(2, false, "test_echo_local");
  lemo::socket::Socket::ptr listen = MakeListen("127.0.0.1", 0);
  LEMO_CHECK(listen != nullptr);
  const uint16_t port = BoundPort(listen);

  std::atomic<bool> stop{false};
  std::atomic<int> done{0};

  iom.schedule([&]() { AcceptLoop(&iom, listen, stop); });

  iom.schedule([port, &done]() {
    lemo::socket::Socket::ptr sock = lemo::socket::Socket::CreateTCPSocket();
    lemo::socket::IPv4Address::ptr addr =
        lemo::socket::IPv4Address::Create("127.0.0.1", port);
    LEMO_CHECK(sock->connect(addr));
    const char* msg = "hello-echo-buffer";
    const size_t len = std::strlen(msg);
    LEMO_CHECK(sock->send(msg, len) == static_cast<int>(len));
    char buf[64] = {};
    LEMO_CHECK(sock->recv(buf, len) == static_cast<int>(len));
    LEMO_CHECK(std::memcmp(buf, msg, len) == 0);
    sock->close();
    done.store(1);
  });

  LEMO_CHECK(wait_eq(done, 1, 5000));
  stop.store(true);
  listen->close();
  lemo_io_test::sleep_ms(50);
  iom.stop();
}

void test_echo_concurrent_integrity() {
  lemo::io::IOManager iom(2, false, "test_echo_concurrent");
  lemo::socket::Socket::ptr listen = MakeListen("127.0.0.1", 0);
  LEMO_CHECK(listen != nullptr);
  const uint16_t port = BoundPort(listen);

  std::atomic<bool> stop{false};
  const int kClients = 2;
  const int kMsgs = 5;
  const int kPayload = 64;
  std::atomic<int> roundtrips{0};
  const int target = kClients * kMsgs;

  iom.schedule([&]() { AcceptLoop(&iom, listen, stop); });

  for (int i = 0; i < kClients; ++i) {
    iom.schedule([port, kMsgs, kPayload, &roundtrips, i]() {
      lemo::socket::Socket::ptr sock = lemo::socket::Socket::CreateTCPSocket();
      lemo::socket::IPv4Address::ptr addr =
          lemo::socket::IPv4Address::Create("127.0.0.1", port);
      LEMO_CHECK(sock->connect(addr));
      std::vector<char> sendbuf(static_cast<size_t>(kPayload));
      std::vector<char> recvbuf(static_cast<size_t>(kPayload));
      for (int m = 0; m < kMsgs; ++m) {
        for (int j = 0; j < kPayload; ++j) {
          sendbuf[static_cast<size_t>(j)] =
              static_cast<char>((i * 17 + m * 31 + j) & 0xFF);
        }
        LEMO_CHECK(sock->send(sendbuf.data(), sendbuf.size()) ==
                   static_cast<int>(sendbuf.size()));
        LEMO_CHECK(sock->recv(recvbuf.data(), recvbuf.size()) ==
                   static_cast<int>(recvbuf.size()));
        LEMO_CHECK(std::memcmp(sendbuf.data(), recvbuf.data(), sendbuf.size()) ==
                   0);
        roundtrips.fetch_add(1);
      }
      sock->close();
    });
  }

  LEMO_CHECK(lemo_io_test::wait_eq(roundtrips, target, 5000));
  stop.store(true, std::memory_order_release);
  listen->close();
  lemo_io_test::sleep_ms(100);
  iom.stop();
}

}  // namespace

int main() {
  signal(SIGPIPE, SIG_IGN);
  test_socket_bind_connect();
  test_echo_local();
  test_echo_concurrent_integrity();
  std::printf("PASS test_echo_server\n");
  return 0;
}
