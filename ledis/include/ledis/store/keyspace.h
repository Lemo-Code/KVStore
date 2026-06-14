#pragma once

#include "ledis/store/object.h"
#include "ledis/store/store_containers.h"

#include <cstddef>
#include <cstdint>

namespace ledis {

struct KeyspaceRecord {
  Sds key;
  LedisObject obj;
  int64_t expire_at_ms = 0;
};

class Keyspace {
 public:
  bool get(const Sds& key, LedisObject* out);
  bool set(const Sds& key, LedisObject obj, int64_t expire_at_ms = 0);
  size_t del(const Sds& key);
  bool exists(const Sds& key);
  size_t size() const { return dict_.size(); }
  size_t validSize(int64_t now_ms) const;
  void flush();

  bool expireInSeconds(const Sds& key, int64_t ttl_seconds, int64_t now_ms);
  bool expireInMilliseconds(const Sds& key, int64_t ttl_ms, int64_t now_ms);
  bool expireAtSeconds(const Sds& key, int64_t unix_timestamp_sec, int64_t now_ms);
  bool expireAtMilliseconds(const Sds& key, int64_t unix_timestamp_ms, int64_t now_ms);
  bool persist(const Sds& key);
  /** 读取 key 时同时返回 expire_at_ms（0 表示无过期）。 */
  bool getWithExpire(const Sds& key, LedisObject* out, int64_t* expire_at_ms);
  /** 将 key 重命名为 newkey；newkey 已存在则被覆盖。 */
  bool renameKey(const Sds& key, const Sds& newkey, int64_t now_ms);
  /** -2 不存在；-1 无过期；否则剩余秒数。 */
  int64_t ttlSeconds(const Sds& key, int64_t now_ms) const;
  /** -2 不存在；-1 无过期；否则剩余毫秒数。 */
  int64_t ttlMilliseconds(const Sds& key, int64_t now_ms) const;
  size_t activeExpireCycle(size_t max_keys, int64_t now_ms);

  void collectRecords(int64_t now_ms, Vector<KeyspaceRecord>* out) const;
  void replaceRecords(const Vector<KeyspaceRecord>& records);

  void listValidKeys(int64_t now_ms, Vector<Sds>* out) const;
  /** cursor 从 0 开始；返回 0 表示本轮扫描结束。 */
  uint64_t scanKeys(int64_t now_ms, uint64_t cursor, size_t count,
                     const String& pattern, Vector<Sds>* batch) const;

  /** WATCH 快照 token；key 不存在或已过期时为 "nil"。 */
  String watchToken(const Sds& key, int64_t now_ms) const;
  bool watchTokenMatches(const Sds& key, int64_t now_ms, const String& token) const;

  void touchKey(const Sds& key, int64_t now_ms);
  size_t memoryBytes(int64_t now_ms) const;
  /** 返回最适合淘汰的 key（LRU/LFU + 是否仅 volatile）。 */
  bool oldestEvictionKey(int64_t now_ms, const Sds* skip_key, bool volatile_only,
                         bool use_lfu, Sds* out_key, uint8_t* out_lfu,
                         uint64_t* out_lru) const;
  /** @deprecated 内部转调 oldestEvictionKey(allkeys-lru)。 */
  bool oldestLruKey(int64_t now_ms, const Sds* skip_key, Sds* out_key,
                    uint64_t* out_lru) const;

 private:
  struct Entry {
    LedisObject obj;
    int64_t expire_at_ms = 0;
    uint64_t lru_access_ms = 0;
    uint8_t lfu_counter = 5;
  };

  static bool isExpired(const Entry& entry, int64_t now_ms);
  void removeIfExpired(const Sds& key, int64_t now_ms);

  SdsDict<Entry> dict_;
};

}  // namespace ledis
