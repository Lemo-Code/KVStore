#pragma once

#include "ledis/types.h"

#include <cstdint>

namespace ledis {

class LedisEngine;
class Session;

/** IO 线程上下文：从 ReplyRouter 取包并分发给各 Session。 */
class IoWorker {
 public:
  IoWorker(uint32_t io_thread_id, LedisEngine* engine);

  uint32_t ioThreadId() const { return io_thread_id_; }

  void registerSession(Session* session);
  void unregisterSession(uint64_t conn_id);

  /** 消费本 IO 线程 Outbound 队列，按 conn_id 投递。 */
  size_t pollReplies();

 private:
  uint32_t io_thread_id_;
  LedisEngine* engine_;
  StdUnorderedMap<uint64_t, Session*> sessions_;
};

}  // namespace ledis
