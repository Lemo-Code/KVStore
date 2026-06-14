/**
 * @file test_resp_writer.cc
 */
#include "../test_common.h"

#include "ledis/command/command.h"
#include "ledis/command/command_result.h"
#include "ledis/protocol/resp_writer.h"
#include "ledis/store/sds.h"

#include <string>
#include <vector>

namespace {

void test_encode_ok_pong() {
  LEDIS_CHECK(ledis::RespWriter::encodeSimpleString(ledis::Sds("OK")) ==
              "+OK\r\n");
  LEDIS_CHECK(ledis::RespWriter::encodeSimpleString(ledis::Sds("PONG")) ==
              "+PONG\r\n");
}

void test_encode_bulk_and_null() {
  LEDIS_CHECK(ledis::RespWriter::encodeBulk(ledis::Sds("bar")) ==
              "$3\r\nbar\r\n");
  LEDIS_CHECK(ledis::RespWriter::encodeNullBulk() == "$-1\r\n");
  LEDIS_CHECK(ledis::RespWriter::encodeBulk(ledis::Sds("", 0)) == "$0\r\n\r\n");
}

void test_encode_command() {
  std::vector<ledis::Sds> argv;
  argv.push_back(ledis::Sds("SET"));
  argv.push_back(ledis::Sds("mykey"));
  argv.push_back(ledis::Sds("myvalue"));
  const std::string wire = ledis::RespWriter::encodeCommand(argv);
  LEDIS_CHECK(wire ==
              "*3\r\n$3\r\nSET\r\n$5\r\nmykey\r\n$7\r\nmyvalue\r\n");
}

void test_result_helpers() {
  LEDIS_CHECK(
      ledis::RespWriter::encode(ledis::CommandResult::integer(2).value) ==
      ":2\r\n");
  LEDIS_CHECK(
      ledis::RespWriter::encode(ledis::CommandResult::error("ERR foo").value) ==
      "-ERR foo\r\n");
}

}  // namespace

int main() {
  test_encode_ok_pong();
  test_encode_bulk_and_null();
  test_encode_command();
  test_result_helpers();
  std::printf("test_resp_writer: OK\n");
  return 0;
}
