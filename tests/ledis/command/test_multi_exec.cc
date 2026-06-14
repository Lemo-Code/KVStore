/**
 * @file test_multi_exec.cc
 * @brief MULTI / EXEC / DISCARD 事务
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

void test_multi_exec_basic() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  LEDIS_CHECK(dispatch(registry, ctx, db, "MULTI", {}).value.bulk.str() == "OK");
  LEDIS_CHECK(ctx.in_multi);
  LEDIS_CHECK(
      dispatch(registry, ctx, db, "SET", {"k", "1"}).value.bulk.str() == "QUEUED");
  LEDIS_CHECK(
      dispatch(registry, ctx, db, "INCR", {"k"}).value.bulk.str() == "QUEUED");
  LEDIS_CHECK(ctx.multi_queue.size() == 2);
  LEDIS_CHECK(!db.keyspace(0).exists(ledis::Sds("k")));

  const auto exec = dispatch(registry, ctx, db, "EXEC", {}).value;
  LEDIS_CHECK(!ctx.in_multi);
  LEDIS_CHECK(exec.type == ledis::RespType::kArray);
  LEDIS_CHECK(exec.array.size() == 2);
  LEDIS_CHECK(exec.array[0].bulk.str() == "OK");
  LEDIS_CHECK(exec.array[1].integer == 2);

  ledis::LedisObject obj;
  LEDIS_CHECK(db.keyspace(0).get(ledis::Sds("k"), &obj));
  ledis::Sds value;
  LEDIS_CHECK(obj.asString(&value));
  LEDIS_CHECK(value.str() == "2");
}

void test_discard() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  dispatch(registry, ctx, db, "MULTI", {});
  dispatch(registry, ctx, db, "SET", {"x", "y"});
  LEDIS_CHECK(dispatch(registry, ctx, db, "DISCARD", {}).value.bulk.str() == "OK");
  LEDIS_CHECK(!ctx.in_multi);
  LEDIS_CHECK(ctx.multi_queue.empty());
  LEDIS_CHECK(!db.keyspace(0).exists(ledis::Sds("x")));
}

void test_multi_errors() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  const auto exec = dispatch(registry, ctx, db, "EXEC", {}).value;
  LEDIS_CHECK(exec.type == ledis::RespType::kError);
  LEDIS_CHECK(exec.bulk.str().find("EXEC without MULTI") != std::string::npos);

  const auto discard = dispatch(registry, ctx, db, "DISCARD", {}).value;
  LEDIS_CHECK(discard.type == ledis::RespType::kError);

  dispatch(registry, ctx, db, "MULTI", {});
  const auto nested = dispatch(registry, ctx, db, "MULTI", {}).value;
  LEDIS_CHECK(nested.type == ledis::RespType::kError);
  LEDIS_CHECK(nested.bulk.str().find("can not be nested") != std::string::npos);
}

void test_exec_empty() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  dispatch(registry, ctx, db, "MULTI", {});
  const auto exec = dispatch(registry, ctx, db, "EXEC", {}).value;
  LEDIS_CHECK(exec.type == ledis::RespType::kArray);
  LEDIS_CHECK(exec.array.empty());
}

void test_exec_wrongtype_in_batch() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  dispatch(registry, ctx, db, "HSET", {"h", "f", "v"});
  dispatch(registry, ctx, db, "MULTI", {});
  dispatch(registry, ctx, db, "HGET", {"h", "f"});
  dispatch(registry, ctx, db, "HGET", {"h", "missing"});

  const auto exec = dispatch(registry, ctx, db, "EXEC", {}).value;
  LEDIS_CHECK(exec.array.size() == 2);
  LEDIS_CHECK(exec.array[0].bulk.str() == "v");
  LEDIS_CHECK(exec.array[1].type == ledis::RespType::kNull);
}

void test_watch_exec_abort() {
  ledis::DBManager db;
  ledis::SessionContext client1;
  ledis::SessionContext client2;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  dispatch(registry, client1, db, "SET", {"balance", "100"});
  LEDIS_CHECK(dispatch(registry, client1, db, "WATCH", {"balance"}).value.bulk.str() ==
              "OK");
  dispatch(registry, client1, db, "MULTI", {});
  dispatch(registry, client1, db, "GET", {"balance"});

  dispatch(registry, client2, db, "SET", {"balance", "999"});

  const auto exec = dispatch(registry, client1, db, "EXEC", {}).value;
  LEDIS_CHECK(exec.isNullBulk());
  LEDIS_CHECK(dispatch(registry, client1, db, "GET", {"balance"}).value.bulk.str() ==
              "999");
}

void test_watch_exec_success() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  dispatch(registry, ctx, db, "SET", {"x", "1"});
  dispatch(registry, ctx, db, "WATCH", {"x"});
  dispatch(registry, ctx, db, "MULTI", {});
  dispatch(registry, ctx, db, "INCR", {"x"});
  const auto exec = dispatch(registry, ctx, db, "EXEC", {}).value;
  LEDIS_CHECK(exec.type == ledis::RespType::kArray);
  LEDIS_CHECK(exec.array.size() == 1);
  LEDIS_CHECK(exec.array[0].integer == 2);
  LEDIS_CHECK(ctx.watch_tokens.empty());
}

void test_unwatch() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  dispatch(registry, ctx, db, "SET", {"k", "v"});
  dispatch(registry, ctx, db, "WATCH", {"k"});
  LEDIS_CHECK(!ctx.watch_tokens.empty());
  LEDIS_CHECK(dispatch(registry, ctx, db, "UNWATCH", {}).value.bulk.str() == "OK");
  LEDIS_CHECK(ctx.watch_tokens.empty());
}

}  // namespace

int main() {
  test_multi_exec_basic();
  test_discard();
  test_multi_errors();
  test_exec_empty();
  test_exec_wrongtype_in_batch();
  test_watch_exec_abort();
  test_watch_exec_success();
  test_unwatch();
  std::printf("test_multi_exec: OK\n");
  return 0;
}
