/**
 * @file test_ledis_stream.cc
 * @brief LedisStream：pipe 模拟长连接读写
 */
#include "../test_common.h"

#include "ledis/command/command.h"
#include "ledis/command/command_result.h"
#include "ledis/protocol/resp_writer.h"
#include "ledis/stream/ledis_stream.h"

#include <cstring>
#include <unistd.h>

namespace {

void test_client_server_roundtrip() {
  int fds[2];
  LEDIS_CHECK(pipe(fds) == 0);

  ledis::Command ping;
  ping.name = ledis::Sds("PING");
  const std::string cmd = ledis::RespWriter::encodeCommand(ping);

  LEDIS_CHECK(ledis::LedisStream::writeBytes(fds[1], cmd) ==
              static_cast<ssize_t>(cmd.size()));

  ledis::LedisStream server_stream;
  LEDIS_CHECK(server_stream.readMore(fds[0]) > 0);

  ledis::Command req;
  LEDIS_CHECK(server_stream.tryReadCommand(&req) == ledis::ParseResult::kOk);
  LEDIS_CHECK(req.name.str() == "PING");

  const std::string resp =
      ledis::RespWriter::encode(ledis::CommandResult::pong().value);
  LEDIS_CHECK(ledis::LedisStream::writeBytes(fds[1], resp) ==
              static_cast<ssize_t>(resp.size()));

  ledis::LedisStream client_stream;
  LEDIS_CHECK(client_stream.readMore(fds[0]) > 0);
  ledis::CommandResult out;
  LEDIS_CHECK(client_stream.tryReadResponse(&out) == ledis::ParseResult::kOk);
  LEDIS_CHECK(out.value.type == ledis::RespType::kSimpleString);
  LEDIS_CHECK(out.value.bulk.str() == "PONG");

  close(fds[0]);
  close(fds[1]);
}

void test_pipeline_on_stream() {
  int fds[2];
  LEDIS_CHECK(pipe(fds) == 0);

  const char* f1 = "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n";
  const char* f2 = "*1\r\n$4\r\nPING\r\n";
  std::string batch;
  batch.append(f1).append(f2);
  LEDIS_CHECK(ledis::LedisStream::writeBytes(fds[1], batch) ==
              static_cast<ssize_t>(batch.size()));

  ledis::LedisStream stream;
  LEDIS_CHECK(stream.readMore(fds[0]) > 0);

  ledis::Command r1;
  ledis::Command r2;
  LEDIS_CHECK(stream.tryReadCommand(&r1) == ledis::ParseResult::kOk);
  LEDIS_CHECK(stream.tryReadCommand(&r2) == ledis::ParseResult::kOk);
  LEDIS_CHECK(r1.name.str() == "GET");
  LEDIS_CHECK(r2.name.str() == "PING");
  LEDIS_CHECK(stream.readChain().empty());

  close(fds[0]);
  close(fds[1]);
}

}  // namespace

int main() {
  test_client_server_roundtrip();
  test_pipeline_on_stream();
  std::printf("test_ledis_stream: OK\n");
  return 0;
}
