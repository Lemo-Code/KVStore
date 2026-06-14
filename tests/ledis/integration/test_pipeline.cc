/**
 * @file test_pipeline.cc
 * @brief Pipeline：一次发送多命令，响应顺序一致
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
#include <string>
#include <thread>
#include <vector>

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

std::vector<ledis::CommandResult> readAllResponses(lemo::socket::Socket::ptr& sock,
                                                   size_t expect_count) {
  ledis::LedisStream stream;
  std::vector<ledis::CommandResult> out;
  for (int i = 0; i < 500 && out.size() < expect_count; ++i) {
    const int fd = sock->getSocket();
    stream.readMore(fd);
    ledis::CommandResult resp;
    while (stream.tryReadResponse(&resp) == ledis::ParseResult::kOk) {
      out.push_back(resp);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return out;
}

void test_pipeline_order() {
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

    std::string batch;
    ledis::Command set;
    set.name = ledis::Sds("SET");
    set.args.push_back(ledis::Sds("p"));
    set.args.push_back(ledis::Sds("v"));
    batch += ledis::RespWriter::encodeCommand(set);

    ledis::Command get;
    get.name = ledis::Sds("GET");
    get.args.push_back(ledis::Sds("p"));
    batch += ledis::RespWriter::encodeCommand(get);

    ledis::Command ping;
    ping.name = ledis::Sds("PING");
    batch += ledis::RespWriter::encodeCommand(ping);

    LEDIS_CHECK(sock->send(batch.data(), batch.size()) ==
                static_cast<int>(batch.size()));

    const std::vector<ledis::CommandResult> resps = readAllResponses(sock, 3);
    LEDIS_CHECK(resps.size() == 3);
    LEDIS_CHECK(resps[0].value.bulk.str() == "OK");
    LEDIS_CHECK(resps[1].value.bulk.str() == "v");
    LEDIS_CHECK(resps[2].value.bulk.str() == "PONG");

    sock->close();
    done.store(1);
  });

  LEDIS_CHECK(waitUntil(&done, 1, 5000));
  server.stop();
}

}  // namespace

int main() {
  test_pipeline_order();
  std::printf("test_pipeline: OK\n");
  return 0;
}
