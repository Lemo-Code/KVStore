#pragma once

#include "ledis/command/registry.h"
#include "ledis/config/ledis_settings.h"
#include "ledis/session/db_worker.h"
#include "ledis/session/inbound_queue.h"
#include "ledis/session/blocking_list.h"
#include "ledis/session/pubsub.h"
#include "ledis/session/reply_router.h"
#include "ledis/store/aof.h"
#include "ledis/store/db_manager.h"

#include "ledis/types.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>

namespace ledis {

/**
 * @brief Ledis 运行时核心：Store + Registry + IO/DB 队列 + DB Worker。
 */
class LedisEngine {
 public:
  explicit LedisEngine(const LedisSettings& settings = LedisSettings());

  DBManager& db() { return db_; }
  const DBManager& db() const { return db_; }
  CommandRegistry& registry() { return registry_; }
  const CommandRegistry& registry() const { return registry_; }
  InboundQueue& inbound() { return inbound_; }
  const InboundQueue& inbound() const { return inbound_; }
  ReplyRouter& replyRouter() { return replies_; }
  const ReplyRouter& replyRouter() const { return replies_; }
  uint32_t ioThreadCount() const { return replies_.ioThreadCount(); }

  bool activeExpireEnabled() const { return active_expire_enabled_; }
  size_t activeExpireCycleKeys() const { return active_expire_cycle_keys_; }

  void startDbWorker();
  void stopDbWorker();
  bool dbWorkerRunning() const;

  ~LedisEngine();

  CommandResult dispatchSync(SessionContext& ctx, const Command& cmd);

  String snapshotPath() const;
  String aofPath() const;
  bool saveSnapshotSync();
  bool startBgsave();
  bool startBgRewriteAof();

  size_t maxclients() const { return maxclients_; }
  PubSubHub& pubsub() { return pubsub_; }
  const PubSubHub& pubsub() const { return pubsub_; }
  BlockingListHub& blockingLists() { return blocking_lists_; }
  const BlockingListHub& blockingLists() const { return blocking_lists_; }

 private:
  friend class DbWorker;

  void onConfigSet(const String& param, const String& value);
  void registerPersistenceCommands();
  void joinBgsaveThread();
  void joinBgaofThread();
  void initAof(const LedisSettings& settings);
  void maybeAppendAof(const SessionContext& ctx, const Command& cmd,
                      const CommandResult& result);
  void syncAofIfNeeded(int64_t now_ms);
  void signalBlockingLists(const SessionContext& ctx, const Command& cmd,
                           const CommandResult& result);

  String dir_;
  String dbfilename_;
  String appendfilename_;
  bool aof_enabled_ = false;
  bool aof_replaying_ = false;
  AppendFsyncPolicy appendfsync_ = AppendFsyncPolicy::kEverySec;
  int64_t last_aof_fsync_ms_ = 0;
  AofWriter aof_writer_;
  SessionContext replay_ctx_;
  std::atomic<bool> aof_rewriting_{false};
  std::mutex aof_rewrite_mu_;
  StdVector<String> aof_rewrite_buffer_;
  std::atomic<bool> bgsave_running_{false};
  UniquePtr<std::thread> bgsave_thread_;
  std::atomic<bool> bgaof_running_{false};
  UniquePtr<std::thread> bgaof_thread_;

  DBManager db_;
  PubSubHub pubsub_;
  BlockingListHub blocking_lists_;
  CommandRegistry registry_;
  InboundQueue inbound_;
  ReplyRouter replies_;
  bool active_expire_enabled_ = true;
  size_t active_expire_cycle_keys_ = 20;
  size_t maxclients_ = 10000;
  UniquePtr<DbWorker> db_worker_;
};

}  // namespace ledis
