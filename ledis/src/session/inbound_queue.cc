#include "ledis/session/inbound_queue.h"

namespace ledis {

InboundQueue::InboundQueue(size_t max_pending) : max_pending_(max_pending) {}

bool InboundQueue::tryPush(CommandEnvelope env) {
  if (pending_.load(std::memory_order_relaxed) >= max_pending_) {
    return false;
  }
  queue_.push(std::move(env));
  pending_.fetch_add(1, std::memory_order_relaxed);
  return true;
}

bool InboundQueue::tryPop(CommandEnvelope* out) {
  if (!out) {
    return false;
  }
  if (!queue_.try_pop(*out)) {
    return false;
  }
  pending_.fetch_sub(1, std::memory_order_relaxed);
  return true;
}

}  // namespace ledis
