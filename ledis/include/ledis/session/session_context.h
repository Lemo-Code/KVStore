#pragma once

#include "ledis/command/command.h"

namespace ledis {

/** 单连接会话上下文（SELECT DB、AUTH、事务、Pub/Sub 等）。 */
struct SessionContext {
  int db_index = 0;
  bool authenticated = false;
  bool in_multi = false;
  uint64_t conn_id = 0;
  uint32_t io_thread_id = 0;
  StdVector<Command> multi_queue;
  StdUnorderedMap<String, String> watch_tokens;
  StdUnorderedSet<String> pubsub_channels;
  StdUnorderedSet<String> pubsub_patterns;

  bool inPubSub() const {
    return !pubsub_channels.empty() || !pubsub_patterns.empty();
  }
};

}  // namespace ledis
