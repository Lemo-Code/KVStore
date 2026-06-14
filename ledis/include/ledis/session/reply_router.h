#pragma once

#include "ledis/session/command_envelope.h"
#include "ledis/session/spsc_queue.h"
#include "ledis/types.h"

#include <cstdint>

namespace ledis {

static constexpr size_t kReplyQueueCapacity = 4096;

/** DB Worker → IO Worker 的回包路由（每 IO 线程一条 SPSC）。 */
class ReplyRouter {
 public:
  explicit ReplyRouter(uint32_t io_thread_count);

  uint32_t ioThreadCount() const {
    return static_cast<uint32_t>(queues_.size());
  }

  bool tryPush(ReplyEnvelope env);
  bool tryPop(uint32_t io_thread_id, ReplyEnvelope* out);
  bool empty(uint32_t io_thread_id) const;

 private:
  using Queue = LockFreeSpscQueue<ReplyEnvelope, kReplyQueueCapacity>;
  StdVector<UniquePtr<Queue>> queues_;
};

}  // namespace ledis
