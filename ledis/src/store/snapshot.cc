#include "ledis/store/snapshot.h"

#include "ledis/protocol/resp_reader.h"
#include "ledis/protocol/resp_writer.h"

#include "lemo/buffer/chain_buffer.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>

namespace ledis {
namespace {

constexpr const char* kSnapshotMagic = "LEDIS003\r\n";

int64_t nowMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
      .count();
}

RespValue objectToResp(const LedisObject& obj) {
  if (obj.isString()) {
    Sds value;
    obj.asString(&value);
    RespArray payload;
    payload.push_back(RespValue::makeBulk(Sds("string")));
    payload.push_back(RespValue::makeBulk(Move(value)));
    return RespValue::makeArray(Move(payload));
  }
  if (obj.isHash()) {
    RespArray fields;
    const HashDict* hash = obj.asHash();
    if (hash) {
      for (HashDict::const_iterator it = hash->begin(); it != hash->end(); ++it) {
        RespArray pair;
        pair.push_back(RespValue::makeBulk(it->first));
        pair.push_back(RespValue::makeBulk(it->second));
        fields.push_back(RespValue::makeArray(Move(pair)));
      }
    }
    RespArray payload;
    payload.push_back(RespValue::makeBulk(Sds("hash")));
    payload.push_back(RespValue::makeArray(Move(fields)));
    return RespValue::makeArray(Move(payload));
  }
  if (obj.isList()) {
    RespArray elems;
    const ListDeque* list = obj.asList();
    if (list) {
      for (size_t i = 0; i < list->size(); ++i) {
        elems.push_back(RespValue::makeBulk((*list)[i]));
      }
    }
    RespArray payload;
    payload.push_back(RespValue::makeBulk(Sds("list")));
    payload.push_back(RespValue::makeArray(Move(elems)));
    return RespValue::makeArray(Move(payload));
  }
  if (obj.isSet()) {
    RespArray members;
    const SdsSet* set = obj.asSet();
    if (set) {
      for (SdsSet::const_iterator it = set->begin(); it != set->end(); ++it) {
        members.push_back(RespValue::makeBulk(*it));
      }
    }
    RespArray payload;
    payload.push_back(RespValue::makeBulk(Sds("set")));
    payload.push_back(RespValue::makeArray(Move(members)));
    return RespValue::makeArray(Move(payload));
  }
  RespArray pairs;
  const ZsetDict* zset = obj.asZset();
  if (zset) {
    for (ZsetDict::const_iterator it = zset->begin(); it != zset->end(); ++it) {
      RespArray pair;
      pair.push_back(RespValue::makeBulk(it->first));
      char score_buf[64];
      std::snprintf(score_buf, sizeof(score_buf), "%.17g", it->second);
      pair.push_back(RespValue::makeBulk(Sds(score_buf)));
      pairs.push_back(RespValue::makeArray(Move(pair)));
    }
  }
  RespArray payload;
  payload.push_back(RespValue::makeBulk(Sds("zset")));
  payload.push_back(RespValue::makeArray(Move(pairs)));
  return RespValue::makeArray(Move(payload));
}

bool respToObject(const RespValue& payload, LedisObject* out) {
  if (!out || payload.type != RespType::kArray || payload.array.size() != 2 ||
      payload.array[0].type != RespType::kBulkString) {
    return false;
  }
  const String& type = payload.array[0].bulk.str();
  const RespValue& data = payload.array[1];
  if (type == "string") {
    if (data.type != RespType::kBulkString) {
      return false;
    }
    *out = LedisObject::makeString(data.bulk);
    return true;
  }
  if (type == "hash") {
    if (data.type != RespType::kArray) {
      return false;
    }
    LedisObject obj = LedisObject::makeHash();
    for (size_t i = 0; i < data.array.size(); ++i) {
      const RespValue& pair = data.array[i];
      if (pair.type != RespType::kArray || pair.array.size() != 2 ||
          pair.array[0].type != RespType::kBulkString ||
          pair.array[1].type != RespType::kBulkString) {
        return false;
      }
      obj.hashSet(pair.array[0].bulk, pair.array[1].bulk);
    }
    *out = Move(obj);
    return true;
  }
  if (type == "list") {
    if (data.type != RespType::kArray) {
      return false;
    }
    LedisObject obj = LedisObject::makeList();
    for (size_t i = 0; i < data.array.size(); ++i) {
      if (data.array[i].type != RespType::kBulkString) {
        return false;
      }
      obj.listPushBack(data.array[i].bulk);
    }
    *out = Move(obj);
    return true;
  }
  if (type == "set") {
    if (data.type != RespType::kArray) {
      return false;
    }
    LedisObject obj = LedisObject::makeSet();
    for (size_t i = 0; i < data.array.size(); ++i) {
      if (data.array[i].type != RespType::kBulkString) {
        return false;
      }
      obj.setAdd(data.array[i].bulk);
    }
    *out = Move(obj);
    return true;
  }
  if (type == "zset") {
    if (data.type != RespType::kArray) {
      return false;
    }
    LedisObject obj = LedisObject::makeZset();
    for (size_t i = 0; i < data.array.size(); ++i) {
      const RespValue& pair = data.array[i];
      if (pair.type != RespType::kArray || pair.array.size() != 2 ||
          pair.array[0].type != RespType::kBulkString ||
          pair.array[1].type != RespType::kBulkString) {
        return false;
      }
      char* end = nullptr;
      const double score =
          std::strtod(pair.array[1].bulk.data(), &end);
      if (end != pair.array[1].bulk.data() +
                     static_cast<ptrdiff_t>(pair.array[1].bulk.size())) {
        return false;
      }
      obj.zsetAdd(pair.array[0].bulk, score);
    }
    *out = Move(obj);
    return true;
  }
  return false;
}

RespValue dbToResp(const DBManager& db, int64_t now_ms) {
  RespArray dbs;
  dbs.reserve(DBManager::kDbCount);
  for (size_t db_index = 0; db_index < DBManager::kDbCount; ++db_index) {
    Vector<KeyspaceRecord> records;
    db.keyspace(db_index).collectRecords(now_ms, &records);
    RespArray entries;
    entries.reserve(records.size());
    for (size_t i = 0; i < records.size(); ++i) {
      const KeyspaceRecord& rec = records[i];
      RespArray entry;
      entry.push_back(RespValue::makeBulk(rec.key));
      entry.push_back(RespValue::makeInteger(rec.expire_at_ms));
      entry.push_back(objectToResp(rec.obj));
      entries.push_back(RespValue::makeArray(Move(entry)));
    }
    dbs.push_back(RespValue::makeArray(Move(entries)));
  }
  return RespValue::makeArray(Move(dbs));
}

bool loadDbFromResp(const RespValue& root, DBManager* db) {
  if (!db || root.type != RespType::kArray ||
      root.array.size() != DBManager::kDbCount) {
    return false;
  }
  for (size_t db_index = 0; db_index < DBManager::kDbCount; ++db_index) {
    const RespValue& db_val = root.array[db_index];
    if (db_val.type != RespType::kArray) {
      return false;
    }
    Vector<KeyspaceRecord> records;
    for (size_t i = 0; i < db_val.array.size(); ++i) {
      const RespValue& entry = db_val.array[i];
      if (entry.type != RespType::kArray || entry.array.size() != 3 ||
          entry.array[0].type != RespType::kBulkString ||
          entry.array[1].type != RespType::kInteger) {
        return false;
      }
      KeyspaceRecord rec;
      rec.key = entry.array[0].bulk;
      rec.expire_at_ms = entry.array[1].integer;
      if (!respToObject(entry.array[2], &rec.obj)) {
        return false;
      }
      records.push_back(Move(rec));
    }
    db->keyspace(db_index).replaceRecords(records);
  }
  return true;
}

bool writeFileAtomic(const String& path, const String& content) {
  const String tmp = path + ".tmp";
  {
    std::ofstream out(tmp.c_str(), std::ios::binary | std::ios::trunc);
    if (!out) {
      return false;
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!out.good()) {
      return false;
    }
  }
  if (std::rename(tmp.c_str(), path.c_str()) != 0) {
    return false;
  }
  return true;
}

String readFile(const String& path) {
  std::ifstream in(path.c_str(), std::ios::binary);
  if (!in) {
    return String();
  }
  in.seekg(0, std::ios::end);
  const std::streamoff size = in.tellg();
  if (size < 0) {
    return String();
  }
  String content(static_cast<size_t>(size), '\0');
  in.seekg(0, std::ios::beg);
  in.read(&content[0], size);
  if (!in.good()) {
    return String();
  }
  return content;
}

}  // namespace

String makeSnapshotPath(const String& dir, const String& dbfilename) {
  if (dir.empty()) {
    return dbfilename;
  }
  if (dir.back() == '/') {
    return dir + dbfilename;
  }
  return dir + "/" + dbfilename;
}

bool saveSnapshot(const DBManager& db, const String& path, int64_t now_ms) {
  const RespValue root = dbToResp(db, now_ms);
  String content = kSnapshotMagic;
  content += RespWriter::encode(root);
  return writeFileAtomic(path, content);
}

bool loadSnapshot(const String& path, DBManager* db) {
  if (!db) {
    return false;
  }
  const String content = readFile(path);
  if (content.empty()) {
    return true;
  }
  const size_t magic_len = std::strlen(kSnapshotMagic);
  if (content.size() < magic_len ||
      content.compare(0, magic_len, kSnapshotMagic) != 0) {
    return false;
  }
  lemo::buffer::ChainBuffer chain;
  chain.append(content.data() + magic_len, content.size() - magic_len);
  RespReader reader(chain);
  RespValue root;
  if (reader.parseOne(&root) != ParseResult::kOk) {
    return false;
  }
  return loadDbFromResp(root, db);
}

DBManager cloneDbSnapshot(const DBManager& db, int64_t now_ms) {
  DBManager out;
  for (size_t i = 0; i < DBManager::kDbCount; ++i) {
    Vector<KeyspaceRecord> records;
    db.keyspace(i).collectRecords(now_ms, &records);
    out.keyspace(i).replaceRecords(records);
  }
  return out;
}

}  // namespace ledis
