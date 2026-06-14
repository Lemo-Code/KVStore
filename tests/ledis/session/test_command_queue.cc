/**
 * @file test_command_queue.cc
 * @brief InboundQueue(MPSC) + ReplyRouter(SPSC) 单元测试
 */
#include "../test_common.h"

#include "ledis/session/command_envelope.h"
#include "ledis/session/inbound_queue.h"
#include "ledis/session/reply_router.h"

namespace {

void test_inbound_mpsc() {
  ledis::InboundQueue inbound(1024);
  ledis::CommandEnvelope env;
  env.conn_id = 7;
  env.io_thread_id = 0;
  env.seq = 1;
  env.cmd.name = ledis::Sds("PING");
  LEDIS_CHECK(inbound.tryPush(env));

  ledis::CommandEnvelope out;
  LEDIS_CHECK(inbound.tryPop(&out));
  LEDIS_CHECK(out.conn_id == 7);
  LEDIS_CHECK(out.cmd.name.str() == "PING");
  LEDIS_CHECK(inbound.empty());
}

void test_reply_router_spsc() {
  ledis::ReplyRouter router(2);
  ledis::ReplyEnvelope reply;
  reply.conn_id = 3;
  reply.io_thread_id = 1;
  reply.seq = 9;
  reply.result = ledis::CommandResult::pong();
  LEDIS_CHECK(router.tryPush(reply));

  ledis::ReplyEnvelope out;
  LEDIS_CHECK(router.tryPop(1, &out));
  LEDIS_CHECK(out.conn_id == 3);
  LEDIS_CHECK(out.seq == 9);
  LEDIS_CHECK(out.result.value.bulk.str() == "PONG");
  LEDIS_CHECK(router.empty(1));
}

}  // namespace

int main() {
  test_inbound_mpsc();
  test_reply_router_spsc();
  std::printf("test_command_queue: OK\n");
  return 0;
}
