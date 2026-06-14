#include "ledis/session/db_worker.h"

#include "ledis/session/command_envelope.h"
#include "ledis/session/ledis_engine.h"

#include <chrono>
#include <thread>

namespace ledis {
namespace {

int64_t nowMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
      .count();
}

}  // namespace

DbWorker::DbWorker(LedisEngine* engine) : engine_(engine) {}

DbWorker::~DbWorker() { stop(); }

void DbWorker::start() {
  if (running_) {
    return;
  }
  running_ = true;
  thread_ = std::thread([this] { runLoop(); });
}

void DbWorker::stop() {
  running_ = false;
  if (thread_.joinable()) {
    thread_.join();
  }
}

void DbWorker::runActiveExpire() {
  if (!engine_->activeExpireEnabled()) {
    return;
  }
  const int64_t now = nowMs();
  const size_t limit = engine_->activeExpireCycleKeys();
  for (size_t i = 0; i < engine_->db().dbCount(); ++i) {
    engine_->db().keyspace(i).activeExpireCycle(limit, now);
  }
}

void DbWorker::runLoop() {
  auto last_expire = std::chrono::steady_clock::now();
  while (running_.load(std::memory_order_relaxed)) {
    CommandEnvelope env;
    if (!engine_->inbound().tryPop(&env)) {
      const auto now = std::chrono::steady_clock::now();
      if (now - last_expire >= std::chrono::milliseconds(100)) {
        runActiveExpire();
        (void)engine_->blockingLists().expireTimeouts(nowMs());
        last_expire = now;
      }
      std::this_thread::sleep_for(std::chrono::microseconds(50));
      continue;
    }

    CommandResult result = engine_->dispatchSync(env.ctx, env.cmd);

    if (result.blocked) {
      continue;
    }

    ReplyEnvelope reply;
    reply.conn_id = env.conn_id;
    reply.io_thread_id = env.io_thread_id;
    reply.seq = env.seq;
    reply.ctx = env.ctx;
    reply.result = std::move(result);

    while (running_.load(std::memory_order_relaxed) &&
           !engine_->replyRouter().tryPush(std::move(reply))) {
      std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
  }
}

}  // namespace ledis
