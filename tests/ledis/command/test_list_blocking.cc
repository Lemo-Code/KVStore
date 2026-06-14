/**
 * @file test_list_blocking.cc
 * @brief BLPOP / BRPOP 阻塞弹出
 */
#include "../test_common.h"

#include "ledis/command/registry.h"
#include "ledis/session/blocking_list.h"
#include "ledis/session/reply_router.h"
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

void test_blpop_immediate() {
  ledis::DBManager db;
  ledis::ReplyRouter router(1);
  ledis::BlockingListHub hub;
  hub.setReplyRouter(&router);

  ledis::SessionContext ctx;
  ctx.conn_id = 1;
  ctx.io_thread_id = 0;

  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();
  RegisterBlockingListCommands(&registry, &hub);

  dispatch(registry, ctx, db, "RPUSH", {"q", "hello"});
  const auto pop = dispatch(registry, ctx, db, "BLPOP", {"q", "0"});
  LEDIS_CHECK(!pop.blocked);
  LEDIS_CHECK(pop.value.type == ledis::RespType::kArray);
  LEDIS_CHECK(pop.value.array[0].bulk.str() == "q");
  LEDIS_CHECK(pop.value.array[1].bulk.str() == "hello");
}

void test_blpop_blocking_wake() {
  ledis::DBManager db;
  ledis::ReplyRouter router(1);
  ledis::BlockingListHub hub;
  hub.setReplyRouter(&router);

  ledis::SessionContext waiter;
  waiter.conn_id = 2;
  waiter.io_thread_id = 0;

  ledis::SessionContext pusher;
  pusher.conn_id = 3;
  pusher.io_thread_id = 0;

  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();
  RegisterBlockingListCommands(&registry, &hub);

  const auto blocked = dispatch(registry, waiter, db, "BLPOP", {"tasks", "5"});
  LEDIS_CHECK(blocked.blocked);

  dispatch(registry, pusher, db, "LPUSH", {"tasks", "job1"});
  hub.signalPush(db, 0, ledis::Sds("tasks"));

  ledis::ReplyEnvelope reply;
  LEDIS_CHECK(router.tryPop(0, &reply));
  LEDIS_CHECK(reply.conn_id == 2);
  LEDIS_CHECK(reply.result.value.type == ledis::RespType::kArray);
  LEDIS_CHECK(reply.result.value.array[0].bulk.str() == "tasks");
  LEDIS_CHECK(reply.result.value.array[1].bulk.str() == "job1");
}

void test_brpop_and_wrongtype() {
  ledis::DBManager db;
  ledis::ReplyRouter router(1);
  ledis::BlockingListHub hub;
  hub.setReplyRouter(&router);

  ledis::SessionContext ctx;
  ctx.conn_id = 4;
  ctx.io_thread_id = 0;

  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();
  RegisterBlockingListCommands(&registry, &hub);

  dispatch(registry, ctx, db, "RPUSH", {"rq", "a", "b"});
  const auto pop = dispatch(registry, ctx, db, "BRPOP", {"rq", "0"}).value;
  LEDIS_CHECK(pop.array[1].bulk.str() == "b");

  dispatch(registry, ctx, db, "SET", {"bad", "x"});
  const auto wt = dispatch(registry, ctx, db, "BLPOP", {"bad", "0"});
  LEDIS_CHECK(wt.value.type == ledis::RespType::kError);
}

void test_rpoplpush_and_brpoplpush() {
  ledis::DBManager db;
  ledis::ReplyRouter router(1);
  ledis::BlockingListHub hub;
  hub.setReplyRouter(&router);

  ledis::SessionContext ctx;
  ctx.conn_id = 5;
  ctx.io_thread_id = 0;

  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();
  RegisterBlockingListCommands(&registry, &hub);

  dispatch(registry, ctx, db, "RPUSH", {"src", "a", "b"});
  const auto moved =
      dispatch(registry, ctx, db, "RPOPLPUSH", {"src", "dst"}).value;
  LEDIS_CHECK(moved.bulk.str() == "b");
  LEDIS_CHECK(dispatch(registry, ctx, db, "LLEN", {"src"}).value.integer == 1);
  LEDIS_CHECK(dispatch(registry, ctx, db, "LLEN", {"dst"}).value.integer == 1);

  ledis::SessionContext waiter;
  waiter.conn_id = 6;
  waiter.io_thread_id = 0;
  LEDIS_CHECK(dispatch(registry, waiter, db, "BRPOPLPUSH", {"empty", "dst2", "2"})
                  .blocked);
  dispatch(registry, ctx, db, "LPUSH", {"empty", "job"});
  hub.signalPush(db, 0, ledis::Sds("empty"));
  ledis::ReplyEnvelope reply;
  LEDIS_CHECK(router.tryPop(0, &reply));
  LEDIS_CHECK(reply.conn_id == 6);
  LEDIS_CHECK(reply.result.value.bulk.str() == "job");
}

}  // namespace

int main() {
  test_blpop_immediate();
  test_blpop_blocking_wake();
  test_brpop_and_wrongtype();
  test_rpoplpush_and_brpoplpush();
  std::printf("test_list_blocking: OK\n");
  return 0;
}
