/**
 * @file test_resp_reader.cc
 * @brief RespReader：半包、粘包、Pipeline、非法帧（protocol.md §10）
 */
#include "../test_common.h"

#include "lemo/buffer/chain_buffer.h"
#include "ledis/command/command.h"
#include "ledis/command/command_result.h"
#include "ledis/protocol/resp_reader.h"

#include <cstring>
#include <string>

namespace {

void feed(lemo::buffer::ChainBuffer& chain, const char* s) {
  chain.append(s, std::strlen(s));
}

void test_get_command_single_frame() {
  lemo::buffer::ChainBuffer chain;
  feed(chain, "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n");

  ledis::RespReader reader(chain);
  ledis::Command cmd;
  size_t consumed = 0;
  LEDIS_CHECK(reader.tryParseCommand(&cmd, &consumed) == ledis::ParseResult::kOk);
  LEDIS_CHECK(cmd.name.str() == "GET");
  LEDIS_CHECK(cmd.args.size() == 1);
  LEDIS_CHECK(cmd.args[0].str() == "foo");
  chain.consume(consumed);
  LEDIS_CHECK(chain.empty());
}

void test_half_packet() {
  lemo::buffer::ChainBuffer chain(8);
  ledis::RespReader reader(chain);
  ledis::Command cmd;
  size_t consumed = 0;

  feed(chain, "*2\r\n$3\r\n");
  LEDIS_CHECK(reader.tryParseCommand(&cmd, &consumed) ==
              ledis::ParseResult::kNeedMore);

  feed(chain, "GET\r\n$3\r\nfo");
  LEDIS_CHECK(reader.tryParseCommand(&cmd, &consumed) ==
              ledis::ParseResult::kNeedMore);

  feed(chain, "o\r\n");
  LEDIS_CHECK(reader.tryParseCommand(&cmd, &consumed) == ledis::ParseResult::kOk);
  LEDIS_CHECK(cmd.name.str() == "GET");
}

void test_pipeline_two_commands() {
  lemo::buffer::ChainBuffer chain;
  const char* f1 = "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n";
  const char* f2 = "*1\r\n$4\r\nPING\r\n";
  feed(chain, f1);
  feed(chain, f2);

  ledis::RespReader reader(chain);
  ledis::Command cmd;

  LEDIS_CHECK(reader.parseCommand(&cmd) == ledis::ParseResult::kOk);
  LEDIS_CHECK(cmd.name.str() == "GET");

  LEDIS_CHECK(reader.parseCommand(&cmd) == ledis::ParseResult::kOk);
  LEDIS_CHECK(cmd.name.str() == "PING");
  LEDIS_CHECK(chain.empty());
}

void test_response_parser_integer() {
  lemo::buffer::ChainBuffer chain;
  feed(chain, ":2\r\n");

  ledis::RespReader reader(chain);
  ledis::RespValue v;
  LEDIS_CHECK(reader.parseOne(&v) == ledis::ParseResult::kOk);
  LEDIS_CHECK(v.type == ledis::RespType::kInteger);
  LEDIS_CHECK(v.integer == 2);
}

void test_null_bulk_get_miss() {
  lemo::buffer::ChainBuffer chain;
  feed(chain, "$-1\r\n");

  ledis::RespReader reader(chain);
  ledis::RespValue v;
  LEDIS_CHECK(reader.parseOne(&v) == ledis::ParseResult::kOk);
  LEDIS_CHECK(v.isNullBulk());
}

void test_invalid_array_element() {
  lemo::buffer::ChainBuffer chain;
  feed(chain, "*2\r\n+OK\r\n$3\r\nfoo\r\n");

  ledis::RespReader reader(chain);
  ledis::Command cmd;
  size_t consumed = 0;
  LEDIS_CHECK(reader.tryParseCommand(&cmd, &consumed) ==
              ledis::ParseResult::kProtocolError);
}

}  // namespace

int main() {
  test_get_command_single_frame();
  test_half_packet();
  test_pipeline_two_commands();
  test_response_parser_integer();
  test_null_bulk_get_miss();
  test_invalid_array_element();
  std::printf("test_resp_reader: OK\n");
  return 0;
}
