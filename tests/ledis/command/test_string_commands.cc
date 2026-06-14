/**
 * @file test_string_commands.cc
 * @brief APPEND / STRLEN / SETEX
 */
#include "../test_common.h"

#include "ledis/command/registry.h"
#include "ledis/session/session_context.h"
#include "ledis/store/db_manager.h"

namespace {

ledis::CommandResult dispatch(ledis::CommandRegistry& registry, ledis::SessionContext& ctx,
                              ledis::DBManager& db, const char* name,
                              std::initializer_list<const char*> args) {
  ledis::Command cmd;
  cmd.name = ledis::Sds(name);
  for (const char* arg : args) {
    cmd.args.push_back(ledis::Sds(arg));
  }
  return registry.dispatch(ctx, db, cmd);
}

void test_append() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  LEDIS_CHECK(dispatch(registry, ctx, db, "APPEND", {"k", "hello"}).value.integer == 5);
  LEDIS_CHECK(dispatch(registry, ctx, db, "GET", {"k"}).value.bulk.str() == "hello");
  LEDIS_CHECK(dispatch(registry, ctx, db, "APPEND", {"k", " world"}).value.integer == 11);
  LEDIS_CHECK(dispatch(registry, ctx, db, "GET", {"k"}).value.bulk.str() == "hello world");

  dispatch(registry, ctx, db, "SET", {"ttl", "a", "EX", "60"});
  dispatch(registry, ctx, db, "APPEND", {"ttl", "b"});
  LEDIS_CHECK(dispatch(registry, ctx, db, "TTL", {"ttl"}).value.integer >= 0);
  LEDIS_CHECK(dispatch(registry, ctx, db, "GET", {"ttl"}).value.bulk.str() == "ab");

  dispatch(registry, ctx, db, "HSET", {"h", "f", "v"});
  const auto bad = dispatch(registry, ctx, db, "APPEND", {"h", "x"});
  LEDIS_CHECK(bad.value.type == ledis::RespType::kError);
  LEDIS_CHECK(bad.value.bulk.str().find("wrong type") != std::string::npos);
}

void test_strlen() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  LEDIS_CHECK(dispatch(registry, ctx, db, "STRLEN", {"missing"}).value.integer == 0);
  dispatch(registry, ctx, db, "SET", {"s", "abc"});
  LEDIS_CHECK(dispatch(registry, ctx, db, "STRLEN", {"s"}).value.integer == 3);

  dispatch(registry, ctx, db, "HSET", {"h", "f", "v"});
  const auto bad = dispatch(registry, ctx, db, "STRLEN", {"h"});
  LEDIS_CHECK(bad.value.type == ledis::RespType::kError);
}

void test_setex() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  LEDIS_CHECK(dispatch(registry, ctx, db, "SETEX", {"k", "10", "val"}).value.bulk.str() == "OK");
  LEDIS_CHECK(dispatch(registry, ctx, db, "GET", {"k"}).value.bulk.str() == "val");
  LEDIS_CHECK(dispatch(registry, ctx, db, "TTL", {"k"}).value.integer > 0);
  LEDIS_CHECK(dispatch(registry, ctx, db, "TTL", {"k"}).value.integer <= 10);

  const auto bad = dispatch(registry, ctx, db, "SETEX", {"k", "0", "x"});
  LEDIS_CHECK(bad.value.type == ledis::RespType::kError);

  dispatch(registry, ctx, db, "HSET", {"h", "f", "v"});
  const auto wrong = dispatch(registry, ctx, db, "SETEX", {"h", "5", "x"});
  LEDIS_CHECK(wrong.value.type == ledis::RespType::kError);
}

void test_pexpire_psetex_pttl() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  LEDIS_CHECK(dispatch(registry, ctx, db, "PSETEX", {"k", "5000", "v"}).value.bulk.str() == "OK");
  LEDIS_CHECK(dispatch(registry, ctx, db, "GET", {"k"}).value.bulk.str() == "v");
  const int64_t pttl = dispatch(registry, ctx, db, "PTTL", {"k"}).value.integer;
  LEDIS_CHECK(pttl > 0);
  LEDIS_CHECK(pttl <= 5000);

  LEDIS_CHECK(dispatch(registry, ctx, db, "PEXPIRE", {"k", "10000"}).value.integer == 1);
  LEDIS_CHECK(dispatch(registry, ctx, db, "PTTL", {"k"}).value.integer > 5000);
}

}  // namespace

int main() {
  test_append();
  test_strlen();
  test_setex();
  test_pexpire_psetex_pttl();
  std::printf("test_string_commands: OK\n");
  return 0;
}
