#pragma once

#include "ledis/session/command_envelope.h"
#include "ledis/session/mpsc_queue.h"

#include <atomic>
#include <cstddef>

namespace ledis {

/** IO Worker → DB Worker 的入站命令队列（MPSC + 背压计数）。 */
class InboundQueue {
 public:
  explicit InboundQueue(size_t max_pending = 1024 * 64);

  bool tryPush(CommandEnvelope env);
  bool tryPop(CommandEnvelope* out);
  size_t pending() const { return pending_.load(std::memory_order_relaxed); }
  size_t maxPending() const { return max_pending_; }
  bool empty() const { return queue_.empty(); }

 private:
  size_t max_pending_;
  std::atomic<size_t> pending_{0};
  LockFreeMpscQueue<CommandEnvelope> queue_;
};

}  // namespace ledis
