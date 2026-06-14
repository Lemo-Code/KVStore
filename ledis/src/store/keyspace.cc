#include "ledis/store/keyspace.h"

#include <climits>

#include "ledis/store/db_manager.h"
#include "ledis/store/memory.h"
#include "ledis/types.h"

#include <algorithm>
#include <chrono>
#include <cstdio>

namespace ledis {
namespace {

int64_t nowMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
      .count();
}

uint64_t nextLruClock() {
  static uint64_t counter = 1;
  return counter++;
}

bool keyMatchesPattern(const char* key, const char* pattern) {
  if (*pattern == '\0') {
    return *key == '\0';
  }
  if (*pattern == '*') {
    if (*(pattern + 1) == '\0') {
      return true;
    }
    for (; *key != '\0'; ++key) {
      if (keyMatchesPattern(key, pattern + 1)) {
        return true;
      }
    }
    return keyMatchesPattern(key, pattern + 1);
  }
  if (*key == '\0') {
    return false;
  }
  if (*pattern == '?' || *pattern == *key) {
    return keyMatchesPattern(key + 1, pattern + 1);
  }
  return false;
}

struct SdsLess {
  bool operator()(const Sds& lhs, const Sds& rhs) const {
    return lhs.str() < rhs.str();
  }
};

String objectWatchToken(const LedisObject& obj) {
  if (obj.isString()) {
    Sds value;
    obj.asString(&value);
    return String("s:") + value.str();
  }
  if (obj.isHash()) {
    String out = "h:";
    Vector<Sds> fields;
    const HashDict* hash = obj.asHash();
    if (hash) {
      for (HashDict::const_iterator it = hash->begin(); it != hash->end(); ++it) {
        fields.push_back(it->first);
      }
      std::sort(fields.begin(), fields.end(), SdsLess());
      for (size_t i = 0; i < fields.size(); ++i) {
        Sds value;
        obj.hashGet(fields[i], &value);
        out += fields[i].str();
        out += '=';
        out += value.str();
        out += ';';
      }
    }
    return out;
  }
  if (obj.isList()) {
    String out = "l:";
    const ListDeque* list = obj.asList();
    if (list) {
      for (size_t i = 0; i < list->size(); ++i) {
        out += (*list)[i].str();
        out += ',';
      }
    }
    return out;
  }
  if (obj.isSet()) {
    String out = "t:";
    Vector<Sds> members;
    const SdsSet* set = obj.asSet();
    if (set) {
      for (SdsSet::const_iterator it = set->begin(); it != set->end(); ++it) {
        members.push_back(*it);
      }
      std::sort(members.begin(), members.end(), SdsLess());
      for (size_t i = 0; i < members.size(); ++i) {
        out += members[i].str();
        out += ',';
      }
    }
    return out;
  }
  if (obj.isZset()) {
    String out = "z:";
    Vector<Sds> members;
    const ZsetDict* zset = obj.asZset();
    if (zset) {
      for (ZsetDict::const_iterator it = zset->begin(); it != zset->end(); ++it) {
        members.push_back(it->first);
      }
      std::sort(members.begin(), members.end(), SdsLess());
      for (size_t i = 0; i < members.size(); ++i) {
        double score = 0;
        obj.zsetScore(members[i], &score);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.17g", score);
        out += members[i].str();
        out += ':';
        out += buf;
        out += ';';
      }
    }
    return out;
  }
  return String("?");
}

}  // namespace

bool Keyspace::isExpired(const Entry& entry, int64_t now_ms) {
  return entry.expire_at_ms > 0 && now_ms >= entry.expire_at_ms;
}

void Keyspace::removeIfExpired(const Sds& key, int64_t now_ms) {
  const auto it = dict_.find(key);
  if (it != dict_.end() && isExpired(it->second, now_ms)) {
    dict_.erase(it);
  }
}

bool Keyspace::get(const Sds& key, LedisObject* out) {
  int64_t expire_at_ms = 0;
  return getWithExpire(key, out, &expire_at_ms);
}

bool Keyspace::getWithExpire(const Sds& key, LedisObject* out,
                             int64_t* expire_at_ms) {
  if (!out) {
    return false;
  }
  const int64_t now = nowMs();
  removeIfExpired(key, now);
  const auto it = dict_.find(key);
  if (it == dict_.end()) {
    return false;
  }
  *out = it->second.obj;
  if (expire_at_ms) {
    *expire_at_ms = it->second.expire_at_ms;
  }
  if (DBManager::active_) {
    DBManager::active_->onKeyRead(DBManager::active_db_index_, key, now);
  }
  return true;
}

bool Keyspace::set(const Sds& key, LedisObject obj, int64_t expire_at_ms) {
  SdsDict<Entry>::iterator it = dict_.find(key);
  if (it == dict_.end()) {
    dict_.insert(SdsDict<Entry>::value_type(key, Entry()));
    it = dict_.find(key);
  }
  it->second.obj = Move(obj);
  it->second.expire_at_ms = expire_at_ms;
  const int64_t now = nowMs();
  it->second.lru_access_ms = nextLruClock();
  it->second.lfu_counter = 5;
  if (DBManager::active_) {
    DBManager::active_->onKeyWritten(DBManager::active_db_index_, key, now);
  }
  return true;
}

size_t Keyspace::del(const Sds& key) {
  return dict_.erase(key);
}

bool Keyspace::exists(const Sds& key) {
  const int64_t now = nowMs();
  removeIfExpired(key, now);
  return dict_.find(key) != dict_.end();
}

size_t Keyspace::validSize(int64_t now_ms) const {
  size_t count = 0;
  for (SdsDict<Entry>::const_iterator it = dict_.begin(); it != dict_.end();
       ++it) {
    if (!isExpired(it->second, now_ms)) {
      ++count;
    }
  }
  return count;
}

void Keyspace::flush() {
  dict_.clear();
}

bool Keyspace::expireInSeconds(const Sds& key, int64_t ttl_seconds, int64_t now_ms) {
  if (ttl_seconds < 0) {
    return false;
  }
  removeIfExpired(key, now_ms);
  const auto it = dict_.find(key);
  if (it == dict_.end()) {
    return false;
  }
  it->second.expire_at_ms = now_ms + ttl_seconds * 1000;
  return true;
}

bool Keyspace::expireInMilliseconds(const Sds& key, int64_t ttl_ms, int64_t now_ms) {
  if (ttl_ms < 0) {
    return false;
  }
  removeIfExpired(key, now_ms);
  const auto it = dict_.find(key);
  if (it == dict_.end()) {
    return false;
  }
  it->second.expire_at_ms = now_ms + ttl_ms;
  return true;
}

bool Keyspace::expireAtSeconds(const Sds& key, int64_t unix_timestamp_sec,
                               int64_t now_ms) {
  using namespace std::chrono;
  const int64_t unix_now =
      duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
  const int64_t ttl = unix_timestamp_sec - unix_now;
  if (ttl <= 0) {
    removeIfExpired(key, now_ms);
    const auto it = dict_.find(key);
    if (it == dict_.end()) {
      return false;
    }
    dict_.erase(it);
    return true;
  }
  return expireInSeconds(key, ttl, now_ms);
}

bool Keyspace::expireAtMilliseconds(const Sds& key, int64_t unix_timestamp_ms,
                                    int64_t now_ms) {
  using namespace std::chrono;
  const int64_t unix_now =
      duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
  const int64_t ttl_ms = unix_timestamp_ms - unix_now;
  if (ttl_ms <= 0) {
    removeIfExpired(key, now_ms);
    const auto it = dict_.find(key);
    if (it == dict_.end()) {
      return false;
    }
    dict_.erase(it);
    return true;
  }
  return expireInMilliseconds(key, ttl_ms, now_ms);
}

bool Keyspace::renameKey(const Sds& key, const Sds& newkey, int64_t now_ms) {
  if (key == newkey) {
    removeIfExpired(key, now_ms);
    return dict_.find(key) != dict_.end();
  }
  removeIfExpired(key, now_ms);
  const auto it = dict_.find(key);
  if (it == dict_.end()) {
    return false;
  }
  Entry entry = Move(it->second);
  dict_.erase(it);
  dict_.erase(newkey);
  dict_.insert(SdsDict<Entry>::value_type(newkey, Move(entry)));
  return true;
}

bool Keyspace::persist(const Sds& key) {
  const auto it = dict_.find(key);
  if (it == dict_.end()) {
    return false;
  }
  it->second.expire_at_ms = 0;
  return true;
}

int64_t Keyspace::ttlSeconds(const Sds& key, int64_t now_ms) const {
  const auto it = dict_.find(key);
  if (it == dict_.end()) {
    return -2;
  }
  if (it->second.expire_at_ms == 0) {
    return -1;
  }
  if (isExpired(it->second, now_ms)) {
    return -2;
  }
  const int64_t remain_ms = it->second.expire_at_ms - now_ms;
  return remain_ms <= 0 ? -2 : remain_ms / 1000;
}

int64_t Keyspace::ttlMilliseconds(const Sds& key, int64_t now_ms) const {
  const auto it = dict_.find(key);
  if (it == dict_.end()) {
    return -2;
  }
  if (it->second.expire_at_ms == 0) {
    return -1;
  }
  if (isExpired(it->second, now_ms)) {
    return -2;
  }
  const int64_t remain_ms = it->second.expire_at_ms - now_ms;
  return remain_ms <= 0 ? -2 : remain_ms;
}

size_t Keyspace::activeExpireCycle(size_t max_keys, int64_t now_ms) {
  size_t removed = 0;
  for (SdsDict<Entry>::iterator it = dict_.begin();
       it != dict_.end() && removed < max_keys;) {
    if (isExpired(it->second, now_ms)) {
      it = dict_.erase(it);
      ++removed;
    } else {
      ++it;
    }
  }
  return removed;
}

void Keyspace::collectRecords(int64_t now_ms, Vector<KeyspaceRecord>* out) const {
  if (!out) {
    return;
  }
  for (SdsDict<Entry>::const_iterator it = dict_.begin(); it != dict_.end(); ++it) {
    if (isExpired(it->second, now_ms)) {
      continue;
    }
    KeyspaceRecord rec;
    rec.key = it->first;
    rec.obj = it->second.obj;
    rec.expire_at_ms = it->second.expire_at_ms;
    out->push_back(Move(rec));
  }
}

void Keyspace::replaceRecords(const Vector<KeyspaceRecord>& records) {
  dict_.clear();
  const int64_t now = nowMs();
  for (size_t i = 0; i < records.size(); ++i) {
    const KeyspaceRecord& rec = records[i];
    Entry entry;
    entry.obj = rec.obj;
    entry.expire_at_ms = rec.expire_at_ms;
    entry.lru_access_ms = nextLruClock();
    entry.lfu_counter = 5;
    dict_.insert(SdsDict<Entry>::value_type(rec.key, Move(entry)));
  }
}

void Keyspace::listValidKeys(int64_t now_ms, Vector<Sds>* out) const {
  if (!out) {
    return;
  }
  for (SdsDict<Entry>::const_iterator it = dict_.begin(); it != dict_.end(); ++it) {
    if (!isExpired(it->second, now_ms)) {
      out->push_back(it->first);
    }
  }
}

uint64_t Keyspace::scanKeys(int64_t now_ms, uint64_t cursor, size_t count,
                             const String& pattern, Vector<Sds>* batch) const {
  if (!batch) {
    return 0;
  }
  Vector<Sds> keys;
  listValidKeys(now_ms, &keys);
  std::sort(keys.begin(), keys.end(), SdsLess());
  if (cursor > keys.size()) {
    cursor = 0;
  }
  const size_t limit = count > 0 ? count : 10;
  size_t pos = static_cast<size_t>(cursor);
  while (pos < keys.size() && batch->size() < limit) {
    if (keyMatchesPattern(keys[pos].str().c_str(), pattern.c_str())) {
      batch->push_back(keys[pos]);
    }
    ++pos;
  }
  return pos >= keys.size() ? 0 : static_cast<uint64_t>(pos);
}

String Keyspace::watchToken(const Sds& key, int64_t now_ms) const {
  const auto it = dict_.find(key);
  if (it == dict_.end() || isExpired(it->second, now_ms)) {
    return String("nil");
  }
  return std::to_string(it->second.expire_at_ms) + "|" +
         objectWatchToken(it->second.obj);
}

bool Keyspace::watchTokenMatches(const Sds& key, int64_t now_ms,
                                 const String& token) const {
  return watchToken(key, now_ms) == token;
}

void Keyspace::touchKey(const Sds& key, int64_t now_ms) {
  const auto it = dict_.find(key);
  if (it != dict_.end() && !isExpired(it->second, now_ms)) {
    it->second.lru_access_ms = nextLruClock();
    if (it->second.lfu_counter < 255) {
      ++it->second.lfu_counter;
    }
  }
}

size_t Keyspace::memoryBytes(int64_t now_ms) const {
  size_t total = 0;
  for (SdsDict<Entry>::const_iterator it = dict_.begin(); it != dict_.end(); ++it) {
    if (isExpired(it->second, now_ms)) {
      continue;
    }
    total += estimateEntryMemory(it->first, it->second.obj);
  }
  return total;
}

bool Keyspace::oldestEvictionKey(int64_t now_ms, const Sds* skip_key,
                                 bool volatile_only, bool use_lfu, Sds* out_key,
                                 uint8_t* out_lfu, uint64_t* out_lru) const {
  if (!out_key || !out_lfu || !out_lru) {
    return false;
  }
  bool found = false;
  uint8_t best_lfu = 255;
  uint64_t best_lru = UINT64_MAX;
  for (SdsDict<Entry>::const_iterator it = dict_.begin(); it != dict_.end(); ++it) {
    if (isExpired(it->second, now_ms)) {
      continue;
    }
    if (volatile_only && it->second.expire_at_ms == 0) {
      continue;
    }
    if (skip_key && it->first == *skip_key) {
      continue;
    }
    const uint8_t lfu = it->second.lfu_counter;
    const uint64_t lru = it->second.lru_access_ms;
    if (!found) {
      found = true;
    } else if (use_lfu) {
      if (lfu > best_lfu || (lfu == best_lfu && lru >= best_lru)) {
        continue;
      }
    } else if (lru >= best_lru) {
      continue;
    }
    best_lfu = lfu;
    best_lru = lru;
    *out_key = it->first;
  }
  if (found) {
    *out_lfu = best_lfu;
    *out_lru = best_lru;
  }
  return found;
}

bool Keyspace::oldestLruKey(int64_t now_ms, const Sds* skip_key, Sds* out_key,
                            uint64_t* out_lru) const {
  uint8_t lfu = 0;
  return oldestEvictionKey(now_ms, skip_key, false, false, out_key, &lfu, out_lru);
}

}  // namespace ledis
