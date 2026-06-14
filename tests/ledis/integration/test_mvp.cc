/**
 * @file test_mvp.cc
 * @brief LedisServer 集成：PING / SET / GET / DEL
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
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return ledis::CommandResult::error("ERR test timeout");
}

void sendCommand(lemo::socket::Socket::ptr& sock, const ledis::Command& cmd) {
  const std::string wire = ledis::RespWriter::encodeCommand(cmd);
  LEDIS_CHECK(sock->send(wire.data(), wire.size()) ==
              static_cast<int>(wire.size()));
}

void test_mvp_single_thread() {
  ledis::LedisSettings settings;
  settings.host = "127.0.0.1";
  settings.port = 0;
  settings.single_thread_mode = true;
  settings.io_threads = 1;

  ledis::LedisServer server(settings);
  LEDIS_CHECK(server.start());
  const uint16_t port = server.boundPort();

  std::atomic<int> done{0};
  server.runtime()->iom().schedule([port, &done]() {
    lemo::socket::Socket::ptr sock = lemo::socket::Socket::CreateTCPSocket();
    LEDIS_CHECK(sock->connect(
        lemo::socket::IPv4Address::Create("127.0.0.1", port)));

    ledis::Command ping;
    ping.name = ledis::Sds("PING");
    sendCommand(sock, ping);
    auto pong = readOneResponse(sock);
    LEDIS_CHECK(pong.value.bulk.str() == "PONG");

    ledis::Command set;
    set.name = ledis::Sds("SET");
    set.args.push_back(ledis::Sds("foo"));
    set.args.push_back(ledis::Sds("bar"));
    sendCommand(sock, set);
    auto ok = readOneResponse(sock);
    LEDIS_CHECK(ok.value.bulk.str() == "OK");

    ledis::Command get;
    get.name = ledis::Sds("GET");
    get.args.push_back(ledis::Sds("foo"));
    sendCommand(sock, get);
    auto val = readOneResponse(sock);
    LEDIS_CHECK(val.value.bulk.str() == "bar");

    ledis::Command del;
    del.name = ledis::Sds("DEL");
    del.args.push_back(ledis::Sds("foo"));
    sendCommand(sock, del);
    auto n = readOneResponse(sock);
    LEDIS_CHECK(n.value.type == ledis::RespType::kInteger);
    LEDIS_CHECK(n.value.integer == 1);

    sock->close();
    done.store(1);
  });

  LEDIS_CHECK(waitUntil(&done, 1, 5000));
  server.stop();
}

}  // namespace

int main() {
  test_mvp_single_thread();
  std::printf("test_mvp: OK\n");
  return 0;
}
