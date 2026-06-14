#pragma once

#include "ledis/command/command.h"
#include "ledis/command/command_result.h"
#include "ledis/command/registry.h"
#include "ledis/session/session_context.h"
#include "ledis/store/db_manager.h"
#include "ledis/types.h"

#include <cstdint>
#include <mutex>

namespace ledis {

class ReplyRouter;

enum class RpoplpushStatus { kOk, kEmpty, kWrongType };

/** BLPOP / BRPOP / RPOPLPUSH / BRPOPLPUSH 阻塞等待与唤醒。 */
class BlockingListHub {
 public:
  struct WaitRequest {
    uint64_t conn_id = 0;
    uint32_t io_thread_id = 0;
    uint64_t seq = 0;
    SessionContext ctx;
    StdVector<Sds> keys;
    int64_t deadline_ms = 0;
    bool pop_left = true;
  };

  struct PushWaitRequest {
    uint64_t conn_id = 0;
    uint32_t io_thread_id = 0;
    uint64_t seq = 0;
    SessionContext ctx;
    Sds source;
    Sds dest;
    int64_t deadline_ms = 0;
  };

  struct PopResult {
    Sds key;
    Sds value;
  };

  void setReplyRouter(ReplyRouter* router) { router_ = router; }

  static RpoplpushStatus tryRpoplpush(DBManager& db, SessionContext& ctx,
                                      const Sds& source, const Sds& dest, Sds* out);

  /** WRONGTYPE 时 *wrong_type=true；可立即弹出时返回 true。 */
  static bool tryPop(DBManager& db, SessionContext& ctx, bool pop_left,
                     const StdVector<Sds>& keys, PopResult* out, bool* wrong_type);

  CommandResult blockingPop(SessionContext& ctx, DBManager& db, const Command& cmd,
                            bool pop_left);
  CommandResult rpoplpush(SessionContext& ctx, DBManager& db, const Command& cmd);
  CommandResult blockingRpoplpush(SessionContext& ctx, DBManager& db,
                                    const Command& cmd);

  void signalPush(DBManager& db, size_t db_index, const Sds& key);
  void disconnect(uint64_t conn_id);
  size_t expireTimeouts(int64_t now_ms);

 private:
  CommandResult makePopReply(const PopResult& pop) const;
  bool deliverPop(const WaitRequest& req, const PopResult& pop);
  bool deliverValue(const PushWaitRequest& req, const Sds& value);
  void tryWakeAll(DBManager& db);
  void tryWakePushWaiters(DBManager& db, const Sds& source_key);

  ReplyRouter* router_ = nullptr;
  std::mutex mu_;
  StdVector<WaitRequest> waiters_;
  StdVector<PushWaitRequest> push_waiters_;
};

void RegisterBlockingListCommands(CommandRegistry* registry, BlockingListHub* hub);

}  // namespace ledis
