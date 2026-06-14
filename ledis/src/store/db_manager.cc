#include "ledis/store/db_manager.h"

#include "ledis/store/eviction.h"
#include "ledis/store/memory.h"

#include <climits>

namespace ledis {

thread_local DBManager* DBManager::active_ = nullptr;
thread_local size_t DBManager::active_db_index_ = 0;

DBManager::CommandScope::CommandScope(DBManager& db, size_t db_index)
    : db_(db), prev_db_index_(active_db_index_), prev_active_(active_) {
  active_ = &db_;
  active_db_index_ = db_index;
}

DBManager::CommandScope::~CommandScope() {
  active_ = prev_active_;
  active_db_index_ = prev_db_index_;
}

Keyspace& DBManager::keyspace(size_t index) {
  return databases_[index % kDbCount];
}

const Keyspace& DBManager::keyspace(size_t index) const {
  return databases_[index % kDbCount];
}

Keyspace& DBManager::keyspaceOf(const SessionContext& ctx) {
  return keyspace(static_cast<size_t>(ctx.db_index));
}

const Keyspace& DBManager::keyspaceOf(const SessionContext& ctx) const {
  return keyspace(static_cast<size_t>(ctx.db_index));
}

bool DBManager::select(SessionContext* ctx, int index) {
  if (!ctx || index < 0 || static_cast<size_t>(index) >= kDbCount) {
    return false;
  }
  ctx->db_index = index;
  return true;
}

bool DBManager::moveKey(SessionContext* ctx, int dest_db, const Sds& key,
                        int64_t now_ms) {
  if (!ctx || dest_db < 0 || static_cast<size_t>(dest_db) >= kDbCount ||
      dest_db == ctx->db_index) {
    return false;
  }
  Keyspace& src = keyspace(static_cast<size_t>(ctx->db_index));
  LedisObject obj;
  int64_t expire_at_ms = 0;
  if (!src.getWithExpire(key, &obj, &expire_at_ms)) {
    return false;
  }
  src.del(key);
  keyspace(static_cast<size_t>(dest_db)).set(key, Move(obj), expire_at_ms);
  return true;
}

void DBManager::flushAll() {
  for (size_t i = 0; i < kDbCount; ++i) {
    databases_[i].flush();
  }
}

size_t DBManager::usedMemoryBytes(int64_t now_ms) const {
  size_t total = 0;
  for (size_t i = 0; i < kDbCount; ++i) {
    total += databases_[i].memoryBytes(now_ms);
  }
  return total;
}

void DBManager::onKeyWritten(size_t db_index, const Sds& key, int64_t now_ms) {
  (void)db_index;
  (void)key;
  evictIfNeeded(now_ms, db_index, &key);
}

void DBManager::onKeyRead(size_t db_index, const Sds& key, int64_t now_ms) {
  keyspace(db_index).touchKey(key, now_ms);
}

size_t DBManager::evictIfNeeded(int64_t now_ms, size_t db_index,
                                const Sds* protected_key) {
  if (maxmemory_ == 0) {
    return 0;
  }
  const bool volatile_only = maxmemoryPolicyVolatileOnly(maxmemory_policy_);
  const bool use_lfu = maxmemoryPolicyUsesLfu(maxmemory_policy_);
  size_t evicted = 0;
  while (usedMemoryBytes(now_ms) > maxmemory_) {
    size_t best_db = 0;
    Sds best_key;
    uint8_t best_lfu = 255;
    uint64_t best_lru = UINT64_MAX;
    bool found = false;
    for (size_t i = 0; i < kDbCount; ++i) {
      const Sds* skip = (protected_key && i == db_index) ? protected_key : nullptr;
      Sds candidate;
      uint8_t lfu = 0;
      uint64_t lru = 0;
      if (!databases_[i].oldestEvictionKey(now_ms, skip, volatile_only, use_lfu,
                                           &candidate, &lfu, &lru)) {
        continue;
      }
      if (!found) {
        found = true;
      } else if (use_lfu) {
        if (lfu > best_lfu || (lfu == best_lfu && lru >= best_lru)) {
          continue;
        }
      } else if (lru >= best_lru) {
        continue;
      }
      best_db = i;
      best_key = Move(candidate);
      best_lfu = lfu;
      best_lru = lru;
    }
    if (!found) {
      break;
    }
    databases_[best_db].del(best_key);
    ++evicted;
  }
  return evicted;
}

}  // namespace ledis
