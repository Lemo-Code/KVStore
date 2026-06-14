/**
 * @file test_zset_commands.cc
 * @brief Phase 5.4：ZADD / ZSCORE / ZCARD / ZRANGE / ZREM / ZRANK + WRONGTYPE
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

void test_zset_commands() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  LEDIS_CHECK(dispatch(registry, ctx, db, "ZADD", {"z", "1", "a", "2", "b", "1", "a"})
                  .value.integer == 2);
  LEDIS_CHECK(dispatch(registry, ctx, db, "ZCARD", {"z"}).value.integer == 2);
  LEDIS_CHECK(dispatch(registry, ctx, db, "ZSCORE", {"z", "a"}).value.bulk.str() == "1");
  LEDIS_CHECK(dispatch(registry, ctx, db, "ZSCORE", {"z", "missing"}).value.isNullBulk());
  LEDIS_CHECK(dispatch(registry, ctx, db, "ZRANK", {"z", "a"}).value.integer == 0);
  LEDIS_CHECK(dispatch(registry, ctx, db, "ZRANK", {"z", "b"}).value.integer == 1);

  const auto range = dispatch(registry, ctx, db, "ZRANGE", {"z", "0", "-1", "WITHSCORES"})
                         .value;
  LEDIS_CHECK(range.type == ledis::RespType::kArray);
  LEDIS_CHECK(range.array.size() == 4);
  LEDIS_CHECK(range.array[0].bulk.str() == "a");
  LEDIS_CHECK(range.array[1].bulk.str() == "1");
  LEDIS_CHECK(range.array[2].bulk.str() == "b");
  LEDIS_CHECK(range.array[3].bulk.str() == "2");

  LEDIS_CHECK(dispatch(registry, ctx, db, "ZREM", {"z", "a"}).value.integer == 1);
  LEDIS_CHECK(dispatch(registry, ctx, db, "ZCARD", {"z"}).value.integer == 1);
  LEDIS_CHECK(dispatch(registry, ctx, db, "ZREM", {"z", "b"}).value.integer == 1);
  LEDIS_CHECK(dispatch(registry, ctx, db, "ZCARD", {"z"}).value.integer == 0);
}

void test_zrevrange_and_zrangebyscore() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  dispatch(registry, ctx, db, "ZADD", {"z", "1", "a", "2", "b", "3", "c"});

  const auto rev = dispatch(registry, ctx, db, "ZREVRANGE", {"z", "0", "-1", "WITHSCORES"})
                       .value;
  LEDIS_CHECK(rev.array.size() == 6);
  LEDIS_CHECK(rev.array[0].bulk.str() == "c");
  LEDIS_CHECK(rev.array[1].bulk.str() == "3");
  LEDIS_CHECK(rev.array[4].bulk.str() == "a");

  const auto byscore =
      dispatch(registry, ctx, db, "ZRANGEBYSCORE", {"z", "1", "2", "WITHSCORES"}).value;
  LEDIS_CHECK(byscore.array.size() == 4);
  LEDIS_CHECK(byscore.array[0].bulk.str() == "a");
  LEDIS_CHECK(byscore.array[2].bulk.str() == "b");

  const auto excl = dispatch(registry, ctx, db, "ZRANGEBYSCORE",
                             {"z", "(1", "2", "WITHSCORES"})
                      .value;
  LEDIS_CHECK(excl.array.size() == 2);
  LEDIS_CHECK(excl.array[0].bulk.str() == "b");

  const auto limited = dispatch(registry, ctx, db, "ZRANGEBYSCORE",
                                {"z", "-inf", "+inf", "LIMIT", "1", "1"})
                           .value;
  LEDIS_CHECK(limited.array.size() == 1);
  LEDIS_CHECK(limited.array[0].bulk.str() == "b");
}

void test_zrevrank_and_zrevrangebyscore() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  dispatch(registry, ctx, db, "ZADD", {"z", "1", "a", "2", "b", "3", "c"});
  LEDIS_CHECK(dispatch(registry, ctx, db, "ZREVRANK", {"z", "c"}).value.integer == 0);
  LEDIS_CHECK(dispatch(registry, ctx, db, "ZREVRANK", {"z", "a"}).value.integer == 2);

  const auto revscore = dispatch(registry, ctx, db, "ZREVRANGEBYSCORE",
                                 {"z", "1", "2", "WITHSCORES"})
                            .value;
  LEDIS_CHECK(revscore.array.size() == 4);
  LEDIS_CHECK(revscore.array[0].bulk.str() == "b");
  LEDIS_CHECK(revscore.array[2].bulk.str() == "a");
}

void test_zset_wrongtype() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  LEDIS_CHECK(dispatch(registry, ctx, db, "SET", {"k", "string"}).value.bulk.str() == "OK");
  const auto zadd = dispatch(registry, ctx, db, "ZADD", {"k", "1", "x"});
  LEDIS_CHECK(zadd.value.type == ledis::RespType::kError);
  LEDIS_CHECK(zadd.value.bulk.str().find("wrong type") != std::string::npos);

  LEDIS_CHECK(dispatch(registry, ctx, db, "ZADD", {"k2", "1", "v"}).value.integer == 1);
  const auto get = dispatch(registry, ctx, db, "GET", {"k2"});
  LEDIS_CHECK(get.value.type == ledis::RespType::kError);
  LEDIS_CHECK(dispatch(registry, ctx, db, "TYPE", {"k2"}).value.bulk.str() == "zset");
}

void test_zincrby_and_zcount() {
  ledis::DBManager db;
  ledis::SessionContext ctx;
  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();

  dispatch(registry, ctx, db, "ZADD", {"z", "1", "a", "2", "b", "3", "c"});

  LEDIS_CHECK(dispatch(registry, ctx, db, "ZCOUNT", {"z", "1", "2"}).value.integer == 2);

  LEDIS_CHECK(dispatch(registry, ctx, db, "ZINCRBY", {"z", "1.5", "a"}).value.bulk.str() == "2.5");
  LEDIS_CHECK(dispatch(registry, ctx, db, "ZSCORE", {"z", "a"}).value.bulk.str() == "2.5");
  LEDIS_CHECK(dispatch(registry, ctx, db, "ZINCRBY", {"z", "10", "new"}).value.bulk.str() == "10");

  LEDIS_CHECK(dispatch(registry, ctx, db, "ZCOUNT", {"z", "(1", "(3"}).value.integer == 2);
  LEDIS_CHECK(dispatch(registry, ctx, db, "ZCOUNT", {"z", "20", "30"}).value.integer == 0);
}

}  // namespace

int main() {
  test_zset_commands();
  test_zrevrange_and_zrangebyscore();
  test_zrevrank_and_zrevrangebyscore();
  test_zincrby_and_zcount();
  test_zset_wrongtype();
  return 0;
}
