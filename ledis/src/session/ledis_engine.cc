#include "ledis/session/ledis_engine.h"

#include "ledis/protocol/resp_writer.h"
#include "ledis/session/blocking_list.h"
#include "ledis/session/pubsub.h"
#include "ledis/store/aof.h"
#include "ledis/store/eviction.h"
#include "ledis/store/snapshot.h"

#include <chrono>
#include <cctype>
#include <cstdio>

namespace ledis {
namespace {

int64_t nowMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
      .count();
}

}  // namespace

LedisEngine::LedisEngine(const LedisSettings& settings)
    : inbound_(settings.max_pending_commands),
      replies_(settings.io_threads > 0 ? settings.io_threads : 1),
      active_expire_enabled_(settings.active_expire_enabled),
      active_expire_cycle_keys_(settings.active_expire_cycle_keys),
      dir_(settings.dir),
      dbfilename_(settings.dbfilename),
      appendfilename_(settings.appendfilename),
      aof_enabled_(settings.appendonly),
      maxclients_(settings.maxclients),
      replay_ctx_() {
  bool fsync_ok = false;
  appendfsync_ = parseAppendFsyncPolicy(settings.appendfsync, &fsync_ok);
  if (!fsync_ok) {
    appendfsync_ = AppendFsyncPolicy::kEverySec;
  }
  bool policy_ok = false;
  const MaxmemoryPolicy policy =
      parseMaxmemoryPolicy(settings.maxmemory_policy, &policy_ok);
  if (policy_ok) {
    db_.setMaxmemoryPolicy(policy);
  }
  registry_.registerDefaultCommands();
  RegisterPubSubCommands(&registry_, &pubsub_);
  RegisterBlockingListCommands(&registry_, &blocking_lists_);
  pubsub_.setReplyRouter(&replies_);
  blocking_lists_.setReplyRouter(&replies_);
  registry_.setRequirePass(settings.requirepass);
  const uint16_t port = settings.port != 0 ? settings.port : 6379;
  db_.setMaxmemory(settings.maxmemory);
  registry_.setServerConfig(port, settings.maxclients, settings.maxmemory,
                            settings.maxmemory_policy);
  registry_.setSnapshotConfig(dir_, dbfilename_);
  registry_.setAofConfig(settings.appendonly, appendfilename_,
                         appendFsyncPolicyName(appendfsync_));
  registerPersistenceCommands();
  registry_.setConfigApplyCallback(
      [this](const String& param, const String& value) { onConfigSet(param, value); });
  (void)loadSnapshot(snapshotPath(), &db_);
  initAof(settings);
}

LedisEngine::~LedisEngine() {
  joinBgaofThread();
  joinBgsaveThread();
  stopDbWorker();
}

void LedisEngine::joinBgsaveThread() {
  if (bgsave_thread_ && bgsave_thread_->joinable()) {
    bgsave_thread_->join();
  }
  bgsave_thread_.reset();
}

void LedisEngine::joinBgaofThread() {
  if (bgaof_thread_ && bgaof_thread_->joinable()) {
    bgaof_thread_->join();
  }
  bgaof_thread_.reset();
}

String LedisEngine::snapshotPath() const {
  return makeSnapshotPath(dir_, dbfilename_);
}

String LedisEngine::aofPath() const {
  return makeAofPath(dir_, appendfilename_);
}

void LedisEngine::initAof(const LedisSettings& settings) {
  if (!settings.appendonly) {
    return;
  }
  replay_ctx_.db_index = 0;
  replay_ctx_.authenticated = settings.requirepass.empty();
  aof_replaying_ = true;
  if (!loadAof(aofPath(), &db_, &registry_, &replay_ctx_)) {
    aof_replaying_ = false;
    return;
  }
  aof_replaying_ = false;
  (void)aof_writer_.open(aofPath(), false);
}

void LedisEngine::syncAofIfNeeded(int64_t now_ms) {
  switch (appendfsync_) {
    case AppendFsyncPolicy::kAlways:
      (void)aof_writer_.fsyncDisk();
      last_aof_fsync_ms_ = now_ms;
      break;
    case AppendFsyncPolicy::kEverySec:
      if (now_ms - last_aof_fsync_ms_ >= 1000) {
        (void)aof_writer_.fsyncDisk();
        last_aof_fsync_ms_ = now_ms;
      }
      break;
    case AppendFsyncPolicy::kNo:
      break;
  }
}

void LedisEngine::maybeAppendAof(const SessionContext& ctx, const Command& cmd,
                                 const CommandResult& result) {
  (void)ctx;
  if (!aof_enabled_ || aof_replaying_ || !aof_writer_.isOpen() ||
      result.value.isError() || !isAofWriteCommand(cmd)) {
    return;
  }
  const String wire = RespWriter::encodeCommand(cmd);
  if (aof_rewriting_.load(std::memory_order_relaxed)) {
    std::lock_guard<std::mutex> lock(aof_rewrite_mu_);
    aof_rewrite_buffer_.push_back(wire);
  }
  if (!aof_writer_.appendRaw(wire)) {
    return;
  }
  (void)aof_writer_.flushBuffer();
  syncAofIfNeeded(nowMs());
}

void LedisEngine::signalBlockingLists(const SessionContext& ctx, const Command& cmd,
                                      const CommandResult& result) {
  if (result.blocked || result.value.isError()) {
    return;
  }
  const String& name = cmd.name.str();
  if ((name == "LPUSH" || name == "RPUSH") && !cmd.args.empty()) {
    blocking_lists_.signalPush(db_, static_cast<size_t>(ctx.db_index), cmd.args[0]);
  }
}

bool LedisEngine::saveSnapshotSync() {
  return saveSnapshot(db_, snapshotPath(), nowMs());
}

bool LedisEngine::startBgsave() {
  bool expected = false;
  if (!bgsave_running_.compare_exchange_strong(expected, true)) {
    return false;
  }
  joinBgsaveThread();
  const int64_t now = nowMs();
  const DBManager snapshot_db = cloneDbSnapshot(db_, now);
  const String path = snapshotPath();
  bgsave_thread_.reset(new std::thread([this, snapshot_db, path, now]() {
    const bool ok = saveSnapshot(snapshot_db, path, now);
    if (!ok) {
      bgsave_running_.store(false);
      return;
    }
    bgsave_running_.store(false);
  }));
  return true;
}

bool LedisEngine::startBgRewriteAof() {
  bool expected = false;
  if (!bgaof_running_.compare_exchange_strong(expected, true)) {
    return false;
  }
  joinBgaofThread();
  aof_rewriting_.store(true, std::memory_order_relaxed);
  {
    std::lock_guard<std::mutex> lock(aof_rewrite_mu_);
    aof_rewrite_buffer_.clear();
  }

  const int64_t now = nowMs();
  const DBManager snapshot_db = cloneDbSnapshot(db_, now);
  const String final_path = aofPath();
  const String tmp_path = final_path + ".rewrite.tmp";
  bgaof_thread_.reset(new std::thread([this, snapshot_db, tmp_path, final_path, now]() {
    bool ok = rewriteAofFromDb(snapshot_db, tmp_path, now);
    if (ok) {
      StdVector<String> pending;
      {
        std::lock_guard<std::mutex> lock(aof_rewrite_mu_);
        pending.swap(aof_rewrite_buffer_);
      }
      ok = appendRawCommandsToFile(tmp_path, pending);
    }
    if (ok) {
      ok = (std::rename(tmp_path.c_str(), final_path.c_str()) == 0);
    }
    aof_rewriting_.store(false, std::memory_order_relaxed);
    if (ok) {
      aof_writer_.close();
      (void)aof_writer_.open(final_path, false);
      last_aof_fsync_ms_ = nowMs();
    }
    bgaof_running_.store(false);
  }));
  return true;
}

void LedisEngine::onConfigSet(const String& param, const String& value) {
  if (param == "maxclients") {
    maxclients_ = static_cast<size_t>(std::stoull(value));
    registry_.setServerConfig(registry_.configPort(), maxclients_, db_.maxmemory(),
                              maxmemoryPolicyName(db_.maxmemoryPolicy()));
    return;
  }
  if (param == "maxmemory") {
    db_.setMaxmemory(static_cast<size_t>(std::stoull(value)));
    registry_.setServerConfig(registry_.configPort(), maxclients_, db_.maxmemory(),
                              maxmemoryPolicyName(db_.maxmemoryPolicy()));
    return;
  }
  if (param == "maxmemory-policy" || param == "maxmemory_policy") {
    bool ok = false;
    const MaxmemoryPolicy policy = parseMaxmemoryPolicy(value, &ok);
    if (ok) {
      db_.setMaxmemoryPolicy(policy);
      registry_.setServerConfig(registry_.configPort(), maxclients_, db_.maxmemory(),
                                maxmemoryPolicyName(policy));
    }
    return;
  }
  if (param == "dir") {
    dir_ = value;
    registry_.setSnapshotConfig(dir_, dbfilename_);
    return;
  }
  if (param == "dbfilename") {
    dbfilename_ = value;
    registry_.setSnapshotConfig(dir_, dbfilename_);
    return;
  }
  if (param == "appendfilename") {
    appendfilename_ = value;
    registry_.setAofConfig(aof_enabled_, appendfilename_,
                           appendFsyncPolicyName(appendfsync_));
    if (aof_enabled_ && aof_writer_.isOpen()) {
      aof_writer_.close();
      (void)aof_writer_.open(aofPath(), false);
    }
    return;
  }
  if (param == "appendfsync") {
    bool ok = false;
    appendfsync_ = parseAppendFsyncPolicy(value, &ok);
    if (ok) {
      registry_.setAofConfig(aof_enabled_, appendfilename_,
                             appendFsyncPolicyName(appendfsync_));
    }
    return;
  }
  if (param == "appendonly") {
    String v = value;
    for (size_t i = 0; i < v.size(); ++i) {
      v[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(v[i])));
    }
    const bool enable = (v == "yes");
    if (enable == aof_enabled_) {
      return;
    }
    aof_enabled_ = enable;
    registry_.setAofConfig(aof_enabled_, appendfilename_,
                           appendFsyncPolicyName(appendfsync_));
    if (aof_enabled_) {
      (void)aof_writer_.open(aofPath(), false);
    } else {
      aof_writer_.close();
    }
  }
}

void LedisEngine::registerPersistenceCommands() {
  registry_.registerHandler("SAVE", [this](SessionContext& ctx, DBManager& db,
                                           const Command& cmd) {
    (void)ctx;
    (void)db;
    if (!cmd.args.empty()) {
      return CommandResult::error(
          "ERR wrong number of arguments for 'save' command");
    }
    if (!saveSnapshotSync()) {
      return CommandResult::error("ERR failed saving snapshot");
    }
    return CommandResult::ok();
  });
  registry_.registerHandler("BGSAVE", [this](SessionContext& ctx, DBManager& db,
                                             const Command& cmd) {
    (void)ctx;
    (void)db;
    if (!cmd.args.empty()) {
      return CommandResult::error(
          "ERR wrong number of arguments for 'bgsave' command");
    }
    if (bgsave_running_.load(std::memory_order_relaxed)) {
      return CommandResult::error("ERR Background save already in progress");
    }
    if (!startBgsave()) {
      return CommandResult::error("ERR failed starting background save");
    }
    return CommandResult::ok();
  });
  registry_.registerHandler("BGREWRITEAOF", [this](SessionContext& ctx, DBManager& db,
                                                   const Command& cmd) {
    (void)ctx;
    (void)db;
    if (!cmd.args.empty()) {
      return CommandResult::error(
          "ERR wrong number of arguments for 'bgrewriteaof' command");
    }
    if (!aof_enabled_) {
      return CommandResult::error("ERR Append only file is not enabled");
    }
    if (bgaof_running_.load(std::memory_order_relaxed)) {
      return CommandResult::error("ERR Background AOF rewrite already in progress");
    }
    if (!startBgRewriteAof()) {
      return CommandResult::error("ERR failed starting background AOF rewrite");
    }
    return CommandResult::ok();
  });
}

void LedisEngine::startDbWorker() {
  if (!db_worker_) {
    db_worker_.reset(new DbWorker(this));
  }
  db_worker_->start();
}

void LedisEngine::stopDbWorker() {
  if (db_worker_) {
    db_worker_->stop();
  }
}

bool LedisEngine::dbWorkerRunning() const {
  return db_worker_ && db_worker_->running();
}

CommandResult LedisEngine::dispatchSync(SessionContext& ctx, const Command& cmd) {
  const String& name = cmd.name.str();
  StdVector<Command> exec_batch;
  if (name == "EXEC" && ctx.in_multi) {
    exec_batch = ctx.multi_queue;
  }

  CommandResult result = registry_.dispatch(ctx, db_, cmd);

  if (name == "EXEC") {
    if (!result.value.isError() && result.value.type == RespType::kArray) {
      for (size_t i = 0; i < exec_batch.size(); ++i) {
        maybeAppendAof(ctx, exec_batch[i], CommandResult::ok());
        signalBlockingLists(ctx, exec_batch[i], CommandResult::ok());
      }
    }
  } else if (name != "MULTI" && name != "DISCARD" && !ctx.in_multi) {
    maybeAppendAof(ctx, cmd, result);
    signalBlockingLists(ctx, cmd, result);
  }
  return result;
}

}  // namespace ledis
