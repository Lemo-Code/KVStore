/**
 * @file test_list_commands.cc
 * @brief Phase 5.2：LPUSH / RPUSH / LPOP / RPOP / LLEN / LRANGE + WRONGTYPE
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

void test_list_commands() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  LEDIS_CHECK(dispatch(registry, ctx, db, "RPUSH", {"q", "a", "b", "c"}).value.integer == 3);
  LEDIS_CHECK(dispatch(registry, ctx, db, "LPUSH", {"q", "head"}).value.integer == 4);
  LEDIS_CHECK(dispatch(registry, ctx, db, "LLEN", {"q"}).value.integer == 4);

  const auto range = dispatch(registry, ctx, db, "LRANGE", {"q", "0", "-1"}).value;
  LEDIS_CHECK(range.type == ledis::RespType::kArray);
  LEDIS_CHECK(range.array.size() == 4);
  LEDIS_CHECK(range.array[0].bulk.str() == "head");
  LEDIS_CHECK(range.array[1].bulk.str() == "a");
  LEDIS_CHECK(range.array[2].bulk.str() == "b");
  LEDIS_CHECK(range.array[3].bulk.str() == "c");

  LEDIS_CHECK(dispatch(registry, ctx, db, "LPOP", {"q"}).value.bulk.str() == "head");
  LEDIS_CHECK(dispatch(registry, ctx, db, "RPOP", {"q"}).value.bulk.str() == "c");
  LEDIS_CHECK(dispatch(registry, ctx, db, "LLEN", {"q"}).value.integer == 2);

  const auto sub = dispatch(registry, ctx, db, "LRANGE", {"q", "0", "0"}).value;
  LEDIS_CHECK(sub.array.size() == 1);
  LEDIS_CHECK(sub.array[0].bulk.str() == "a");

  LEDIS_CHECK(dispatch(registry, ctx, db, "LPOP", {"q"}).value.bulk.str() == "a");
  LEDIS_CHECK(dispatch(registry, ctx, db, "RPOP", {"q"}).value.bulk.str() == "b");
  LEDIS_CHECK(dispatch(registry, ctx, db, "LLEN", {"q"}).value.integer == 0);
  LEDIS_CHECK(dispatch(registry, ctx, db, "LPOP", {"q"}).value.type == ledis::RespType::kNull);
}

void test_list_index_trim() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  dispatch(registry, ctx, db, "RPUSH", {"q", "a", "b", "c", "d", "e"});

  LEDIS_CHECK(dispatch(registry, ctx, db, "LINDEX", {"q", "0"}).value.bulk.str() == "a");
  LEDIS_CHECK(dispatch(registry, ctx, db, "LINDEX", {"q", "-1"}).value.bulk.str() == "e");
  LEDIS_CHECK(dispatch(registry, ctx, db, "LINDEX", {"q", "99"}).value.type ==
              ledis::RespType::kNull);

  LEDIS_CHECK(dispatch(registry, ctx, db, "LTRIM", {"q", "1", "3"}).value.bulk.str() == "OK");
  const auto range = dispatch(registry, ctx, db, "LRANGE", {"q", "0", "-1"}).value;
  LEDIS_CHECK(range.array.size() == 3);
  LEDIS_CHECK(range.array[0].bulk.str() == "b");
  LEDIS_CHECK(range.array[2].bulk.str() == "d");

  LEDIS_CHECK(dispatch(registry, ctx, db, "LTRIM", {"q", "5", "10"}).value.bulk.str() == "OK");
  LEDIS_CHECK(dispatch(registry, ctx, db, "LLEN", {"q"}).value.integer == 0);
}

void test_list_wrongtype() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  LEDIS_CHECK(dispatch(registry, ctx, db, "SET", {"k", "string"}).value.bulk.str() == "OK");
  const auto lpush = dispatch(registry, ctx, db, "LPUSH", {"k", "x"});
  LEDIS_CHECK(lpush.value.type == ledis::RespType::kError);
  LEDIS_CHECK(lpush.value.bulk.str().find("wrong type") != std::string::npos);

  LEDIS_CHECK(dispatch(registry, ctx, db, "RPUSH", {"k2", "v"}).value.integer == 1);
  const auto get = dispatch(registry, ctx, db, "GET", {"k2"});
  LEDIS_CHECK(get.value.type == ledis::RespType::kError);
}

}  // namespace

int main() {
  test_list_commands();
  test_list_index_trim();
  test_list_wrongtype();
  return 0;
}
