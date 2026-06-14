#pragma once

#include "ledis/session/session_context.h"
#include "ledis/store/eviction.h"
#include "ledis/store/keyspace.h"
#include "ledis/types.h"

namespace ledis {

class DBManager {
 public:
  static constexpr size_t kDbCount = 16;

  /** 命令执行期间激活 DB 上下文，供 Keyspace 触发 LRU / 淘汰。 */
  class CommandScope {
   public:
    CommandScope(DBManager& db, size_t db_index);
    ~CommandScope();

   private:
    DBManager& db_;
    size_t prev_db_index_;
    DBManager* prev_active_;
  };

  size_t dbCount() const { return kDbCount; }
  Keyspace& keyspace(size_t index);
  const Keyspace& keyspace(size_t index) const;
  Keyspace& keyspaceOf(const SessionContext& ctx);
  const Keyspace& keyspaceOf(const SessionContext& ctx) const;

  /** 切换 DB；index 非法时返回 false。 */
  bool select(SessionContext* ctx, int index);
  /** 将 key 从 ctx 当前 DB 移到 dest_db；不存在或 dest 非法时返回 false。 */
  bool moveKey(SessionContext* ctx, int dest_db, const Sds& key, int64_t now_ms);
  void flushAll();

  void setMaxmemory(size_t bytes) { maxmemory_ = bytes; }
  size_t maxmemory() const { return maxmemory_; }
  void setMaxmemoryPolicy(MaxmemoryPolicy policy) { maxmemory_policy_ = policy; }
  MaxmemoryPolicy maxmemoryPolicy() const { return maxmemory_policy_; }
  size_t usedMemoryBytes(int64_t now_ms) const;

  void onKeyWritten(size_t db_index, const Sds& key, int64_t now_ms);
  void onKeyRead(size_t db_index, const Sds& key, int64_t now_ms);

 private:
  friend class Keyspace;
  size_t evictIfNeeded(int64_t now_ms, size_t db_index, const Sds* protected_key);

  Array<Keyspace, kDbCount> databases_;
  size_t maxmemory_ = 0;
  MaxmemoryPolicy maxmemory_policy_ = MaxmemoryPolicy::kAllkeysLru;

  static thread_local DBManager* active_;
  static thread_local size_t active_db_index_;
};

}  // namespace ledis
