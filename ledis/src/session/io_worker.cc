#include "ledis/session/io_worker.h"

#include "ledis/session/ledis_engine.h"
#include "ledis/session/session.h"

namespace ledis {

IoWorker::IoWorker(uint32_t io_thread_id, LedisEngine* engine)
    : io_thread_id_(io_thread_id), engine_(engine) {}

void IoWorker::registerSession(Session* session) {
  if (session) {
    sessions_[session->connId()] = session;
  }
}

void IoWorker::unregisterSession(uint64_t conn_id) {
  engine_->pubsub().disconnect(conn_id);
  engine_->blockingLists().disconnect(conn_id);
  sessions_.erase(conn_id);
}

size_t IoWorker::pollReplies() {
  size_t count = 0;
  ReplyEnvelope reply;
  while (engine_->replyRouter().tryPop(io_thread_id_, &reply)) {
    const auto it = sessions_.find(reply.conn_id);
    if (it != sessions_.end() && it->second) {
      it->second->deliverReply(std::move(reply));
      ++count;
    }
  }
  return count;
}

}  // namespace ledis
