#include "ledis/session/blocking_list.h"

#include "ledis/command/registry.h"
#include "ledis/session/reply_router.h"
#include "ledis/store/object.h"

#include <chrono>
#include <cstdlib>

namespace ledis {
namespace {

int64_t nowMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

int64_t parseTimeoutSec(const Sds& s, bool* ok) {
  if (s.empty()) {
    if (ok) {
      *ok = false;
    }
    return 0;
  }
  char* end = nullptr;
  const long long v = std::strtoll(s.data(), &end, 10);
  if (end != s.data() + static_cast<ptrdiff_t>(s.size())) {
    if (ok) {
      *ok = false;
    }
    return 0;
  }
  if (ok) {
    *ok = true;
  }
  return static_cast<int64_t>(v);
}

Keyspace& keyspaceOf(DBManager& db, SessionContext& ctx) {
  return db.keyspace(static_cast<size_t>(ctx.db_index));
}

}  // namespace

bool BlockingListHub::tryPop(DBManager& db, SessionContext& ctx, bool pop_left,
                               const StdVector<Sds>& keys, PopResult* out,
                               bool* wrong_type) {
  if (wrong_type) {
    *wrong_type = false;
  }
  if (!out || keys.empty()) {
    return false;
  }
  Keyspace& ks = keyspaceOf(db, ctx);
  for (size_t i = 0; i < keys.size(); ++i) {
    LedisObject obj;
    if (!ks.get(keys[i], &obj)) {
      continue;
    }
    if (!obj.isList()) {
      if (wrong_type) {
        *wrong_type = true;
      }
      return false;
    }
    Sds value;
    const bool popped = pop_left ? obj.listPopFront(&value) : obj.listPopBack(&value);
    if (!popped) {
      continue;
    }
    if (obj.listLen() == 0) {
      ks.del(keys[i]);
    } else {
      ks.set(keys[i], Move(obj));
    }
    out->key = keys[i];
    out->value = Move(value);
    return true;
  }
  return false;
}

CommandResult BlockingListHub::makePopReply(const PopResult& pop) const {
  RespArray elems;
  elems.push_back(RespValue::makeBulk(pop.key));
  elems.push_back(RespValue::makeBulk(pop.value));
  return CommandResult::fromValue(RespValue::makeArray(Move(elems)));
}

bool BlockingListHub::deliverPop(const WaitRequest& req, const PopResult& pop) {
  if (!router_) {
    return false;
  }
  ReplyEnvelope reply;
  reply.conn_id = req.conn_id;
  reply.io_thread_id = req.io_thread_id;
  reply.seq = req.seq;
  reply.ctx = req.ctx;
  reply.result = makePopReply(pop);
  return router_->tryPush(Move(reply));
}

void BlockingListHub::tryWakeAll(DBManager& db) {
  if (waiters_.empty()) {
    return;
  }
  StdVector<WaitRequest> pending;
  pending.swap(waiters_);
  for (size_t i = 0; i < pending.size(); ++i) {
    PopResult pop;
    bool wrong_type = false;
    if (tryPop(db, pending[i].ctx, pending[i].pop_left, pending[i].keys, &pop,
               &wrong_type)) {
      (void)deliverPop(pending[i], pop);
      continue;
    }
    if (wrong_type) {
      ReplyEnvelope reply;
      reply.conn_id = pending[i].conn_id;
      reply.io_thread_id = pending[i].io_thread_id;
      reply.seq = pending[i].seq;
      reply.ctx = pending[i].ctx;
      reply.result = CommandResult::wrongType();
      (void)router_->tryPush(Move(reply));
      continue;
    }
    waiters_.push_back(Move(pending[i]));
  }
}

CommandResult BlockingListHub::blockingPop(SessionContext& ctx, DBManager& db,
                                           const Command& cmd, bool pop_left) {
  if (cmd.args.size() < 2) {
    return CommandResult::error(pop_left
                                    ? "ERR wrong number of arguments for 'blpop' command"
                                    : "ERR wrong number of arguments for 'brpop' command");
  }
  bool ok = false;
  const int64_t timeout_sec = parseTimeoutSec(cmd.args[cmd.args.size() - 1], &ok);
  if (!ok || timeout_sec < 0) {
    return CommandResult::error("ERR value is not an integer or out of range");
  }

  StdVector<Sds> keys;
  keys.reserve(cmd.args.size() - 1);
  for (size_t i = 0; i + 1 < cmd.args.size(); ++i) {
    keys.push_back(cmd.args[i]);
  }

  PopResult pop;
  bool wrong_type = false;
  if (tryPop(db, ctx, pop_left, keys, &pop, &wrong_type)) {
    return makePopReply(pop);
  }
  if (wrong_type) {
    return CommandResult::wrongType();
  }

  WaitRequest req;
  req.conn_id = ctx.conn_id;
  req.io_thread_id = ctx.io_thread_id;
  req.ctx = ctx;
  req.keys = keys;
  req.pop_left = pop_left;
  if (timeout_sec > 0) {
    req.deadline_ms = nowMs() + timeout_sec * 1000;
  }

  {
    std::lock_guard<std::mutex> lock(mu_);
    waiters_.push_back(req);
  }

  CommandResult blocked;
  blocked.blocked = true;
  return blocked;
}

void BlockingListHub::signalPush(DBManager& db, size_t db_index, const Sds& key) {
  (void)db_index;
  std::lock_guard<std::mutex> lock(mu_);
  if (!waiters_.empty()) {
    tryWakeAll(db);
  }
  if (!push_waiters_.empty()) {
    tryWakePushWaiters(db, key);
  }
}

RpoplpushStatus BlockingListHub::tryRpoplpush(DBManager& db, SessionContext& ctx,
                                              const Sds& source, const Sds& dest,
                                              Sds* out) {
  if (!out) {
    return RpoplpushStatus::kEmpty;
  }
  Keyspace& ks = keyspaceOf(db, ctx);
  LedisObject src_obj;
  if (!ks.get(source, &src_obj)) {
    return RpoplpushStatus::kEmpty;
  }
  if (!src_obj.isList()) {
    return RpoplpushStatus::kWrongType;
  }
  LedisObject dest_probe;
  if (ks.get(dest, &dest_probe) && !dest_probe.isList()) {
    return RpoplpushStatus::kWrongType;
  }
  Sds value;
  if (!src_obj.listPopBack(&value)) {
    return RpoplpushStatus::kEmpty;
  }
  *out = value;
  if (source == dest) {
    src_obj.listPushFront(Move(value));
    ks.set(source, Move(src_obj));
    return RpoplpushStatus::kOk;
  }
  if (src_obj.listLen() == 0) {
    ks.del(source);
  } else {
    ks.set(source, Move(src_obj));
  }
  LedisObject dest_obj;
  if (!ks.get(dest, &dest_obj)) {
    dest_obj = LedisObject::makeList();
  }
  dest_obj.listPushFront(Move(value));
  ks.set(dest, Move(dest_obj));
  return RpoplpushStatus::kOk;
}

bool BlockingListHub::deliverValue(const PushWaitRequest& req, const Sds& value) {
  if (!router_) {
    return false;
  }
  ReplyEnvelope reply;
  reply.conn_id = req.conn_id;
  reply.io_thread_id = req.io_thread_id;
  reply.seq = req.seq;
  reply.ctx = req.ctx;
  reply.result = CommandResult::bulk(value);
  return router_->tryPush(Move(reply));
}

void BlockingListHub::tryWakePushWaiters(DBManager& db, const Sds& source_key) {
  if (push_waiters_.empty()) {
    return;
  }
  StdVector<PushWaitRequest> pending;
  pending.swap(push_waiters_);
  for (size_t i = 0; i < pending.size(); ++i) {
    if (pending[i].source != source_key) {
      push_waiters_.push_back(Move(pending[i]));
      continue;
    }
    Sds value;
    const RpoplpushStatus st =
        tryRpoplpush(db, pending[i].ctx, pending[i].source, pending[i].dest, &value);
    if (st == RpoplpushStatus::kOk) {
      (void)deliverValue(pending[i], value);
      continue;
    }
    if (st == RpoplpushStatus::kWrongType) {
      ReplyEnvelope reply;
      reply.conn_id = pending[i].conn_id;
      reply.io_thread_id = pending[i].io_thread_id;
      reply.seq = pending[i].seq;
      reply.ctx = pending[i].ctx;
      reply.result = CommandResult::wrongType();
      (void)router_->tryPush(Move(reply));
      continue;
    }
    push_waiters_.push_back(Move(pending[i]));
  }
}

CommandResult BlockingListHub::rpoplpush(SessionContext& ctx, DBManager& db,
                                         const Command& cmd) {
  if (cmd.args.size() != 2) {
    return CommandResult::error(
        "ERR wrong number of arguments for 'rpoplpush' command");
  }
  Sds value;
  switch (tryRpoplpush(db, ctx, cmd.args[0], cmd.args[1], &value)) {
    case RpoplpushStatus::kOk:
      return CommandResult::bulk(Move(value));
    case RpoplpushStatus::kWrongType:
      return CommandResult::wrongType();
    case RpoplpushStatus::kEmpty:
    default:
      return CommandResult::nullBulk();
  }
}

CommandResult BlockingListHub::blockingRpoplpush(SessionContext& ctx, DBManager& db,
                                                 const Command& cmd) {
  if (cmd.args.size() != 3) {
    return CommandResult::error(
        "ERR wrong number of arguments for 'brpoplpush' command");
  }
  bool ok = false;
  const int64_t timeout_sec = parseTimeoutSec(cmd.args[2], &ok);
  if (!ok || timeout_sec < 0) {
    return CommandResult::error("ERR value is not an integer or out of range");
  }
  Sds value;
  switch (tryRpoplpush(db, ctx, cmd.args[0], cmd.args[1], &value)) {
    case RpoplpushStatus::kOk:
      return CommandResult::bulk(Move(value));
    case RpoplpushStatus::kWrongType:
      return CommandResult::wrongType();
    case RpoplpushStatus::kEmpty:
      break;
  }
  PushWaitRequest req;
  req.conn_id = ctx.conn_id;
  req.io_thread_id = ctx.io_thread_id;
  req.ctx = ctx;
  req.source = cmd.args[0];
  req.dest = cmd.args[1];
  if (timeout_sec > 0) {
    req.deadline_ms = nowMs() + timeout_sec * 1000;
  }
  {
    std::lock_guard<std::mutex> lock(mu_);
    push_waiters_.push_back(req);
  }
  CommandResult blocked;
  blocked.blocked = true;
  return blocked;
}

void BlockingListHub::disconnect(uint64_t conn_id) {
  std::lock_guard<std::mutex> lock(mu_);
  for (size_t i = 0; i < waiters_.size();) {
    if (waiters_[i].conn_id == conn_id) {
      waiters_.erase(waiters_.begin() + static_cast<ptrdiff_t>(i));
    } else {
      ++i;
    }
  }
  for (size_t i = 0; i < push_waiters_.size();) {
    if (push_waiters_[i].conn_id == conn_id) {
      push_waiters_.erase(push_waiters_.begin() + static_cast<ptrdiff_t>(i));
    } else {
      ++i;
    }
  }
}

size_t BlockingListHub::expireTimeouts(int64_t now_ms) {
  if (!router_) {
    return 0;
  }
  size_t expired = 0;
  std::lock_guard<std::mutex> lock(mu_);
  for (size_t i = 0; i < waiters_.size();) {
    const WaitRequest& req = waiters_[i];
    if (req.deadline_ms == 0 || now_ms < req.deadline_ms) {
      ++i;
      continue;
    }
    ReplyEnvelope reply;
    reply.conn_id = req.conn_id;
    reply.io_thread_id = req.io_thread_id;
    reply.seq = req.seq;
    reply.ctx = req.ctx;
    reply.result = CommandResult::nullBulk();
    if (router_->tryPush(Move(reply))) {
      ++expired;
    }
    waiters_.erase(waiters_.begin() + static_cast<ptrdiff_t>(i));
  }
  for (size_t i = 0; i < push_waiters_.size();) {
    const PushWaitRequest& req = push_waiters_[i];
    if (req.deadline_ms == 0 || now_ms < req.deadline_ms) {
      ++i;
      continue;
    }
    ReplyEnvelope reply;
    reply.conn_id = req.conn_id;
    reply.io_thread_id = req.io_thread_id;
    reply.seq = req.seq;
    reply.ctx = req.ctx;
    reply.result = CommandResult::nullBulk();
    if (router_->tryPush(Move(reply))) {
      ++expired;
    }
    push_waiters_.erase(push_waiters_.begin() + static_cast<ptrdiff_t>(i));
  }
  return expired;
}

void RegisterBlockingListCommands(CommandRegistry* registry, BlockingListHub* hub) {
  if (!registry || !hub) {
    return;
  }
  registry->registerHandler("BLPOP", [hub](SessionContext& ctx, DBManager& db,
                                             const Command& cmd) {
    return hub->blockingPop(ctx, db, cmd, true);
  });
  registry->registerHandler("BRPOP", [hub](SessionContext& ctx, DBManager& db,
                                             const Command& cmd) {
    return hub->blockingPop(ctx, db, cmd, false);
  });
  registry->registerHandler("RPOPLPUSH", [hub](SessionContext& ctx, DBManager& db,
                                                 const Command& cmd) {
    return hub->rpoplpush(ctx, db, cmd);
  });
  registry->registerHandler("BRPOPLPUSH", [hub](SessionContext& ctx, DBManager& db,
                                                  const Command& cmd) {
    return hub->blockingRpoplpush(ctx, db, cmd);
  });
}

}  // namespace ledis
