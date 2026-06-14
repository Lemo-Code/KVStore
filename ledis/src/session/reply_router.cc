#include "ledis/session/reply_router.h"

namespace ledis {

ReplyRouter::ReplyRouter(uint32_t io_thread_count) {
  queues_.reserve(io_thread_count);
  for (uint32_t i = 0; i < io_thread_count; ++i) {
    queues_.push_back(UniquePtr<Queue>(new Queue()));
  }
}

bool ReplyRouter::tryPush(ReplyEnvelope env) {
  if (env.io_thread_id >= queues_.size()) {
    return false;
  }
  return queues_[env.io_thread_id]->try_push(Move(env));
}

bool ReplyRouter::tryPop(uint32_t io_thread_id, ReplyEnvelope* out) {
  if (!out || io_thread_id >= queues_.size()) {
    return false;
  }
  return queues_[io_thread_id]->try_pop(*out);
}

bool ReplyRouter::empty(uint32_t io_thread_id) const {
  if (io_thread_id >= queues_.size()) {
    return true;
  }
  return queues_[io_thread_id]->empty();
}

}  // namespace ledis
