/**
 * @file test_pubsub_commands.cc
 * @brief SUBSCRIBE / UNSUBSCRIBE / PUBLISH / PUBSUB
 */
#include "../test_common.h"

#include "ledis/command/registry.h"
#include "ledis/session/pubsub.h"
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

void test_subscribe_publish() {
  ledis::DBManager db;
  ledis::ReplyRouter router(1);
  ledis::PubSubHub hub;
  hub.setReplyRouter(&router);

  ledis::SessionContext sub_ctx;
  sub_ctx.conn_id = 1;
  sub_ctx.io_thread_id = 0;

  ledis::SessionContext pub_ctx;
  pub_ctx.conn_id = 2;
  pub_ctx.io_thread_id = 0;

  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();
  RegisterPubSubCommands(&registry, &hub);

  const auto sub = dispatch(registry, sub_ctx, db, "SUBSCRIBE", {"news", "alert"}).value;
  LEDIS_CHECK(sub.type == ledis::RespType::kArray);
  LEDIS_CHECK(sub.array[0].bulk.str() == "subscribe");
  LEDIS_CHECK(sub.array[1].bulk.str() == "news");
  LEDIS_CHECK(sub.array[2].integer == 1);
  LEDIS_CHECK(sub_ctx.pubsub_channels.size() == 2);

  LEDIS_CHECK(dispatch(registry, pub_ctx, db, "PUBLISH", {"news", "hello"}).value.integer == 1);

  ledis::ReplyEnvelope push;
  LEDIS_CHECK(router.tryPop(0, &push));
  LEDIS_CHECK(push.conn_id == 1);
  LEDIS_CHECK(push.unsolicited);
  LEDIS_CHECK(push.result.value.type == ledis::RespType::kArray);
  LEDIS_CHECK(push.result.value.array[0].bulk.str() == "message");
  LEDIS_CHECK(push.result.value.array[1].bulk.str() == "news");
  LEDIS_CHECK(push.result.value.array[2].bulk.str() == "hello");

  const auto get = dispatch(registry, sub_ctx, db, "GET", {"k"});
  LEDIS_CHECK(get.value.type == ledis::RespType::kError);
  LEDIS_CHECK(get.value.bulk.str().find("allowed in this context") != std::string::npos);

  const auto numsub =
      dispatch(registry, pub_ctx, db, "PUBSUB", {"NUMSUB", "news", "missing"}).value;
  LEDIS_CHECK(numsub.array.size() == 4);
  LEDIS_CHECK(numsub.array[1].integer == 1);
  LEDIS_CHECK(numsub.array[3].integer == 0);

  LEDIS_CHECK(dispatch(registry, sub_ctx, db, "UNSUBSCRIBE", {"news"}).value.array[1].bulk.str() ==
              "news");
  LEDIS_CHECK(sub_ctx.pubsub_channels.size() == 1);
  hub.disconnect(1);
}

void test_pubsub_channels() {
  ledis::DBManager db;
  ledis::PubSubHub hub;
  ledis::SessionContext sub_ctx;
  sub_ctx.conn_id = 3;
  sub_ctx.io_thread_id = 0;

  ledis::SessionContext admin_ctx;

  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();
  RegisterPubSubCommands(&registry, &hub);

  dispatch(registry, sub_ctx, db, "SUBSCRIBE", {"chan:a", "chan:b"});
  const auto channels = dispatch(registry, admin_ctx, db, "PUBSUB", {"CHANNELS"}).value;
  LEDIS_CHECK(channels.array.size() == 2);
  hub.disconnect(3);
}

void test_psubscribe_publish() {
  LEDIS_CHECK(ledis::pubsubPatternMatch("news.*", "news.sport"));
  LEDIS_CHECK(!ledis::pubsubPatternMatch("news.*", "alert.sport"));
  LEDIS_CHECK(ledis::pubsubPatternMatch("?", "a"));
  LEDIS_CHECK(ledis::pubsubPatternMatch("[abc]", "b"));

  ledis::DBManager db;
  ledis::ReplyRouter router(1);
  ledis::PubSubHub hub;
  hub.setReplyRouter(&router);

  ledis::SessionContext sub_ctx;
  sub_ctx.conn_id = 10;
  sub_ctx.io_thread_id = 0;

  ledis::SessionContext pub_ctx;
  pub_ctx.conn_id = 11;
  pub_ctx.io_thread_id = 0;

  ledis::CommandRegistry registry;
  registry.registerDefaultCommands();
  RegisterPubSubCommands(&registry, &hub);

  const auto sub =
      dispatch(registry, sub_ctx, db, "PSUBSCRIBE", {"news.*", "alert?"}).value;
  LEDIS_CHECK(sub.type == ledis::RespType::kArray);
  LEDIS_CHECK(sub.array[0].bulk.str() == "psubscribe");
  LEDIS_CHECK(sub.array[1].bulk.str() == "news.*");
  LEDIS_CHECK(sub.array[2].integer == 1);
  LEDIS_CHECK(sub_ctx.pubsub_patterns.size() == 2);

  LEDIS_CHECK(dispatch(registry, pub_ctx, db, "PUBLISH", {"news.tech", "hi"}).value.integer ==
              1);
  ledis::ReplyEnvelope push;
  LEDIS_CHECK(router.tryPop(0, &push));
  LEDIS_CHECK(push.result.value.array[0].bulk.str() == "pmessage");
  LEDIS_CHECK(push.result.value.array[1].bulk.str() == "news.*");
  LEDIS_CHECK(push.result.value.array[2].bulk.str() == "news.tech");
  LEDIS_CHECK(push.result.value.array[3].bulk.str() == "hi");

  LEDIS_CHECK(dispatch(registry, pub_ctx, db, "PUBLISH", {"alert1", "x"}).value.integer == 1);
  LEDIS_CHECK(router.tryPop(0, &push));
  LEDIS_CHECK(push.result.value.array[1].bulk.str() == "alert?");

  const auto numpat = dispatch(registry, pub_ctx, db, "PUBSUB", {"NUMPAT"}).value;
  LEDIS_CHECK(numpat.integer == 2);

  LEDIS_CHECK(
      dispatch(registry, sub_ctx, db, "PUNSUBSCRIBE", {"news.*"}).value.array[1].bulk.str() ==
      "news.*");
  LEDIS_CHECK(sub_ctx.pubsub_patterns.size() == 1);
  hub.disconnect(10);
}

}  // namespace

int main() {
  test_subscribe_publish();
  test_pubsub_channels();
  test_psubscribe_publish();
  std::printf("test_pubsub_commands: OK\n");
  return 0;
}
