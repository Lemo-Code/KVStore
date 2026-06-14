/**
 * @file test_maxclients.cc
 * @brief maxclients 连接上限：超额连接立即关闭
 */
#include "../test_common.h"

#include "ledis/command/command.h"
#include "ledis/protocol/resp_writer.h"
#include "ledis/server/ledis_server.h"
#include "ledis/stream/ledis_stream.h"

#include "lemo/io/runtime.h"
#include "lemo/socket/address.h"
#include "lemo/socket/socket.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>

namespace {

bool waitUntil(std::atomic<int>* flag, int expect, int max_ms) {
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(max_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    if (flag->load() == expect) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return flag->load() == expect;
}

ledis::CommandResult readOneResponse(lemo::socket::Socket::ptr& sock) {
  ledis::LedisStream stream;
  for (int i = 0; i < 200; ++i) {
    const int fd = sock->getSocket();
    if (stream.readMore(fd) > 0) {
      ledis::CommandResult resp;
      if (stream.tryReadResponse(&resp) == ledis::ParseResult::kOk) {
        return resp;
      }
    }
    if (stream.readMore(fd) == 0) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return ledis::CommandResult::error("ERR test timeout");
}

void test_maxclients_reject() {
  ledis::LedisSettings settings;
  settings.host = "127.0.0.1";
  settings.port = 0;
  settings.single_thread_mode = true;
  settings.maxclients = 1;

  ledis::LedisServer server(settings);
  LEDIS_CHECK(server.start());
  const uint16_t port = server.boundPort();

  std::atomic<int> done{0};
  server.runtime()->iom().schedule([port, &done]() {
    lemo::socket::Socket::ptr c1 = lemo::socket::Socket::CreateTCPSocket();
    LEDIS_CHECK(c1->connect(
        lemo::socket::IPv4Address::Create("127.0.0.1", port)));

    lemo::socket::Socket::ptr c2 = lemo::socket::Socket::CreateTCPSocket();
    LEDIS_CHECK(c2->connect(
        lemo::socket::IPv4Address::Create("127.0.0.1", port)));

    char buf[16];
    const int n = c2->recv(buf, sizeof(buf));
    LEDIS_CHECK(n <= 0);

    ledis::Command ping;
    ping.name = ledis::Sds("PING");
    const std::string wire = ledis::RespWriter::encodeCommand(ping);
    LEDIS_CHECK(c1->send(wire.data(), wire.size()) ==
                static_cast<int>(wire.size()));
    auto pong = readOneResponse(c1);
    LEDIS_CHECK(pong.value.bulk.str() == "PONG");

    c1->close();
    c2->close();
    done.store(1);
  });

  LEDIS_CHECK(waitUntil(&done, 1, 5000));
  server.stop();
}

}  // namespace

int main() {
  test_maxclients_reject();
  std::printf("test_maxclients: OK\n");
  return 0;
}
