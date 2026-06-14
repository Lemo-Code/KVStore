#include "ledis/store/aof.h"

#include "ledis/protocol/resp_reader.h"
#include "ledis/protocol/resp_writer.h"
#include "ledis/store/keyspace.h"
#include "ledis/store/snapshot.h"

#include "lemo/buffer/chain_buffer.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace ledis {
namespace {

constexpr const char* kAofMagic = "LEDIS-AOF1\r\n";

bool isWriteName(const String& name) {
  static const char* kNames[] = {
      "SELECT",   "SET",      "SETEX",    "PSETEX",   "GETSET",   "APPEND",   "DEL",
      "MSET",     "EXPIRE",   "PEXPIRE",  "EXPIREAT", "PEXPIREAT", "PERSIST",  "RENAME",
      "RENAMENX", "MOVE",     "INCR",     "DECR",     "INCRBY",   "DECRBY",   "HSET",     "HDEL",     "HINCRBY",
      "LPUSH",    "RPUSH",    "LPOP",     "RPOP",     "RPOPLPUSH", "LTRIM",    "SADD",     "SREM",
      "SINTERSTORE", "SUNIONSTORE", "SDIFFSTORE",
      "ZADD",     "ZINCRBY",  "ZREM",     "FLUSHDB",  "FLUSHALL", nullptr,
  };
  for (size_t i = 0; kNames[i] != nullptr; ++i) {
    if (name == kNames[i]) {
      return true;
    }
  }
  return false;
}

String formatScore(double score) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.17g", score);
  return String(buf);
}

bool appendCommand(AofWriter* writer, Command* cmd) {
  return writer && writer->append(*cmd);
}

bool rewriteString(AofWriter* writer, const Sds& key, const LedisObject& obj,
                    int64_t expire_at_ms, int64_t now_ms) {
  Sds value;
  if (!obj.asString(&value)) {
    return false;
  }
  Command cmd;
  cmd.name = Sds("SET");
  cmd.args.push_back(key);
  cmd.args.push_back(Move(value));
  if (expire_at_ms > 0) {
    const int64_t ttl_ms = expire_at_ms - now_ms;
    if (ttl_ms <= 0) {
      return true;
    }
    cmd.args.push_back(Sds("PX"));
    cmd.args.push_back(Sds(std::to_string(ttl_ms)));
  }
  return appendCommand(writer, &cmd);
}

bool rewriteHash(AofWriter* writer, const Sds& key, const LedisObject& obj,
                 int64_t expire_at_ms, int64_t now_ms) {
  const HashDict* hash = obj.asHash();
  if (!hash) {
    return false;
  }
  for (HashDict::const_iterator it = hash->begin(); it != hash->end(); ++it) {
    Command cmd;
    cmd.name = Sds("HSET");
    cmd.args.push_back(key);
    cmd.args.push_back(it->first);
    cmd.args.push_back(it->second);
    if (!appendCommand(writer, &cmd)) {
      return false;
    }
  }
  if (expire_at_ms > 0) {
    const int64_t ttl_ms = expire_at_ms - now_ms;
    if (ttl_ms <= 0) {
      return true;
    }
    Command expire;
    expire.name = Sds("PEXPIRE");
    expire.args.push_back(key);
    expire.args.push_back(Sds(std::to_string(ttl_ms)));
    return appendCommand(writer, &expire);
  }
  return true;
}

bool rewriteList(AofWriter* writer, const Sds& key, const LedisObject& obj,
                 int64_t expire_at_ms, int64_t now_ms) {
  const ListDeque* list = obj.asList();
  if (!list || list->empty()) {
    return true;
  }
  Command cmd;
  cmd.name = Sds("RPUSH");
  cmd.args.push_back(key);
  for (size_t i = 0; i < list->size(); ++i) {
    cmd.args.push_back((*list)[i]);
  }
  if (!appendCommand(writer, &cmd)) {
    return false;
  }
  if (expire_at_ms > 0) {
    const int64_t ttl_ms = expire_at_ms - now_ms;
    if (ttl_ms <= 0) {
      return true;
    }
    Command expire;
    expire.name = Sds("PEXPIRE");
    expire.args.push_back(key);
    expire.args.push_back(Sds(std::to_string(ttl_ms)));
    return appendCommand(writer, &expire);
  }
  return true;
}

bool rewriteSet(AofWriter* writer, const Sds& key, const LedisObject& obj,
                int64_t expire_at_ms, int64_t now_ms) {
  const SdsSet* set = obj.asSet();
  if (!set || set->empty()) {
    return true;
  }
  Command cmd;
  cmd.name = Sds("SADD");
  cmd.args.push_back(key);
  for (SdsSet::const_iterator it = set->begin(); it != set->end(); ++it) {
    cmd.args.push_back(*it);
  }
  if (!appendCommand(writer, &cmd)) {
    return false;
  }
  if (expire_at_ms > 0) {
    const int64_t ttl_ms = expire_at_ms - now_ms;
    if (ttl_ms <= 0) {
      return true;
    }
    Command expire;
    expire.name = Sds("PEXPIRE");
    expire.args.push_back(key);
    expire.args.push_back(Sds(std::to_string(ttl_ms)));
    return appendCommand(writer, &expire);
  }
  return true;
}

bool rewriteZset(AofWriter* writer, const Sds& key, const LedisObject& obj,
                 int64_t expire_at_ms, int64_t now_ms) {
  const ZsetDict* zset = obj.asZset();
  if (!zset || zset->empty()) {
    return true;
  }
  Command cmd;
  cmd.name = Sds("ZADD");
  cmd.args.push_back(key);
  for (ZsetDict::const_iterator it = zset->begin(); it != zset->end(); ++it) {
    cmd.args.push_back(Sds(formatScore(it->second)));
    cmd.args.push_back(it->first);
  }
  if (!appendCommand(writer, &cmd)) {
    return false;
  }
  if (expire_at_ms > 0) {
    const int64_t ttl_ms = expire_at_ms - now_ms;
    if (ttl_ms <= 0) {
      return true;
    }
    Command expire;
    expire.name = Sds("PEXPIRE");
    expire.args.push_back(key);
    expire.args.push_back(Sds(std::to_string(ttl_ms)));
    return appendCommand(writer, &expire);
  }
  return true;
}

bool rewriteRecord(AofWriter* writer, const KeyspaceRecord& rec, int64_t now_ms) {
  if (rec.obj.isString()) {
    return rewriteString(writer, rec.key, rec.obj, rec.expire_at_ms, now_ms);
  }
  if (rec.obj.isHash()) {
    return rewriteHash(writer, rec.key, rec.obj, rec.expire_at_ms, now_ms);
  }
  if (rec.obj.isList()) {
    return rewriteList(writer, rec.key, rec.obj, rec.expire_at_ms, now_ms);
  }
  if (rec.obj.isSet()) {
    return rewriteSet(writer, rec.key, rec.obj, rec.expire_at_ms, now_ms);
  }
  if (rec.obj.isZset()) {
    return rewriteZset(writer, rec.key, rec.obj, rec.expire_at_ms, now_ms);
  }
  return false;
}

}  // namespace

AppendFsyncPolicy parseAppendFsyncPolicy(const String& value_raw, bool* ok) {
  String value = value_raw;
  for (size_t i = 0; i < value.size(); ++i) {
    value[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(value[i])));
  }
  if (value == "always") {
    if (ok) {
      *ok = true;
    }
    return AppendFsyncPolicy::kAlways;
  }
  if (value == "everysec") {
    if (ok) {
      *ok = true;
    }
    return AppendFsyncPolicy::kEverySec;
  }
  if (value == "no") {
    if (ok) {
      *ok = true;
    }
    return AppendFsyncPolicy::kNo;
  }
  if (ok) {
    *ok = false;
  }
  return AppendFsyncPolicy::kEverySec;
}

String appendFsyncPolicyName(AppendFsyncPolicy policy) {
  switch (policy) {
    case AppendFsyncPolicy::kAlways:
      return String("always");
    case AppendFsyncPolicy::kNo:
      return String("no");
    case AppendFsyncPolicy::kEverySec:
    default:
      return String("everysec");
  }
}

String makeAofPath(const String& dir, const String& appendfilename) {
  return makeSnapshotPath(dir, appendfilename);
}

bool isAofWriteCommand(const Command& cmd) { return isWriteName(cmd.name.str()); }

AofWriter::~AofWriter() { close(); }

bool AofWriter::open(const String& path, bool truncate) {
  close();
  path_ = path;
  if (!truncate) {
    FILE* probe = std::fopen(path.c_str(), "rb");
    if (!probe) {
      truncate = true;
    } else {
      std::fclose(probe);
    }
  }
  const char* mode = truncate ? "wb" : "ab";
  fp_ = std::fopen(path.c_str(), mode);
  if (!fp_) {
    return false;
  }
  if (truncate) {
    if (std::fwrite(kAofMagic, 1, std::strlen(kAofMagic), fp_) !=
            std::strlen(kAofMagic) ||
        std::fflush(fp_) != 0) {
      close();
      return false;
    }
  }
  return true;
}

void AofWriter::close() {
  if (fp_) {
    std::fclose(fp_);
    fp_ = nullptr;
  }
  path_.clear();
}

bool AofWriter::appendRaw(const String& wire) {
  if (!fp_ || wire.empty()) {
    return false;
  }
  return std::fwrite(wire.data(), 1, wire.size(), fp_) == wire.size();
}

bool AofWriter::append(const Command& cmd) {
  if (!fp_) {
    return false;
  }
  SdsArgList argv;
  argv.push_back(cmd.name);
  for (size_t i = 0; i < cmd.args.size(); ++i) {
    argv.push_back(cmd.args[i]);
  }
  return appendRaw(RespWriter::encodeCommand(argv));
}

bool AofWriter::flushBuffer() {
  if (!fp_) {
    return false;
  }
  return std::fflush(fp_) == 0;
}

bool AofWriter::fsyncDisk() {
  if (!fp_) {
    return false;
  }
  return ::fsync(::fileno(fp_)) == 0;
}

bool AofWriter::flush() {
  return flushBuffer() && fsyncDisk();
}

bool rewriteAofFromDb(const DBManager& db, const String& path, int64_t now_ms) {
  AofWriter writer;
  if (!writer.open(path, true)) {
    return false;
  }
  for (size_t db_idx = 0; db_idx < db.dbCount(); ++db_idx) {
    Vector<KeyspaceRecord> records;
    db.keyspace(db_idx).collectRecords(now_ms, &records);
    if (records.empty()) {
      continue;
    }
    Command select;
    select.name = Sds("SELECT");
    select.args.push_back(Sds(std::to_string(db_idx)));
    if (!writer.append(select)) {
      return false;
    }
    for (size_t i = 0; i < records.size(); ++i) {
      if (!rewriteRecord(&writer, records[i], now_ms)) {
        return false;
      }
    }
  }
  return writer.flush();
}

bool appendRawCommandsToFile(const String& path, const StdVector<String>& frames) {
  if (frames.empty()) {
    return true;
  }
  FILE* fp = std::fopen(path.c_str(), "ab");
  if (!fp) {
    return false;
  }
  for (size_t i = 0; i < frames.size(); ++i) {
    if (std::fwrite(frames[i].data(), 1, frames[i].size(), fp) != frames[i].size()) {
      std::fclose(fp);
      return false;
    }
  }
  if (std::fflush(fp) != 0) {
    std::fclose(fp);
    return false;
  }
  const bool ok = ::fsync(::fileno(fp)) == 0;
  std::fclose(fp);
  return ok;
}

bool loadAof(const String& path, DBManager* db, CommandRegistry* registry,
             SessionContext* ctx) {
  if (!db || !registry || !ctx) {
    return false;
  }
  FILE* in = std::fopen(path.c_str(), "rb");
  if (!in) {
    return true;
  }
  if (std::fseek(in, 0, SEEK_END) != 0) {
    std::fclose(in);
    return false;
  }
  const long size = std::ftell(in);
  if (size < 0) {
    std::fclose(in);
    return false;
  }
  if (std::fseek(in, 0, SEEK_SET) != 0) {
    std::fclose(in);
    return false;
  }
  if (size == 0) {
    std::fclose(in);
    return true;
  }
  String content(static_cast<size_t>(size), '\0');
  if (std::fread(&content[0], 1, content.size(), in) != content.size()) {
    std::fclose(in);
    return false;
  }
  std::fclose(in);

  const size_t magic_len = std::strlen(kAofMagic);
  size_t offset = 0;
  if (content.size() >= magic_len &&
      content.compare(0, magic_len, kAofMagic) == 0) {
    offset = magic_len;
  }

  lemo::buffer::ChainBuffer chain;
  if (offset < content.size()) {
    chain.append(content.data() + offset, content.size() - offset);
  }
  RespReader reader(chain);
  for (;;) {
    Command cmd;
    const ParseResult pr = reader.parseCommand(&cmd);
    if (pr == ParseResult::kNeedMore) {
      break;
    }
    if (pr != ParseResult::kOk) {
      return false;
    }
    const CommandResult result = registry->dispatch(*ctx, *db, cmd);
    if (result.value.isError()) {
      return false;
    }
  }
  return chain.readable() == 0;
}

}  // namespace ledis
