/**
 * @file test_ping.cc
 */
#include "../test_common.h"

#include "ledis/command/registry.h"
#include "ledis/session/session_context.h"
#include "ledis/store/db_manager.h"

namespace {

void test_ping_get_set() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  ledis::Command ping;
  ping.name = ledis::Sds("PING");
  auto pong = registry.dispatch(ctx, db, ping);
  LEDIS_CHECK(pong.value.bulk.str() == "PONG");

  ledis::Command echo;
  echo.name = ledis::Sds("ECHO");
  echo.args.push_back(ledis::Sds("hello"));
  LEDIS_CHECK(registry.dispatch(ctx, db, echo).value.bulk.str() == "hello");

  ledis::Command set_cmd;
  set_cmd.name = ledis::Sds("SET");
  set_cmd.args.push_back(ledis::Sds("mykey"));
  set_cmd.args.push_back(ledis::Sds("myval"));
  auto ok = registry.dispatch(ctx, db, set_cmd);
  LEDIS_CHECK(ok.value.bulk.str() == "OK");

  ledis::Command get_cmd;
  get_cmd.name = ledis::Sds("GET");
  get_cmd.args.push_back(ledis::Sds("mykey"));
  auto got = registry.dispatch(ctx, db, get_cmd);
  LEDIS_CHECK(got.value.type == ledis::RespType::kBulkString);
  LEDIS_CHECK(got.value.bulk.str() == "myval");

  get_cmd.args[0] = ledis::Sds("missing");
  auto miss = registry.dispatch(ctx, db, get_cmd);
  LEDIS_CHECK(miss.value.isNullBulk());
}

}  // namespace

int main() {
  test_ping_get_set();
  std::printf("test_ping: OK\n");
  return 0;
}
