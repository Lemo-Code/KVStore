#include "ledis/command/registry.h"

#include "ledis/store/aof.h"
#include "ledis/store/eviction.h"
#include "ledis/store/object.h"
#include "ledis/types.h"

#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace ledis {
namespace {

int64_t parseInt64Arg(const Sds& s, bool* ok) {
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

double parseDoubleArg(const Sds& s, bool* ok) {
  if (s.empty()) {
    if (ok) {
      *ok = false;
    }
    return 0;
  }
  char* end = nullptr;
  const double v = std::strtod(s.data(), &end);
  if (end != s.data() + static_cast<ptrdiff_t>(s.size())) {
    if (ok) {
      *ok = false;
    }
    return 0;
  }
  if (ok) {
    *ok = true;
  }
  return v;
}

String formatScore(double score) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.17g", score);
  return String(buf);
}

bool parseZsetScoreBound(const Sds& raw, double* score, bool* exclusive) {
  if (!score || !exclusive) {
    return false;
  }
  String s = raw.str();
  *exclusive = false;
  if (s == "-inf") {
    *score = -INFINITY;
    return true;
  }
  if (s == "+inf") {
    *score = INFINITY;
    return true;
  }
  if (!s.empty() && s[0] == '(') {
    *exclusive = true;
    s = s.substr(1);
  }
  bool ok = false;
  *score = parseDoubleArg(Sds(s), &ok);
  return ok;
}

CommandResult zsetMembersReply(const Vector<Sds>& members,
                               const Vector<double>* scores) {
  RespArray elems;
  elems.reserve(scores ? members.size() * 2 : members.size());
  for (size_t i = 0; i < members.size(); ++i) {
    elems.push_back(RespValue::makeBulk(members[i]));
    if (scores) {
      elems.push_back(RespValue::makeBulk(Sds(formatScore((*scores)[i]))));
    }
  }
  return CommandResult::fromValue(RespValue::makeArray(Move(elems)));
}

Keyspace& ks(SessionContext& ctx, DBManager& db) {
  return db.keyspaceOf(ctx);
}

int64_t nowMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
      .count();
}

String toUpperAscii(String s) {
  for (size_t i = 0; i < s.size(); ++i) {
    s[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[i])));
  }
  return s;
}

const char* objectTypeName(const LedisObject& obj) {
  if (obj.isString()) {
    return "string";
  }
  if (obj.isHash()) {
    return "hash";
  }
  if (obj.isList()) {
    return "list";
  }
  if (obj.isSet()) {
    return "set";
  }
  if (obj.isZset()) {
    return "zset";
  }
  return "none";
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

CommandResult handlePing(SessionContext&, DBManager&, const Command&) {
  return CommandResult::pong();
}

CommandResult handleEcho(SessionContext&, DBManager&, const Command& cmd) {
  if (cmd.args.size() != 1) {
    return CommandResult::error("ERR wrong number of arguments for 'echo' command");
  }
  return CommandResult::bulk(cmd.args[0]);
}

CommandResult handleRandomkey(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (!cmd.args.empty()) {
    return CommandResult::error("ERR wrong number of arguments for 'randomkey' command");
  }
  const int64_t now = nowMs();
  Vector<Sds> keys;
  ks(ctx, db).listValidKeys(now, &keys);
  if (keys.empty()) {
    return CommandResult::nullBulk();
  }
  const size_t idx = static_cast<size_t>(now % static_cast<int64_t>(keys.size()));
  return CommandResult::bulk(keys[idx]);
}

CommandResult handleSelect(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 1) {
    return CommandResult::error("ERR wrong number of arguments for 'select' command");
  }
  bool ok = false;
  const int64_t idx = parseInt64Arg(cmd.args[0], &ok);
  if (!ok || !db.select(&ctx, static_cast<int>(idx))) {
    return CommandResult::error("ERR DB index is out of range");
  }
  return CommandResult::ok();
}

CommandResult handleGet(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 1) {
    return CommandResult::error("ERR wrong number of arguments for 'get' command");
  }
  LedisObject obj;
  if (!ks(ctx, db).get(cmd.args[0], &obj)) {
    return CommandResult::nullBulk();
  }
  Sds value;
  if (!obj.asString(&value)) {
    return CommandResult::wrongType();
  }
  return CommandResult::bulk(Move(value));
}

CommandResult handleSet(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() < 2) {
    return CommandResult::error("ERR wrong number of arguments for 'set' command");
  }
  const Sds& key = cmd.args[0];
  const Sds& value = cmd.args[1];
  bool nx = false;
  bool xx = false;
  int64_t expire_at_ms = 0;
  for (size_t i = 2; i < cmd.args.size(); ++i) {
    const String mod = toUpperAscii(cmd.args[i].str());
    if (mod == "NX") {
      if (xx) {
        return CommandResult::error("ERR syntax error");
      }
      nx = true;
      continue;
    }
    if (mod == "XX") {
      if (nx) {
        return CommandResult::error("ERR syntax error");
      }
      xx = true;
      continue;
    }
    if (mod == "EX") {
      if (i + 1 >= cmd.args.size()) {
        return CommandResult::error("ERR syntax error");
      }
      bool ok = false;
      const int64_t sec = parseInt64Arg(cmd.args[++i], &ok);
      if (!ok || sec < 0) {
        return CommandResult::error("ERR invalid expire time in 'set' command");
      }
      expire_at_ms = nowMs() + sec * 1000;
      continue;
    }
    if (mod == "PX") {
      if (i + 1 >= cmd.args.size()) {
        return CommandResult::error("ERR syntax error");
      }
      bool ok = false;
      const int64_t ms = parseInt64Arg(cmd.args[++i], &ok);
      if (!ok || ms < 0) {
        return CommandResult::error("ERR invalid expire time in 'set' command");
      }
      expire_at_ms = nowMs() + ms;
      continue;
    }
    return CommandResult::error("ERR syntax error");
  }

  LedisObject existing;
  const bool exists = ks(ctx, db).get(key, &existing);
  if (exists) {
    if (!existing.isString()) {
      return CommandResult::wrongType();
    }
    if (nx) {
      return CommandResult::nullBulk();
    }
  } else if (xx) {
    return CommandResult::nullBulk();
  }

  ks(ctx, db).set(key, LedisObject::makeString(value), expire_at_ms);
  return CommandResult::ok();
}

CommandResult handleDel(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.empty()) {
    return CommandResult::error("ERR wrong number of arguments for 'del' command");
  }
  size_t n = 0;
  for (const Sds& key : cmd.args) {
    n += ks(ctx, db).del(key);
  }
  return CommandResult::integer(static_cast<int64_t>(n));
}

CommandResult handleExists(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.empty()) {
    return CommandResult::error("ERR wrong number of arguments for 'exists' command");
  }
  int64_t n = 0;
  for (size_t i = 0; i < cmd.args.size(); ++i) {
    if (ks(ctx, db).exists(cmd.args[i])) {
      ++n;
    }
  }
  return CommandResult::integer(n);
}

CommandResult handleDbsize(SessionContext& ctx, DBManager& db, const Command&) {
  const int64_t now =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count();
  return CommandResult::integer(
      static_cast<int64_t>(ks(ctx, db).validSize(now)));
}

CommandResult handleFlushdb(SessionContext& ctx, DBManager& db, const Command&) {
  ks(ctx, db).flush();
  return CommandResult::ok();
}

CommandResult handleFlushall(SessionContext&, DBManager& db, const Command&) {
  db.flushAll();
  return CommandResult::ok();
}

CommandResult handleType(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 1) {
    return CommandResult::error("ERR wrong number of arguments for 'type' command");
  }
  LedisObject obj;
  if (!ks(ctx, db).get(cmd.args[0], &obj)) {
    return CommandResult::bulk(Sds("none"));
  }
  return CommandResult::bulk(Sds(objectTypeName(obj)));
}

CommandResult handleKeys(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 1) {
    return CommandResult::error("ERR wrong number of arguments for 'keys' command");
  }
  const String& pattern = cmd.args[0].str();
  const int64_t now = nowMs();
  Vector<Sds> keys;
  ks(ctx, db).listValidKeys(now, &keys);
  RespArray elems;
  for (size_t i = 0; i < keys.size(); ++i) {
    if (keyMatchesPattern(keys[i].str().c_str(), pattern.c_str())) {
      elems.push_back(RespValue::makeBulk(keys[i]));
    }
  }
  return CommandResult::fromValue(RespValue::makeArray(Move(elems)));
}

CommandResult handleScan(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.empty()) {
    return CommandResult::error("ERR wrong number of arguments for 'scan' command");
  }
  bool ok = false;
  const int64_t cursor_val = parseInt64Arg(cmd.args[0], &ok);
  if (!ok || cursor_val < 0) {
    return CommandResult::error("ERR invalid cursor");
  }
  String pattern = "*";
  size_t count = 10;
  for (size_t i = 1; i < cmd.args.size(); ++i) {
    const String opt = toUpperAscii(cmd.args[i].str());
    if (opt == "MATCH") {
      if (i + 1 >= cmd.args.size()) {
        return CommandResult::error("ERR syntax error");
      }
      pattern = cmd.args[++i].str();
      continue;
    }
    if (opt == "COUNT") {
      if (i + 1 >= cmd.args.size()) {
        return CommandResult::error("ERR syntax error");
      }
      bool count_ok = false;
      const int64_t n = parseInt64Arg(cmd.args[++i], &count_ok);
      if (!count_ok || n < 1) {
        return CommandResult::error("ERR value is not an integer or out of range");
      }
      count = static_cast<size_t>(n);
      continue;
    }
    return CommandResult::error("ERR syntax error");
  }

  const int64_t now = nowMs();
  Vector<Sds> batch;
  const uint64_t next =
      ks(ctx, db).scanKeys(now, static_cast<uint64_t>(cursor_val), count,
                           pattern, &batch);
  RespArray key_elems;
  for (size_t i = 0; i < batch.size(); ++i) {
    key_elems.push_back(RespValue::makeBulk(batch[i]));
  }
  RespArray reply;
  reply.push_back(RespValue::makeBulk(Sds(std::to_string(next))));
  reply.push_back(RespValue::makeArray(Move(key_elems)));
  return CommandResult::fromValue(RespValue::makeArray(Move(reply)));
}

CommandResult handleExpire(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 2) {
    return CommandResult::error("ERR wrong number of arguments for 'expire' command");
  }
  bool ok = false;
  const int64_t sec = parseInt64Arg(cmd.args[1], &ok);
  if (!ok) {
    return CommandResult::error("ERR value is not an integer or out of range");
  }
  const int64_t now =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count();
  const int64_t n = ks(ctx, db).expireInSeconds(cmd.args[0], sec, now) ? 1 : 0;
  return CommandResult::integer(n);
}

CommandResult handleTtl(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 1) {
    return CommandResult::error("ERR wrong number of arguments for 'ttl' command");
  }
  const int64_t now =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count();
  return CommandResult::integer(ks(ctx, db).ttlSeconds(cmd.args[0], now));
}

CommandResult handlePttl(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 1) {
    return CommandResult::error("ERR wrong number of arguments for 'pttl' command");
  }
  const int64_t now = nowMs();
  return CommandResult::integer(ks(ctx, db).ttlMilliseconds(cmd.args[0], now));
}

CommandResult handlePexpire(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 2) {
    return CommandResult::error("ERR wrong number of arguments for 'pexpire' command");
  }
  bool ok = false;
  const int64_t ms = parseInt64Arg(cmd.args[1], &ok);
  if (!ok) {
    return CommandResult::error("ERR value is not an integer or out of range");
  }
  const int64_t now = nowMs();
  const int64_t n = ks(ctx, db).expireInMilliseconds(cmd.args[0], ms, now) ? 1 : 0;
  return CommandResult::integer(n);
}

CommandResult handlePersist(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 1) {
    return CommandResult::error("ERR wrong number of arguments for 'persist' command");
  }
  const int64_t n = ks(ctx, db).persist(cmd.args[0]) ? 1 : 0;
  return CommandResult::integer(n);
}

CommandResult handleGetset(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 2) {
    return CommandResult::error("ERR wrong number of arguments for 'getset' command");
  }
  Keyspace& keyspace = ks(ctx, db);
  const int64_t now = nowMs();
  LedisObject obj;
  int64_t expire_at_ms = 0;
  const bool existed = keyspace.getWithExpire(cmd.args[0], &obj, &expire_at_ms);
  if (existed && !obj.isString()) {
    return CommandResult::wrongType();
  }
  Sds old_value;
  if (existed) {
    obj.asString(&old_value);
  }
  keyspace.set(cmd.args[0], LedisObject::makeString(cmd.args[1]), expire_at_ms);
  if (!existed) {
    return CommandResult::nullBulk();
  }
  return CommandResult::bulk(Move(old_value));
}

CommandResult handleAppend(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 2) {
    return CommandResult::error("ERR wrong number of arguments for 'append' command");
  }
  Keyspace& keyspace = ks(ctx, db);
  LedisObject obj;
  int64_t expire_at_ms = 0;
  const bool existed = keyspace.getWithExpire(cmd.args[0], &obj, &expire_at_ms);
  Sds value = cmd.args[1];
  if (existed) {
    if (!obj.isString()) {
      return CommandResult::wrongType();
    }
    Sds current;
    obj.asString(&current);
    value = Sds(current.str() + cmd.args[1].str());
  }
  keyspace.set(cmd.args[0], LedisObject::makeString(value), expire_at_ms);
  return CommandResult::integer(static_cast<int64_t>(value.size()));
}

CommandResult handleStrlen(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 1) {
    return CommandResult::error("ERR wrong number of arguments for 'strlen' command");
  }
  LedisObject obj;
  if (!ks(ctx, db).get(cmd.args[0], &obj)) {
    return CommandResult::integer(0);
  }
  if (!obj.isString()) {
    return CommandResult::wrongType();
  }
  Sds value;
  obj.asString(&value);
  return CommandResult::integer(static_cast<int64_t>(value.size()));
}

CommandResult handleSetex(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 3) {
    return CommandResult::error("ERR wrong number of arguments for 'setex' command");
  }
  bool ok = false;
  const int64_t sec = parseInt64Arg(cmd.args[1], &ok);
  if (!ok || sec <= 0) {
    return CommandResult::error("ERR invalid expire time in 'setex' command");
  }
  LedisObject existing;
  if (ks(ctx, db).get(cmd.args[0], &existing) && !existing.isString()) {
    return CommandResult::wrongType();
  }
  const int64_t expire_at_ms = nowMs() + sec * 1000;
  ks(ctx, db).set(cmd.args[0], LedisObject::makeString(cmd.args[2]), expire_at_ms);
  return CommandResult::ok();
}

CommandResult handlePsetex(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 3) {
    return CommandResult::error("ERR wrong number of arguments for 'psetex' command");
  }
  bool ok = false;
  const int64_t ms = parseInt64Arg(cmd.args[1], &ok);
  if (!ok || ms <= 0) {
    return CommandResult::error("ERR invalid expire time in 'psetex' command");
  }
  LedisObject existing;
  if (ks(ctx, db).get(cmd.args[0], &existing) && !existing.isString()) {
    return CommandResult::wrongType();
  }
  const int64_t expire_at_ms = nowMs() + ms;
  ks(ctx, db).set(cmd.args[0], LedisObject::makeString(cmd.args[2]), expire_at_ms);
  return CommandResult::ok();
}

CommandResult handleRename(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 2) {
    return CommandResult::error("ERR wrong number of arguments for 'rename' command");
  }
  const int64_t now = nowMs();
  if (!ks(ctx, db).renameKey(cmd.args[0], cmd.args[1], now)) {
    return CommandResult::error("ERR no such key");
  }
  return CommandResult::ok();
}

CommandResult handleRenamenx(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 2) {
    return CommandResult::error("ERR wrong number of arguments for 'renamenx' command");
  }
  const Sds& key = cmd.args[0];
  const Sds& newkey = cmd.args[1];
  const int64_t now = nowMs();
  if (key != newkey && ks(ctx, db).exists(newkey)) {
    return CommandResult::integer(0);
  }
  if (!ks(ctx, db).renameKey(key, newkey, now)) {
    return CommandResult::error("ERR no such key");
  }
  return CommandResult::integer(1);
}

CommandResult handleMove(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 2) {
    return CommandResult::error("ERR wrong number of arguments for 'move' command");
  }
  bool ok = false;
  const int64_t dest = parseInt64Arg(cmd.args[1], &ok);
  if (!ok) {
    return CommandResult::error("ERR value is not an integer or out of range");
  }
  const int64_t n =
      db.moveKey(&ctx, static_cast<int>(dest), cmd.args[0], nowMs()) ? 1 : 0;
  return CommandResult::integer(n);
}

CommandResult handleTouch(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.empty()) {
    return CommandResult::error("ERR wrong number of arguments for 'touch' command");
  }
  Keyspace& keyspace = ks(ctx, db);
  const int64_t now = nowMs();
  int64_t touched = 0;
  for (size_t i = 0; i < cmd.args.size(); ++i) {
    if (keyspace.exists(cmd.args[i])) {
      keyspace.touchKey(cmd.args[i], now);
      ++touched;
    }
  }
  return CommandResult::integer(touched);
}

CommandResult handleExpireat(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 2) {
    return CommandResult::error("ERR wrong number of arguments for 'expireat' command");
  }
  bool ok = false;
  const int64_t ts = parseInt64Arg(cmd.args[1], &ok);
  if (!ok) {
    return CommandResult::error("ERR value is not an integer or out of range");
  }
  const int64_t now = nowMs();
  const int64_t n =
      ks(ctx, db).expireAtSeconds(cmd.args[0], ts, now) ? 1 : 0;
  return CommandResult::integer(n);
}

CommandResult handlePexpireat(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 2) {
    return CommandResult::error("ERR wrong number of arguments for 'pexpireat' command");
  }
  bool ok = false;
  const int64_t ts = parseInt64Arg(cmd.args[1], &ok);
  if (!ok) {
    return CommandResult::error("ERR value is not an integer or out of range");
  }
  const int64_t now = nowMs();
  const int64_t n =
      ks(ctx, db).expireAtMilliseconds(cmd.args[0], ts, now) ? 1 : 0;
  return CommandResult::integer(n);
}

CommandResult handleMget(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.empty()) {
    return CommandResult::error("ERR wrong number of arguments for 'mget' command");
  }
  RespArray elems;
  elems.reserve(cmd.args.size());
  for (const Sds& key : cmd.args) {
    LedisObject obj;
    if (!ks(ctx, db).get(key, &obj)) {
      elems.push_back(RespValue::makeNull());
      continue;
    }
    Sds value;
    if (!obj.asString(&value)) {
      return CommandResult::wrongType();
    }
    elems.push_back(RespValue::makeBulk(Move(value)));
  }
  return CommandResult::fromValue(RespValue::makeArray(Move(elems)));
}

CommandResult handleMset(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.empty() || cmd.args.size() % 2 != 0) {
    return CommandResult::error("ERR wrong number of arguments for 'mset' command");
  }
  for (size_t i = 0; i + 1 < cmd.args.size(); i += 2) {
    ks(ctx, db).set(cmd.args[i], LedisObject::makeString(cmd.args[i + 1]));
  }
  return CommandResult::ok();
}

CommandResult incrBy(SessionContext& ctx, DBManager& db, const Sds& key,
                     int64_t delta) {
  LedisObject obj;
  int64_t current = 0;
  if (ks(ctx, db).get(key, &obj)) {
    Sds s;
    if (!obj.asString(&s)) {
      return CommandResult::error("ERR value is not an integer or out of range");
    }
    bool ok = false;
    current = parseInt64Arg(s, &ok);
    if (!ok) {
      return CommandResult::error("ERR value is not an integer or out of range");
    }
  }
  const int64_t next = current + delta;
  ks(ctx, db).set(key, LedisObject::makeString(Sds(std::to_string(next))));
  return CommandResult::integer(next);
}

CommandResult handleIncr(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 1) {
    return CommandResult::error("ERR wrong number of arguments for 'incr' command");
  }
  return incrBy(ctx, db, cmd.args[0], 1);
}

CommandResult handleDecr(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 1) {
    return CommandResult::error("ERR wrong number of arguments for 'decr' command");
  }
  return incrBy(ctx, db, cmd.args[0], -1);
}

CommandResult handleIncrby(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 2) {
    return CommandResult::error("ERR wrong number of arguments for 'incrby' command");
  }
  bool ok = false;
  const int64_t delta = parseInt64Arg(cmd.args[1], &ok);
  if (!ok) {
    return CommandResult::error("ERR value is not an integer or out of range");
  }
  return incrBy(ctx, db, cmd.args[0], delta);
}

CommandResult handleDecrby(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 2) {
    return CommandResult::error("ERR wrong number of arguments for 'decrby' command");
  }
  bool ok = false;
  const int64_t delta = parseInt64Arg(cmd.args[1], &ok);
  if (!ok) {
    return CommandResult::error("ERR value is not an integer or out of range");
  }
  return incrBy(ctx, db, cmd.args[0], -delta);
}

CommandResult handleHset(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 3) {
    return CommandResult::error("ERR wrong number of arguments for 'hset' command");
  }
  Keyspace& keyspace = ks(ctx, db);
  LedisObject obj;
  if (!keyspace.get(cmd.args[0], &obj)) {
    obj = LedisObject::makeHash();
  } else if (!obj.isHash()) {
    return CommandResult::wrongType();
  }
  const bool added = obj.hashSet(cmd.args[1], cmd.args[2]);
  keyspace.set(cmd.args[0], Move(obj));
  return CommandResult::integer(added ? 1 : 0);
}

CommandResult handleHget(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 2) {
    return CommandResult::error("ERR wrong number of arguments for 'hget' command");
  }
  LedisObject obj;
  if (!ks(ctx, db).get(cmd.args[0], &obj)) {
    return CommandResult::nullBulk();
  }
  if (!obj.isHash()) {
    return CommandResult::wrongType();
  }
  Sds value;
  if (!obj.hashGet(cmd.args[1], &value)) {
    return CommandResult::nullBulk();
  }
  return CommandResult::bulk(Move(value));
}

CommandResult handleHdel(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() < 2) {
    return CommandResult::error("ERR wrong number of arguments for 'hdel' command");
  }
  Keyspace& keyspace = ks(ctx, db);
  LedisObject obj;
  if (!keyspace.get(cmd.args[0], &obj)) {
    return CommandResult::integer(0);
  }
  if (!obj.isHash()) {
    return CommandResult::wrongType();
  }
  size_t n = 0;
  for (size_t i = 1; i < cmd.args.size(); ++i) {
    n += obj.hashDel(cmd.args[i]);
  }
  if (obj.hashLen() == 0) {
    keyspace.del(cmd.args[0]);
  } else {
    keyspace.set(cmd.args[0], Move(obj));
  }
  return CommandResult::integer(static_cast<int64_t>(n));
}

CommandResult handleHgetall(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 1) {
    return CommandResult::error("ERR wrong number of arguments for 'hgetall' command");
  }
  LedisObject obj;
  if (!ks(ctx, db).get(cmd.args[0], &obj)) {
    return CommandResult::fromValue(RespValue::makeArray({}));
  }
  if (!obj.isHash()) {
    return CommandResult::wrongType();
  }
  RespArray elems;
  const HashDict* hash = obj.asHash();
  for (HashDict::const_iterator it = hash->begin(); it != hash->end(); ++it) {
    elems.push_back(RespValue::makeBulk(it->first));
    elems.push_back(RespValue::makeBulk(it->second));
  }
  return CommandResult::fromValue(RespValue::makeArray(Move(elems)));
}

CommandResult handleHlen(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 1) {
    return CommandResult::error("ERR wrong number of arguments for 'hlen' command");
  }
  LedisObject obj;
  if (!ks(ctx, db).get(cmd.args[0], &obj)) {
    return CommandResult::integer(0);
  }
  if (!obj.isHash()) {
    return CommandResult::wrongType();
  }
  return CommandResult::integer(static_cast<int64_t>(obj.hashLen()));
}

CommandResult handleHexists(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 2) {
    return CommandResult::error("ERR wrong number of arguments for 'hexists' command");
  }
  LedisObject obj;
  if (!ks(ctx, db).get(cmd.args[0], &obj)) {
    return CommandResult::integer(0);
  }
  if (!obj.isHash()) {
    return CommandResult::wrongType();
  }
  return CommandResult::integer(obj.hashExists(cmd.args[1]) ? 1 : 0);
}

CommandResult handleHmget(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() < 2) {
    return CommandResult::error("ERR wrong number of arguments for 'hmget' command");
  }
  LedisObject obj;
  if (!ks(ctx, db).get(cmd.args[0], &obj)) {
    RespArray elems;
    for (size_t i = 1; i < cmd.args.size(); ++i) {
      elems.push_back(RespValue::makeNull());
    }
    return CommandResult::fromValue(RespValue::makeArray(Move(elems)));
  }
  if (!obj.isHash()) {
    return CommandResult::wrongType();
  }
  RespArray elems;
  for (size_t i = 1; i < cmd.args.size(); ++i) {
    Sds value;
    if (obj.hashGet(cmd.args[i], &value)) {
      elems.push_back(RespValue::makeBulk(Move(value)));
    } else {
      elems.push_back(RespValue::makeNull());
    }
  }
  return CommandResult::fromValue(RespValue::makeArray(Move(elems)));
}

CommandResult handleHkeys(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 1) {
    return CommandResult::error("ERR wrong number of arguments for 'hkeys' command");
  }
  LedisObject obj;
  if (!ks(ctx, db).get(cmd.args[0], &obj)) {
    return CommandResult::fromValue(RespValue::makeArray({}));
  }
  if (!obj.isHash()) {
    return CommandResult::wrongType();
  }
  RespArray elems;
  const HashDict* hash = obj.asHash();
  for (HashDict::const_iterator it = hash->begin(); it != hash->end(); ++it) {
    elems.push_back(RespValue::makeBulk(it->first));
  }
  return CommandResult::fromValue(RespValue::makeArray(Move(elems)));
}

CommandResult handleHvals(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 1) {
    return CommandResult::error("ERR wrong number of arguments for 'hvals' command");
  }
  LedisObject obj;
  if (!ks(ctx, db).get(cmd.args[0], &obj)) {
    return CommandResult::fromValue(RespValue::makeArray({}));
  }
  if (!obj.isHash()) {
    return CommandResult::wrongType();
  }
  RespArray elems;
  const HashDict* hash = obj.asHash();
  for (HashDict::const_iterator it = hash->begin(); it != hash->end(); ++it) {
    elems.push_back(RespValue::makeBulk(it->second));
  }
  return CommandResult::fromValue(RespValue::makeArray(Move(elems)));
}

CommandResult handleHincrby(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 3) {
    return CommandResult::error("ERR wrong number of arguments for 'hincrby' command");
  }
  bool ok = false;
  const int64_t delta = parseInt64Arg(cmd.args[2], &ok);
  if (!ok) {
    return CommandResult::error("ERR value is not an integer or out of range");
  }
  Keyspace& keyspace = ks(ctx, db);
  LedisObject obj;
  if (!keyspace.get(cmd.args[0], &obj)) {
    obj = LedisObject::makeHash();
  } else if (!obj.isHash()) {
    return CommandResult::wrongType();
  }
  int64_t next = 0;
  if (!obj.hashIncrBy(cmd.args[1], delta, &next)) {
    return CommandResult::error("ERR hash value is not an integer");
  }
  keyspace.set(cmd.args[0], Move(obj));
  return CommandResult::integer(next);
}

CommandResult handleLpush(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() < 2) {
    return CommandResult::error("ERR wrong number of arguments for 'lpush' command");
  }
  Keyspace& keyspace = ks(ctx, db);
  LedisObject obj;
  if (!keyspace.get(cmd.args[0], &obj)) {
    obj = LedisObject::makeList();
  } else if (!obj.isList()) {
    return CommandResult::wrongType();
  }
  for (size_t i = 1; i < cmd.args.size(); ++i) {
    obj.listPushFront(cmd.args[i]);
  }
  const int64_t len = static_cast<int64_t>(obj.listLen());
  keyspace.set(cmd.args[0], Move(obj));
  return CommandResult::integer(len);
}

CommandResult handleRpush(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() < 2) {
    return CommandResult::error("ERR wrong number of arguments for 'rpush' command");
  }
  Keyspace& keyspace = ks(ctx, db);
  LedisObject obj;
  if (!keyspace.get(cmd.args[0], &obj)) {
    obj = LedisObject::makeList();
  } else if (!obj.isList()) {
    return CommandResult::wrongType();
  }
  for (size_t i = 1; i < cmd.args.size(); ++i) {
    obj.listPushBack(cmd.args[i]);
  }
  const int64_t len = static_cast<int64_t>(obj.listLen());
  keyspace.set(cmd.args[0], Move(obj));
  return CommandResult::integer(len);
}

CommandResult handleLpop(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 1) {
    return CommandResult::error("ERR wrong number of arguments for 'lpop' command");
  }
  Keyspace& keyspace = ks(ctx, db);
  LedisObject obj;
  if (!keyspace.get(cmd.args[0], &obj)) {
    return CommandResult::nullBulk();
  }
  if (!obj.isList()) {
    return CommandResult::wrongType();
  }
  Sds value;
  if (!obj.listPopFront(&value)) {
    return CommandResult::nullBulk();
  }
  if (obj.listLen() == 0) {
    keyspace.del(cmd.args[0]);
  } else {
    keyspace.set(cmd.args[0], Move(obj));
  }
  return CommandResult::bulk(Move(value));
}

CommandResult handleRpop(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 1) {
    return CommandResult::error("ERR wrong number of arguments for 'rpop' command");
  }
  Keyspace& keyspace = ks(ctx, db);
  LedisObject obj;
  if (!keyspace.get(cmd.args[0], &obj)) {
    return CommandResult::nullBulk();
  }
  if (!obj.isList()) {
    return CommandResult::wrongType();
  }
  Sds value;
  if (!obj.listPopBack(&value)) {
    return CommandResult::nullBulk();
  }
  if (obj.listLen() == 0) {
    keyspace.del(cmd.args[0]);
  } else {
    keyspace.set(cmd.args[0], Move(obj));
  }
  return CommandResult::bulk(Move(value));
}

CommandResult handleLlen(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 1) {
    return CommandResult::error("ERR wrong number of arguments for 'llen' command");
  }
  LedisObject obj;
  if (!ks(ctx, db).get(cmd.args[0], &obj)) {
    return CommandResult::integer(0);
  }
  if (!obj.isList()) {
    return CommandResult::wrongType();
  }
  return CommandResult::integer(static_cast<int64_t>(obj.listLen()));
}

CommandResult handleLrange(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 3) {
    return CommandResult::error("ERR wrong number of arguments for 'lrange' command");
  }
  bool ok = false;
  const int64_t start = parseInt64Arg(cmd.args[1], &ok);
  if (!ok) {
    return CommandResult::error("ERR value is not an integer or out of range");
  }
  const int64_t stop = parseInt64Arg(cmd.args[2], &ok);
  if (!ok) {
    return CommandResult::error("ERR value is not an integer or out of range");
  }
  LedisObject obj;
  if (!ks(ctx, db).get(cmd.args[0], &obj)) {
    return CommandResult::fromValue(RespValue::makeArray({}));
  }
  if (!obj.isList()) {
    return CommandResult::wrongType();
  }
  Vector<Sds> range;
  obj.listRange(start, stop, &range);
  RespArray elems;
  elems.reserve(range.size());
  for (size_t i = 0; i < range.size(); ++i) {
    elems.push_back(RespValue::makeBulk(range[i]));
  }
  return CommandResult::fromValue(RespValue::makeArray(Move(elems)));
}

CommandResult handleLindex(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 2) {
    return CommandResult::error("ERR wrong number of arguments for 'lindex' command");
  }
  bool ok = false;
  const int64_t index = parseInt64Arg(cmd.args[1], &ok);
  if (!ok) {
    return CommandResult::error("ERR value is not an integer or out of range");
  }
  LedisObject obj;
  if (!ks(ctx, db).get(cmd.args[0], &obj)) {
    return CommandResult::nullBulk();
  }
  if (!obj.isList()) {
    return CommandResult::wrongType();
  }
  Sds value;
  if (!obj.listIndex(index, &value)) {
    return CommandResult::nullBulk();
  }
  return CommandResult::bulk(Move(value));
}

CommandResult handleLtrim(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 3) {
    return CommandResult::error("ERR wrong number of arguments for 'ltrim' command");
  }
  bool ok = false;
  const int64_t start = parseInt64Arg(cmd.args[1], &ok);
  if (!ok) {
    return CommandResult::error("ERR value is not an integer or out of range");
  }
  const int64_t stop = parseInt64Arg(cmd.args[2], &ok);
  if (!ok) {
    return CommandResult::error("ERR value is not an integer or out of range");
  }
  Keyspace& keyspace = ks(ctx, db);
  LedisObject obj;
  if (!keyspace.get(cmd.args[0], &obj)) {
    return CommandResult::ok();
  }
  if (!obj.isList()) {
    return CommandResult::wrongType();
  }
  obj.listTrim(start, stop);
  if (obj.listLen() == 0) {
    keyspace.del(cmd.args[0]);
  } else {
    keyspace.set(cmd.args[0], Move(obj));
  }
  return CommandResult::ok();
}

CommandResult handleSadd(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() < 2) {
    return CommandResult::error("ERR wrong number of arguments for 'sadd' command");
  }
  Keyspace& keyspace = ks(ctx, db);
  LedisObject obj;
  if (!keyspace.get(cmd.args[0], &obj)) {
    obj = LedisObject::makeSet();
  } else if (!obj.isSet()) {
    return CommandResult::wrongType();
  }
  size_t added = 0;
  for (size_t i = 1; i < cmd.args.size(); ++i) {
    if (obj.setAdd(cmd.args[i])) {
      ++added;
    }
  }
  keyspace.set(cmd.args[0], Move(obj));
  return CommandResult::integer(static_cast<int64_t>(added));
}

CommandResult handleSrem(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() < 2) {
    return CommandResult::error("ERR wrong number of arguments for 'srem' command");
  }
  Keyspace& keyspace = ks(ctx, db);
  LedisObject obj;
  if (!keyspace.get(cmd.args[0], &obj)) {
    return CommandResult::integer(0);
  }
  if (!obj.isSet()) {
    return CommandResult::wrongType();
  }
  size_t removed = 0;
  for (size_t i = 1; i < cmd.args.size(); ++i) {
    removed += obj.setRem(cmd.args[i]);
  }
  if (obj.setLen() == 0) {
    keyspace.del(cmd.args[0]);
  } else {
    keyspace.set(cmd.args[0], Move(obj));
  }
  return CommandResult::integer(static_cast<int64_t>(removed));
}

CommandResult handleSmembers(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 1) {
    return CommandResult::error("ERR wrong number of arguments for 'smembers' command");
  }
  LedisObject obj;
  if (!ks(ctx, db).get(cmd.args[0], &obj)) {
    return CommandResult::fromValue(RespValue::makeArray({}));
  }
  if (!obj.isSet()) {
    return CommandResult::wrongType();
  }
  RespArray elems;
  const SdsSet* set = obj.asSet();
  for (SdsSet::const_iterator it = set->begin(); it != set->end(); ++it) {
    elems.push_back(RespValue::makeBulk(*it));
  }
  return CommandResult::fromValue(RespValue::makeArray(Move(elems)));
}

CommandResult handleScard(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 1) {
    return CommandResult::error("ERR wrong number of arguments for 'scard' command");
  }
  LedisObject obj;
  if (!ks(ctx, db).get(cmd.args[0], &obj)) {
    return CommandResult::integer(0);
  }
  if (!obj.isSet()) {
    return CommandResult::wrongType();
  }
  return CommandResult::integer(static_cast<int64_t>(obj.setLen()));
}

CommandResult handleSismember(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 2) {
    return CommandResult::error("ERR wrong number of arguments for 'sismember' command");
  }
  LedisObject obj;
  if (!ks(ctx, db).get(cmd.args[0], &obj)) {
    return CommandResult::integer(0);
  }
  if (!obj.isSet()) {
    return CommandResult::wrongType();
  }
  const int64_t n = obj.setIsMember(cmd.args[1]) ? 1 : 0;
  return CommandResult::integer(n);
}

CommandResult setMembersReply(const Vector<Sds>& members) {
  RespArray elems;
  elems.reserve(members.size());
  for (size_t i = 0; i < members.size(); ++i) {
    elems.push_back(RespValue::makeBulk(members[i]));
  }
  return CommandResult::fromValue(RespValue::makeArray(Move(elems)));
}

bool loadSetArgs(Keyspace& keyspace, const SdsArgList& keys, size_t begin,
                 Vector<LedisObject>* holders, Vector<const SdsSet*>* sets,
                 CommandResult* wrong_type) {
  if (!holders || !sets) {
    return false;
  }
  holders->clear();
  sets->clear();
  holders->resize(keys.size() - begin);
  sets->resize(keys.size() - begin, nullptr);
  for (size_t i = begin; i < keys.size(); ++i) {
    const size_t idx = i - begin;
    if (!keyspace.get(keys[i], &(*holders)[idx])) {
      continue;
    }
    if (!(*holders)[idx].isSet()) {
      if (wrong_type) {
        *wrong_type = CommandResult::wrongType();
      }
      return false;
    }
    (*sets)[idx] = (*holders)[idx].asSet();
  }
  return true;
}

CommandResult handleSinter(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.empty()) {
    return CommandResult::error("ERR wrong number of arguments for 'sinter' command");
  }
  Keyspace& keyspace = ks(ctx, db);
  Vector<LedisObject> holders;
  Vector<const SdsSet*> sets;
  CommandResult wrong;
  if (!loadSetArgs(keyspace, cmd.args, 0, &holders, &sets, &wrong)) {
    return wrong;
  }
  Vector<Sds> result;
  LedisObject::setIntersect(sets, &result);
  return setMembersReply(result);
}

CommandResult handleSunion(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.empty()) {
    return CommandResult::error("ERR wrong number of arguments for 'sunion' command");
  }
  Keyspace& keyspace = ks(ctx, db);
  Vector<LedisObject> holders;
  Vector<const SdsSet*> sets;
  CommandResult wrong;
  if (!loadSetArgs(keyspace, cmd.args, 0, &holders, &sets, &wrong)) {
    return wrong;
  }
  Vector<Sds> result;
  LedisObject::setUnion(sets, &result);
  return setMembersReply(result);
}

CommandResult handleSdiff(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.empty()) {
    return CommandResult::error("ERR wrong number of arguments for 'sdiff' command");
  }
  Keyspace& keyspace = ks(ctx, db);
  Vector<LedisObject> holders;
  Vector<const SdsSet*> sets;
  CommandResult wrong;
  if (!loadSetArgs(keyspace, cmd.args, 0, &holders, &sets, &wrong)) {
    return wrong;
  }
  const SdsSet* first = sets.empty() ? nullptr : sets[0];
  Vector<const SdsSet*> others;
  for (size_t i = 1; i < sets.size(); ++i) {
    others.push_back(sets[i]);
  }
  Vector<Sds> result;
  LedisObject::setDiff(first, others, &result);
  return setMembersReply(result);
}

CommandResult storeSetResult(Keyspace& keyspace, const Sds& dest, const Vector<Sds>& members) {
  LedisObject obj = LedisObject::makeSet();
  for (size_t i = 0; i < members.size(); ++i) {
    obj.setAdd(members[i]);
  }
  if (members.empty()) {
    keyspace.del(dest);
  } else {
    keyspace.set(dest, Move(obj));
  }
  return CommandResult::integer(static_cast<int64_t>(members.size()));
}

CommandResult handleSinterstore(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() < 2) {
    return CommandResult::error("ERR wrong number of arguments for 'sinterstore' command");
  }
  Keyspace& keyspace = ks(ctx, db);
  Vector<LedisObject> holders;
  Vector<const SdsSet*> sets;
  CommandResult wrong;
  if (!loadSetArgs(keyspace, cmd.args, 1, &holders, &sets, &wrong)) {
    return wrong;
  }
  Vector<Sds> result;
  LedisObject::setIntersect(sets, &result);
  return storeSetResult(keyspace, cmd.args[0], result);
}

CommandResult handleSunionstore(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() < 2) {
    return CommandResult::error("ERR wrong number of arguments for 'sunionstore' command");
  }
  Keyspace& keyspace = ks(ctx, db);
  Vector<LedisObject> holders;
  Vector<const SdsSet*> sets;
  CommandResult wrong;
  if (!loadSetArgs(keyspace, cmd.args, 1, &holders, &sets, &wrong)) {
    return wrong;
  }
  Vector<Sds> result;
  LedisObject::setUnion(sets, &result);
  return storeSetResult(keyspace, cmd.args[0], result);
}

CommandResult handleSdiffstore(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() < 2) {
    return CommandResult::error("ERR wrong number of arguments for 'sdiffstore' command");
  }
  Keyspace& keyspace = ks(ctx, db);
  Vector<LedisObject> holders;
  Vector<const SdsSet*> sets;
  CommandResult wrong;
  if (!loadSetArgs(keyspace, cmd.args, 1, &holders, &sets, &wrong)) {
    return wrong;
  }
  const SdsSet* first = sets.empty() ? nullptr : sets[0];
  Vector<const SdsSet*> others;
  for (size_t i = 1; i < sets.size(); ++i) {
    others.push_back(sets[i]);
  }
  Vector<Sds> result;
  LedisObject::setDiff(first, others, &result);
  return storeSetResult(keyspace, cmd.args[0], result);
}

CommandResult handleZadd(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() < 3 || (cmd.args.size() % 2) == 0) {
    return CommandResult::error("ERR wrong number of arguments for 'zadd' command");
  }
  Keyspace& keyspace = ks(ctx, db);
  LedisObject obj;
  if (!keyspace.get(cmd.args[0], &obj)) {
    obj = LedisObject::makeZset();
  } else if (!obj.isZset()) {
    return CommandResult::wrongType();
  }
  size_t added = 0;
  for (size_t i = 1; i + 1 < cmd.args.size(); i += 2) {
    bool ok = false;
    const double score = parseDoubleArg(cmd.args[i], &ok);
    if (!ok) {
      return CommandResult::error("ERR value is not a valid float");
    }
    if (obj.zsetAdd(cmd.args[i + 1], score)) {
      ++added;
    }
  }
  keyspace.set(cmd.args[0], Move(obj));
  return CommandResult::integer(static_cast<int64_t>(added));
}

CommandResult handleZscore(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 2) {
    return CommandResult::error("ERR wrong number of arguments for 'zscore' command");
  }
  LedisObject obj;
  if (!ks(ctx, db).get(cmd.args[0], &obj)) {
    return CommandResult::nullBulk();
  }
  if (!obj.isZset()) {
    return CommandResult::wrongType();
  }
  double score = 0;
  if (!obj.zsetScore(cmd.args[1], &score)) {
    return CommandResult::nullBulk();
  }
  return CommandResult::bulk(Sds(formatScore(score)));
}

CommandResult handleZcard(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 1) {
    return CommandResult::error("ERR wrong number of arguments for 'zcard' command");
  }
  LedisObject obj;
  if (!ks(ctx, db).get(cmd.args[0], &obj)) {
    return CommandResult::integer(0);
  }
  if (!obj.isZset()) {
    return CommandResult::wrongType();
  }
  return CommandResult::integer(static_cast<int64_t>(obj.zsetLen()));
}

CommandResult handleZrange(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 3 && cmd.args.size() != 4) {
    return CommandResult::error("ERR wrong number of arguments for 'zrange' command");
  }
  bool withscores = false;
  if (cmd.args.size() == 4) {
    String opt = cmd.args[3].str();
    for (size_t i = 0; i < opt.size(); ++i) {
      opt[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(opt[i])));
    }
    if (opt != "WITHSCORES") {
      return CommandResult::error("ERR syntax error");
    }
    withscores = true;
  }
  bool ok = false;
  const int64_t start = parseInt64Arg(cmd.args[1], &ok);
  if (!ok) {
    return CommandResult::error("ERR value is not an integer or out of range");
  }
  const int64_t stop = parseInt64Arg(cmd.args[2], &ok);
  if (!ok) {
    return CommandResult::error("ERR value is not an integer or out of range");
  }
  LedisObject obj;
  if (!ks(ctx, db).get(cmd.args[0], &obj)) {
    return CommandResult::fromValue(RespValue::makeArray({}));
  }
  if (!obj.isZset()) {
    return CommandResult::wrongType();
  }
  Vector<Sds> members;
  Vector<double> scores;
  obj.zsetRangeByRank(start, stop, &members, withscores ? &scores : nullptr);
  return zsetMembersReply(members, withscores ? &scores : nullptr);
}

CommandResult handleZrevrange(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 3 && cmd.args.size() != 4) {
    return CommandResult::error("ERR wrong number of arguments for 'zrevrange' command");
  }
  bool withscores = false;
  if (cmd.args.size() == 4) {
    String opt = cmd.args[3].str();
    for (size_t i = 0; i < opt.size(); ++i) {
      opt[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(opt[i])));
    }
    if (opt != "WITHSCORES") {
      return CommandResult::error("ERR syntax error");
    }
    withscores = true;
  }
  bool ok = false;
  const int64_t start = parseInt64Arg(cmd.args[1], &ok);
  if (!ok) {
    return CommandResult::error("ERR value is not an integer or out of range");
  }
  const int64_t stop = parseInt64Arg(cmd.args[2], &ok);
  if (!ok) {
    return CommandResult::error("ERR value is not an integer or out of range");
  }
  LedisObject obj;
  if (!ks(ctx, db).get(cmd.args[0], &obj)) {
    return CommandResult::fromValue(RespValue::makeArray({}));
  }
  if (!obj.isZset()) {
    return CommandResult::wrongType();
  }
  Vector<Sds> members;
  Vector<double> scores;
  obj.zsetRevRangeByRank(start, stop, &members, withscores ? &scores : nullptr);
  return zsetMembersReply(members, withscores ? &scores : nullptr);
}

CommandResult zrangeByScoreCommand(SessionContext& ctx, DBManager& db,
                                   const Command& cmd, bool reverse,
                                   const char* cmd_name) {
  if (cmd.args.size() < 3) {
    return CommandResult::error(
        (String("ERR wrong number of arguments for '") + cmd_name + "' command").c_str());
  }
  bool withscores = false;
  int64_t offset = 0;
  int64_t limit = -1;
  for (size_t i = 3; i < cmd.args.size();) {
    const String opt = toUpperAscii(cmd.args[i].str());
    if (opt == "WITHSCORES") {
      withscores = true;
      ++i;
      continue;
    }
    if (opt == "LIMIT") {
      if (i + 2 >= cmd.args.size()) {
        return CommandResult::error("ERR syntax error");
      }
      bool ok = false;
      offset = parseInt64Arg(cmd.args[i + 1], &ok);
      if (!ok || offset < 0) {
        return CommandResult::error("ERR value is not an integer or out of range");
      }
      limit = parseInt64Arg(cmd.args[i + 2], &ok);
      if (!ok || limit < 0) {
        return CommandResult::error("ERR value is not an integer or out of range");
      }
      i += 3;
      continue;
    }
    return CommandResult::error("ERR syntax error");
  }
  bool min_exclusive = false;
  bool max_exclusive = false;
  double min_score = 0;
  double max_score = 0;
  if (!parseZsetScoreBound(cmd.args[1], &min_score, &min_exclusive)) {
    return CommandResult::error("ERR min or max is not a float");
  }
  if (!parseZsetScoreBound(cmd.args[2], &max_score, &max_exclusive)) {
    return CommandResult::error("ERR min or max is not a float");
  }
  LedisObject obj;
  if (!ks(ctx, db).get(cmd.args[0], &obj)) {
    return CommandResult::fromValue(RespValue::makeArray({}));
  }
  if (!obj.isZset()) {
    return CommandResult::wrongType();
  }
  Vector<Sds> members;
  Vector<double> scores;
  if (reverse) {
    obj.zsetRevRangeByScore(min_score, min_exclusive, max_score, max_exclusive,
                            offset, limit, &members, withscores ? &scores : nullptr);
  } else {
    obj.zsetRangeByScore(min_score, min_exclusive, max_score, max_exclusive, offset,
                         limit, &members, withscores ? &scores : nullptr);
  }
  return zsetMembersReply(members, withscores ? &scores : nullptr);
}

CommandResult handleZrangebyscore(SessionContext& ctx, DBManager& db, const Command& cmd) {
  return zrangeByScoreCommand(ctx, db, cmd, false, "zrangebyscore");
}

CommandResult handleZrevrangebyscore(SessionContext& ctx, DBManager& db,
                                     const Command& cmd) {
  return zrangeByScoreCommand(ctx, db, cmd, true, "zrevrangebyscore");
}

CommandResult handleZrem(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() < 2) {
    return CommandResult::error("ERR wrong number of arguments for 'zrem' command");
  }
  Keyspace& keyspace = ks(ctx, db);
  LedisObject obj;
  if (!keyspace.get(cmd.args[0], &obj)) {
    return CommandResult::integer(0);
  }
  if (!obj.isZset()) {
    return CommandResult::wrongType();
  }
  size_t removed = 0;
  for (size_t i = 1; i < cmd.args.size(); ++i) {
    removed += obj.zsetRem(cmd.args[i]);
  }
  if (obj.zsetLen() == 0) {
    keyspace.del(cmd.args[0]);
  } else {
    keyspace.set(cmd.args[0], Move(obj));
  }
  return CommandResult::integer(static_cast<int64_t>(removed));
}

CommandResult handleZrank(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 2) {
    return CommandResult::error("ERR wrong number of arguments for 'zrank' command");
  }
  LedisObject obj;
  if (!ks(ctx, db).get(cmd.args[0], &obj)) {
    return CommandResult::nullBulk();
  }
  if (!obj.isZset()) {
    return CommandResult::wrongType();
  }
  const int64_t rank = obj.zsetRank(cmd.args[1]);
  if (rank < 0) {
    return CommandResult::nullBulk();
  }
  return CommandResult::integer(rank);
}

CommandResult handleZrevrank(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 2) {
    return CommandResult::error("ERR wrong number of arguments for 'zrevrank' command");
  }
  LedisObject obj;
  if (!ks(ctx, db).get(cmd.args[0], &obj)) {
    return CommandResult::nullBulk();
  }
  if (!obj.isZset()) {
    return CommandResult::wrongType();
  }
  const int64_t rank = obj.zsetRevRank(cmd.args[1]);
  if (rank < 0) {
    return CommandResult::nullBulk();
  }
  return CommandResult::integer(rank);
}

CommandResult handleZincrby(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 3) {
    return CommandResult::error("ERR wrong number of arguments for 'zincrby' command");
  }
  bool ok = false;
  const double increment = parseDoubleArg(cmd.args[1], &ok);
  if (!ok) {
    return CommandResult::error("ERR value is not a valid float");
  }
  Keyspace& keyspace = ks(ctx, db);
  LedisObject obj;
  if (!keyspace.get(cmd.args[0], &obj)) {
    obj = LedisObject::makeZset();
  } else if (!obj.isZset()) {
    return CommandResult::wrongType();
  }
  const double score = obj.zsetIncrBy(cmd.args[2], increment);
  keyspace.set(cmd.args[0], Move(obj));
  return CommandResult::bulk(Sds(formatScore(score)));
}

CommandResult handleZcount(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (cmd.args.size() != 3) {
    return CommandResult::error("ERR wrong number of arguments for 'zcount' command");
  }
  LedisObject obj;
  if (!ks(ctx, db).get(cmd.args[0], &obj)) {
    return CommandResult::integer(0);
  }
  if (!obj.isZset()) {
    return CommandResult::wrongType();
  }
  double min_score = 0;
  double max_score = 0;
  bool min_exclusive = false;
  bool max_exclusive = false;
  if (!parseZsetScoreBound(cmd.args[1], &min_score, &min_exclusive) ||
      !parseZsetScoreBound(cmd.args[2], &max_score, &max_exclusive)) {
    return CommandResult::error("ERR min or max is not a float value");
  }
  const size_t count =
      obj.zsetCountByScore(min_score, min_exclusive, max_score, max_exclusive);
  return CommandResult::integer(static_cast<int64_t>(count));
}

bool watchesIntact(SessionContext& ctx, DBManager& db) {
  if (ctx.watch_tokens.empty()) {
    return true;
  }
  const int64_t now = nowMs();
  Keyspace& keyspace = ks(ctx, db);
  for (StdUnorderedMap<String, String>::const_iterator it = ctx.watch_tokens.begin();
       it != ctx.watch_tokens.end(); ++it) {
    if (!keyspace.watchTokenMatches(Sds(it->first), now, it->second)) {
      return false;
    }
  }
  return true;
}

CommandResult handleWatch(SessionContext& ctx, DBManager& db, const Command& cmd) {
  if (ctx.in_multi) {
    return CommandResult::error("ERR WATCH inside MULTI is not allowed");
  }
  if (cmd.args.empty()) {
    return CommandResult::error("ERR wrong number of arguments for 'watch' command");
  }
  const int64_t now = nowMs();
  Keyspace& keyspace = ks(ctx, db);
  for (size_t i = 0; i < cmd.args.size(); ++i) {
    ctx.watch_tokens[cmd.args[i].str()] = keyspace.watchToken(cmd.args[i], now);
  }
  return CommandResult::ok();
}

CommandResult handleUnwatch(SessionContext& ctx, DBManager&, const Command& cmd) {
  if (!cmd.args.empty()) {
    return CommandResult::error("ERR wrong number of arguments for 'unwatch' command");
  }
  ctx.watch_tokens.clear();
  return CommandResult::ok();
}

CommandResult handleInfo(SessionContext&, DBManager& db, const Command& cmd) {
  if (cmd.args.size() > 1) {
    return CommandResult::error("ERR wrong number of arguments for 'info' command");
  }
  String section;
  if (!cmd.args.empty()) {
    section = cmd.args[0].str();
    for (size_t i = 0; i < section.size(); ++i) {
      section[i] = static_cast<char>(
          std::tolower(static_cast<unsigned char>(section[i])));
    }
  }
  String out;
  const bool all = section.empty();
  if (all || section == "server") {
    out += "# Server\r\n";
    out += "redis_version:7.0-ledis\r\n";
    out += "ledis_mode:standalone\r\n";
  }
  if (all || section == "memory") {
    out += "# Memory\r\n";
    const int64_t now = nowMs();
    size_t keys = 0;
    for (size_t i = 0; i < db.dbCount(); ++i) {
      keys += db.keyspace(i).validSize(now);
    }
    out += "used_memory:" + std::to_string(db.usedMemoryBytes(now)) + "\r\n";
    out += "maxmemory:" + std::to_string(db.maxmemory()) + "\r\n";
    out += "maxmemory_policy:" + maxmemoryPolicyName(db.maxmemoryPolicy()) + "\r\n";
    out += "db_keys:" + std::to_string(keys) + "\r\n";
  }
  if (out.empty()) {
    return CommandResult::error("ERR invalid info section");
  }
  return CommandResult::bulk(Sds(Move(out)));
}

}  // namespace

void CommandRegistry::setRequirePass(const String& pass) {
  requirepass_ = pass;
}

void CommandRegistry::setServerConfig(uint16_t port, size_t maxclients,
                                      size_t maxmemory,
                                      const String& maxmemory_policy) {
  config_port_ = port;
  config_maxclients_ = maxclients;
  config_maxmemory_ = maxmemory;
  config_maxmemory_policy_ = maxmemory_policy;
}

void CommandRegistry::setSnapshotConfig(const String& dir,
                                        const String& dbfilename) {
  config_dir_ = dir;
  config_dbfilename_ = dbfilename;
}

void CommandRegistry::setAofConfig(bool appendonly,
                                   const String& appendfilename,
                                   const String& appendfsync) {
  config_appendonly_ = appendonly;
  config_appendfilename_ = appendfilename;
  config_appendfsync_ = appendfsync;
}

void CommandRegistry::setConfigApplyCallback(ConfigApplyFn fn) {
  config_apply_ = Move(fn);
}

CommandResult CommandRegistry::configGet(const String& param_raw) const {
  String param = param_raw;
  for (size_t i = 0; i < param.size(); ++i) {
    param[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(param[i])));
  }
  String value;
  if (param == "port") {
    value = std::to_string(config_port_);
  } else if (param == "maxclients") {
    value = std::to_string(config_maxclients_);
  } else if (param == "maxmemory") {
    value = std::to_string(config_maxmemory_);
  } else if (param == "maxmemory-policy" || param == "maxmemory_policy") {
    value = config_maxmemory_policy_;
  } else if (param == "databases" || param == "db_count") {
    value = std::to_string(DBManager::kDbCount);
  } else if (param == "dir") {
    value = config_dir_;
  } else if (param == "dbfilename") {
    value = config_dbfilename_;
  } else if (param == "appendonly") {
    value = config_appendonly_ ? "yes" : "no";
  } else if (param == "appendfilename") {
    value = config_appendfilename_;
  } else if (param == "appendfsync") {
    value = config_appendfsync_;
  } else if (param == "requirepass") {
    value = requirepass_;
  } else {
    return CommandResult::error("ERR unknown configuration parameter");
  }
  RespArray elems;
  elems.push_back(RespValue::makeBulk(Sds(param_raw)));
  elems.push_back(RespValue::makeBulk(Sds(Move(value))));
  return CommandResult::fromValue(RespValue::makeArray(Move(elems)));
}

CommandResult CommandRegistry::configSet(const String& param_raw,
                                         const String& value_raw) {
  String param = param_raw;
  for (size_t i = 0; i < param.size(); ++i) {
    param[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(param[i])));
  }
  if (param == "port" || param == "databases" || param == "db_count") {
    return CommandResult::error("ERR Unsupported CONFIG parameter");
  }
  if (param == "requirepass") {
    requirepass_ = value_raw;
  } else if (param == "maxclients") {
    if (value_raw.empty()) {
      return CommandResult::error("ERR invalid maxclients");
    }
    char* end = nullptr;
    const long long n = std::strtoll(value_raw.c_str(), &end, 10);
    if (end != value_raw.c_str() + static_cast<ptrdiff_t>(value_raw.size()) ||
        n < 0) {
      return CommandResult::error("ERR invalid maxclients");
    }
    config_maxclients_ = static_cast<size_t>(n);
  } else if (param == "maxmemory") {
    char* end = nullptr;
    const long long n = std::strtoll(value_raw.c_str(), &end, 10);
    if (end != value_raw.c_str() + static_cast<ptrdiff_t>(value_raw.size()) ||
        n < 0) {
      return CommandResult::error("ERR invalid maxmemory");
    }
    config_maxmemory_ = static_cast<size_t>(n);
  } else if (param == "maxmemory-policy" || param == "maxmemory_policy") {
    bool ok = false;
    (void)parseMaxmemoryPolicy(value_raw, &ok);
    if (!ok) {
      return CommandResult::error("ERR unsupported maxmemory policy");
    }
    String v = value_raw;
    for (size_t i = 0; i < v.size(); ++i) {
      v[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(v[i])));
    }
    for (size_t i = 0; i < v.size(); ++i) {
      if (v[i] == '_') {
        v[i] = '-';
      }
    }
    config_maxmemory_policy_ = v;
  } else if (param == "dir") {
    if (value_raw.empty()) {
      return CommandResult::error("ERR invalid dir");
    }
    config_dir_ = value_raw;
  } else if (param == "dbfilename") {
    if (value_raw.empty()) {
      return CommandResult::error("ERR invalid dbfilename");
    }
    config_dbfilename_ = value_raw;
  } else if (param == "appendonly") {
    String v = value_raw;
    for (size_t i = 0; i < v.size(); ++i) {
      v[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(v[i])));
    }
    if (v != "yes" && v != "no") {
      return CommandResult::error("ERR invalid appendonly value");
    }
    config_appendonly_ = (v == "yes");
  } else if (param == "appendfilename") {
    if (value_raw.empty()) {
      return CommandResult::error("ERR invalid appendfilename");
    }
    config_appendfilename_ = value_raw;
  } else if (param == "appendfsync") {
    bool ok = false;
    (void)parseAppendFsyncPolicy(value_raw, &ok);
    if (!ok) {
      return CommandResult::error("ERR invalid appendfsync value");
    }
    String v = value_raw;
    for (size_t i = 0; i < v.size(); ++i) {
      v[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(v[i])));
    }
    config_appendfsync_ = v;
  } else {
    return CommandResult::error("ERR unknown configuration parameter");
  }
  if (config_apply_) {
    config_apply_(param, value_raw);
  }
  return CommandResult::ok();
}

void CommandRegistry::registerHandler(const String& name, Handler handler) {
  handlers_[name] = Move(handler);
}

void CommandRegistry::registerDefaultCommands() {
  registerHandler("PING", handlePing);
  registerHandler("ECHO", handleEcho);
  registerHandler("SELECT", handleSelect);
  registerHandler("GET", handleGet);
  registerHandler("SET", handleSet);
  registerHandler("GETSET", handleGetset);
  registerHandler("APPEND", handleAppend);
  registerHandler("STRLEN", handleStrlen);
  registerHandler("SETEX", handleSetex);
  registerHandler("PSETEX", handlePsetex);
  registerHandler("DEL", handleDel);
  registerHandler("EXISTS", handleExists);
  registerHandler("DBSIZE", handleDbsize);
  registerHandler("FLUSHDB", handleFlushdb);
  registerHandler("FLUSHALL", handleFlushall);
  registerHandler("TYPE", handleType);
  registerHandler("RENAME", handleRename);
  registerHandler("RENAMENX", handleRenamenx);
  registerHandler("MOVE", handleMove);
  registerHandler("TOUCH", handleTouch);
  registerHandler("UNLINK", handleDel);
  registerHandler("KEYS", handleKeys);
  registerHandler("RANDOMKEY", handleRandomkey);
  registerHandler("SCAN", handleScan);
  registerHandler("EXPIRE", handleExpire);
  registerHandler("PEXPIRE", handlePexpire);
  registerHandler("EXPIREAT", handleExpireat);
  registerHandler("PEXPIREAT", handlePexpireat);
  registerHandler("TTL", handleTtl);
  registerHandler("PTTL", handlePttl);
  registerHandler("PERSIST", handlePersist);
  registerHandler("MGET", handleMget);
  registerHandler("MSET", handleMset);
  registerHandler("INCR", handleIncr);
  registerHandler("DECR", handleDecr);
  registerHandler("INCRBY", handleIncrby);
  registerHandler("DECRBY", handleDecrby);
  registerHandler("HSET", handleHset);
  registerHandler("HGET", handleHget);
  registerHandler("HDEL", handleHdel);
  registerHandler("HGETALL", handleHgetall);
  registerHandler("HLEN", handleHlen);
  registerHandler("HEXISTS", handleHexists);
  registerHandler("HMGET", handleHmget);
  registerHandler("HKEYS", handleHkeys);
  registerHandler("HVALS", handleHvals);
  registerHandler("HINCRBY", handleHincrby);
  registerHandler("LPUSH", handleLpush);
  registerHandler("RPUSH", handleRpush);
  registerHandler("LPOP", handleLpop);
  registerHandler("RPOP", handleRpop);
  registerHandler("LLEN", handleLlen);
  registerHandler("LRANGE", handleLrange);
  registerHandler("LINDEX", handleLindex);
  registerHandler("LTRIM", handleLtrim);
  registerHandler("SADD", handleSadd);
  registerHandler("SREM", handleSrem);
  registerHandler("SMEMBERS", handleSmembers);
  registerHandler("SCARD", handleScard);
  registerHandler("SISMEMBER", handleSismember);
  registerHandler("SINTER", handleSinter);
  registerHandler("SUNION", handleSunion);
  registerHandler("SDIFF", handleSdiff);
  registerHandler("SINTERSTORE", handleSinterstore);
  registerHandler("SUNIONSTORE", handleSunionstore);
  registerHandler("SDIFFSTORE", handleSdiffstore);
  registerHandler("ZADD", handleZadd);
  registerHandler("ZSCORE", handleZscore);
  registerHandler("ZCARD", handleZcard);
  registerHandler("ZRANGE", handleZrange);
  registerHandler("ZREVRANGE", handleZrevrange);
  registerHandler("ZRANGEBYSCORE", handleZrangebyscore);
  registerHandler("ZREVRANGEBYSCORE", handleZrevrangebyscore);
  registerHandler("ZREM", handleZrem);
  registerHandler("ZINCRBY", handleZincrby);
  registerHandler("ZCOUNT", handleZcount);
  registerHandler("ZRANK", handleZrank);
  registerHandler("ZREVRANK", handleZrevrank);
  registerHandler("WATCH", handleWatch);
  registerHandler("UNWATCH", handleUnwatch);
  registerHandler("INFO", handleInfo);
  registerHandler("AUTH", [this](SessionContext& ctx, DBManager& db, const Command& cmd) {
    (void)db;
    if (requirepass_.empty()) {
      return CommandResult::error("ERR Client sent AUTH, but no password is set");
    }
    if (cmd.args.size() != 1 && cmd.args.size() != 2) {
      return CommandResult::error("ERR wrong number of arguments for 'auth' command");
    }
    const Sds& pass = cmd.args.size() == 1 ? cmd.args[0] : cmd.args[1];
    if (pass.str() != requirepass_) {
      return CommandResult::error("ERR invalid password");
    }
    ctx.authenticated = true;
    return CommandResult::ok();
  });
  registerHandler("CONFIG", [this](SessionContext& ctx, DBManager& db, const Command& cmd) {
    (void)ctx;
    (void)db;
    if (cmd.args.empty()) {
      return CommandResult::error("ERR wrong number of arguments for 'config' command");
    }
    String sub = cmd.args[0].str();
    for (size_t i = 0; i < sub.size(); ++i) {
      sub[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(sub[i])));
    }
    if (sub == "get") {
      if (cmd.args.size() != 2) {
        return CommandResult::error("ERR wrong number of arguments for 'config get' command");
      }
      return configGet(cmd.args[1].str());
    }
    if (sub == "set") {
      if (cmd.args.size() != 3) {
        return CommandResult::error("ERR wrong number of arguments for 'config set' command");
      }
      return configSet(cmd.args[1].str(), cmd.args[2].str());
    }
    return CommandResult::error("ERR CONFIG subcommand must be GET or SET");
  });
}

CommandResult CommandRegistry::dispatchOne(SessionContext& ctx, DBManager& db,
                                           const Command& cmd) const {
  DBManager::CommandScope scope(db, static_cast<size_t>(ctx.db_index));
  const String& name = cmd.name.str();
  if (ctx.inPubSub()) {
    if (name != "SUBSCRIBE" && name != "UNSUBSCRIBE" && name != "PSUBSCRIBE" &&
        name != "PUNSUBSCRIBE" && name != "PING") {
      return CommandResult::error(
          "ERR only (P)SUBSCRIBE / (P)UNSUBSCRIBE / PING / QUIT allowed in this context");
    }
  }
  if (!requirepass_.empty() && !ctx.authenticated) {
    if (name != "AUTH" && name != "PING" && name != "ECHO") {
      return CommandResult::noAuth();
    }
  }
  const auto it = handlers_.find(cmd.name.str());
  if (it == handlers_.end()) {
    return CommandResult::fromValue(RespValue::makeError(
        Sds("ERR unknown command '" + cmd.name.str() + "'")));
  }
  return it->second(ctx, db, cmd);
}

CommandResult CommandRegistry::dispatch(SessionContext& ctx, DBManager& db,
                                        const Command& cmd) const {
  const String& name = cmd.name.str();

  if (ctx.in_multi) {
    if (name == "EXEC") {
      if (!cmd.args.empty()) {
        return CommandResult::error(
            "ERR wrong number of arguments for 'exec' command");
      }
      ctx.in_multi = false;
      const StdVector<Command> queue = Move(ctx.multi_queue);
      ctx.multi_queue.clear();
      if (!watchesIntact(ctx, db)) {
        ctx.watch_tokens.clear();
        return CommandResult::nullBulk();
      }
      ctx.watch_tokens.clear();
      RespArray results;
      results.reserve(queue.size());
      for (size_t i = 0; i < queue.size(); ++i) {
        results.push_back(dispatchOne(ctx, db, queue[i]).value);
      }
      return CommandResult::fromValue(RespValue::makeArray(Move(results)));
    }
    if (name == "DISCARD") {
      if (!cmd.args.empty()) {
        return CommandResult::error(
            "ERR wrong number of arguments for 'discard' command");
      }
      ctx.in_multi = false;
      ctx.multi_queue.clear();
      return CommandResult::ok();
    }
    if (name == "MULTI") {
      return CommandResult::error("ERR MULTI calls can not be nested");
    }
    if (name == "WATCH") {
      return CommandResult::error("ERR WATCH inside MULTI is not allowed");
    }
    ctx.multi_queue.push_back(cmd);
    return CommandResult::queued();
  }

  if (name == "MULTI") {
    if (!cmd.args.empty()) {
      return CommandResult::error(
          "ERR wrong number of arguments for 'multi' command");
    }
    ctx.in_multi = true;
    ctx.multi_queue.clear();
    return CommandResult::ok();
  }
  if (name == "EXEC") {
    return CommandResult::error("ERR EXEC without MULTI");
  }
  if (name == "DISCARD") {
    return CommandResult::error("ERR DISCARD without MULTI");
  }

  return dispatchOne(ctx, db, cmd);
}

}  // namespace ledis
