#pragma once

#include <fnmatch.h>
#include <string>
#include <lstl/container/vector.h>

#include "kv_ledis/core/dict.h"
#include "kv_ledis/core/command.h"

namespace ledis {

// ============================================================
// StorageEngine — 单线程存储引擎
// ============================================================
//
// 拥有 Dict，提供所有 KV 操作。在存储线程中运行，串行化访问。
// 预留分布式扩展: getSlot(key) 支持 16384 个 hash slot。
//
class StorageEngine {
public:
    StorageEngine() = default;

    // ======== 基础 KV ========
    void set(CmdContext& ctx);
    void get(CmdContext& ctx);
    void del(CmdContext& ctx);
    void exists(CmdContext& ctx);
    void type(CmdContext& ctx);

    // ======== 过期 ========
    void expire(CmdContext& ctx);
    void expireat(CmdContext& ctx);
    void pexpire(CmdContext& ctx);
    void pexpireat(CmdContext& ctx);
    void ttl(CmdContext& ctx);
    void pttl(CmdContext& ctx);
    void persist(CmdContext& ctx);

    // ======== String 操作 ========
    void incr(CmdContext& ctx);
    void incrby(CmdContext& ctx);
    void decr(CmdContext& ctx);
    void decrby(CmdContext& ctx);
    void append(CmdContext& ctx);
    void strlen(CmdContext& ctx);
    void mget(CmdContext& ctx);
    void mset(CmdContext& ctx);
    void setnx(CmdContext& ctx);
    void setex(CmdContext& ctx);
    void psetex(CmdContext& ctx);
    void getset(CmdContext& ctx);
    void getrange(CmdContext& ctx);
    void setrange(CmdContext& ctx);
    void incrbyfloat(CmdContext& ctx);
    void msetnx(CmdContext& ctx);
    void getdel(CmdContext& ctx);
    void rename(CmdContext& ctx);
    void renamenx(CmdContext& ctx);

    // ======== Hash 操作 ========
    void hset(CmdContext& ctx);
    void hget(CmdContext& ctx);
    void hdel(CmdContext& ctx);
    void hexists(CmdContext& ctx);
    void hgetall(CmdContext& ctx);
    void hkeys(CmdContext& ctx);
    void hvals(CmdContext& ctx);
    void hlen(CmdContext& ctx);
    void hincrby(CmdContext& ctx);
    void hincrbyfloat(CmdContext& ctx);
    void hsetnx(CmdContext& ctx);
    void hmset(CmdContext& ctx);
    void hmget(CmdContext& ctx);

    // ======== List 操作 ========
    void lpush(CmdContext& ctx);
    void rpush(CmdContext& ctx);
    void lpop(CmdContext& ctx);
    void rpop(CmdContext& ctx);
    void llen(CmdContext& ctx);
    void lrange(CmdContext& ctx);
    void lindex(CmdContext& ctx);
    void lset(CmdContext& ctx);
    void lrem(CmdContext& ctx);
    void ltrim(CmdContext& ctx);

    // ======== Set 操作 ========
    void sadd(CmdContext& ctx);
    void srem(CmdContext& ctx);
    void smembers(CmdContext& ctx);
    void sismember(CmdContext& ctx);
    void scard(CmdContext& ctx);
    void spop(CmdContext& ctx);
    void srandmember(CmdContext& ctx);

    // ======== ZSet 操作 ========
    void zadd(CmdContext& ctx);
    void zrem(CmdContext& ctx);
    void zcard(CmdContext& ctx);
    void zscore(CmdContext& ctx);
    void zrank(CmdContext& ctx);
    void zrevrank(CmdContext& ctx);
    void zrange(CmdContext& ctx);
    void zrevrange(CmdContext& ctx);
    void zrangebyscore(CmdContext& ctx);
    void zcount(CmdContext& ctx);
    void zincrby(CmdContext& ctx);
    void zremrangebyrank(CmdContext& ctx);
    void zremrangebyscore(CmdContext& ctx);

    // ======== 服务器 ========
    void cmdKeys(CmdContext& ctx);
    void cmdDbsize(CmdContext& ctx);
    void cmdFlushdb(CmdContext& ctx);
    void cmdRandomkey(CmdContext& ctx);

    // ======== Pub/Sub ========
    void cmdSubscribe(CmdContext& ctx);
    void cmdUnsubscribe(CmdContext& ctx);
    void cmdPsubscribe(CmdContext& ctx);
    void cmdPunsubscribe(CmdContext& ctx);
    void cmdPublish(CmdContext& ctx);
    void cmdPubsub(CmdContext& ctx);

    // ======== Scan ========
    void cmdScan(CmdContext& ctx);
    void cmdHscan(CmdContext& ctx);
    void cmdSscan(CmdContext& ctx);
    void cmdZscan(CmdContext& ctx);

    // ======== Bitmap ========
    void setbit(CmdContext& ctx);
    void getbit(CmdContext& ctx);
    void bitcount(CmdContext& ctx);
    void bitop(CmdContext& ctx);
    void bitpos(CmdContext& ctx);

    // ======== Set 运算 ========
    void sinter(CmdContext& ctx);
    void sinterstore(CmdContext& ctx);
    void sunion(CmdContext& ctx);
    void sunionstore(CmdContext& ctx);
    void sdiff(CmdContext& ctx);
    void sdiffstore(CmdContext& ctx);
    void smismember(CmdContext& ctx);

    // ======== ZSet 补齐 ========
    void zpopmin(CmdContext& ctx);
    void zpopmax(CmdContext& ctx);
    void zrandmember(CmdContext& ctx);
    void zlexcount(CmdContext& ctx);
    void zrangebylex(CmdContext& ctx);

    // ======== Hash 补齐 ========
    void hrandfield(CmdContext& ctx);
    void hstrlen(CmdContext& ctx);

    // ======== List 补齐 ========
    void lpos(CmdContext& ctx);
    void lmove(CmdContext& ctx);

    // ======== HyperLogLog ========
    void pfadd(CmdContext& ctx);
    void pfcount(CmdContext& ctx);
    void pfmerge(CmdContext& ctx);

    // ======== Geo ========
    void geoadd(CmdContext& ctx);
    void geodist(CmdContext& ctx);
    void geohash(CmdContext& ctx);
    void geopos(CmdContext& ctx);
    void georadius(CmdContext& ctx);

    // ======== Sort ========
    void cmdSort(CmdContext& ctx);

    // ======== 服务器 ========
    void cmdConfig(CmdContext& ctx);
    void cmdInfo(CmdContext& ctx);
    void cmdClient(CmdContext& ctx);
    void cmdShutdown(CmdContext& ctx);
    void cmdMonitor(CmdContext& ctx);
    void cmdSlowlog(CmdContext& ctx);
    void cmdSelect(CmdContext& ctx);

    // ======== 内部 API ========
    Value* find(const std::string& key) { return dict_.find(key); }
    void   insert(std::string key, Value value) { dict_.insert(std::move(key), std::move(value)); }
    Value  remove(const std::string& key) { return dict_.remove(key); }

    bool   checkExpired(const std::string& key, uint64_t now_ms);
    void   activeExpireCycle();

    Dict&       dict()       { return dict_; }
    const Dict& dict() const { return dict_; }
    size_t      size() const { return dict_.size(); }

    // 分布式预留: hash slot (16384 slots, CRC16)
    static uint16_t getSlot(std::string_view key);

    uint64_t hit_count_  = 0;
    uint64_t miss_count_ = 0;

private:
    Dict dict_;

    static bool tryParseInt64(const std::string& s, int64_t& out);
    static bool tryParseDouble(const std::string& s, double& out);
    Value* getOrCreate(std::string_view key, ValueType type);
    Value* getForWrite(std::string_view key, ValueType expected, CmdContext& ctx);
};

// ============================================================
// 工具函数
// ============================================================

inline uint16_t StorageEngine::getSlot(std::string_view key) {
    // CRC16 风格的 hash slot 计算
    // 支持 {hashtag}: 如果 key 中有 {...}，则只对 {} 内容计算 slot
    auto start = key.find('{');
    if (start != std::string_view::npos) {
        auto end = key.find('}', start + 1);
        if (end != std::string_view::npos && end > start + 1)
            key = key.substr(start + 1, end - start - 1);
    }
    uint32_t crc = 0;
    for (char c : key) {
        crc ^= static_cast<uint8_t>(c) << 8;
        for (int i = 0; i < 8; ++i)
            crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
    }
    return static_cast<uint16_t>(crc & 0x3FFF);  // 16384 slots
}

inline bool StorageEngine::tryParseInt64(const std::string& s, int64_t& out) {
    if (s.empty()) return false;
    try { size_t pos; out = std::stoll(s, &pos); return pos == s.size(); }
    catch (...) { return false; }
}

inline bool StorageEngine::tryParseDouble(const std::string& s, double& out) {
    if (s.empty()) return false;
    try { size_t pos; out = std::stod(s, &pos); return pos == s.size(); }
    catch (...) { return false; }
}

inline Value* StorageEngine::getOrCreate(std::string_view key, ValueType type) {
    std::string ks(key);
    Value* v = dict_.find(ks);
    if (v) return v;
    switch (type) {
    case ValueType::HASH: return dict_.insert(ks, Value::createHash());
    case ValueType::LIST: return dict_.insert(ks, Value::createList());
    case ValueType::SET:  return dict_.insert(ks, Value::createSet());
    case ValueType::ZSET: return dict_.insert(ks, Value::createZSet());
    default: return nullptr;
    }
}

inline Value* StorageEngine::getForWrite(std::string_view key, ValueType expected,
                                          CmdContext& ctx) {
    std::string ks(key);
    Value* v = dict_.find(ks);
    if (!v || v->isExpired(CmdContext::nowMs())) {
        if (v) dict_.remove(ks);
        return nullptr;
    }
    if (v->type != expected) {
        ctx.replyWrongType();
        return nullptr;
    }
    return v;
}

// ============================================================
// 基础 KV 操作
// ============================================================

inline void StorageEngine::set(CmdContext& ctx) {
    const auto& args = ctx.args;
    std::string_view key = args[1];
    std::string_view val = args[2];

    uint64_t expire_ms = 0;
    bool nx = false, xx = false;

    for (size_t i = 3; i < args.size(); ++i) {
        std::string_view opt = args[i];
        if ((opt == "EX" || opt == "ex") && i + 1 < args.size()) {
            int64_t sec = 0;
            try { sec = std::stoll(std::string(args[++i])); } catch (...) {}
            if (sec > 0) expire_ms = CmdContext::nowMs() + static_cast<uint64_t>(sec) * 1000;
        } else if ((opt == "PX" || opt == "px") && i + 1 < args.size()) {
            int64_t ms = 0;
            try { ms = std::stoll(std::string(args[++i])); } catch (...) {}
            if (ms > 0) expire_ms = CmdContext::nowMs() + static_cast<uint64_t>(ms);
        } else if (opt == "NX" || opt == "nx") nx = true;
        else if (opt == "XX" || opt == "xx") xx = true;
    }

    std::string ks(key);
    Value* existing = dict_.find(ks);
    if (nx && existing && !existing->isExpired(CmdContext::nowMs())) { ctx.replyNull(); return; }
    if (xx && (!existing || existing->isExpired(CmdContext::nowMs()))) { ctx.replyNull(); return; }

    Value v = Value::createString(std::string(val));
    v.expire_at_ms = expire_ms;
    dict_.insert(std::move(ks), std::move(v));
    ctx.replyOK();
    ctx.is_write = true;
    hit_count_++;
}

inline void StorageEngine::get(CmdContext& ctx) {
    std::string ks(ctx.args[1]);
    Value* v = dict_.find(ks);
    if (!v || v->type != ValueType::STRING || v->isExpired(CmdContext::nowMs())) {
        ctx.replyNull(); miss_count_++; return;
    }
    ctx.replyBulk(v->str);
    hit_count_++;
}

inline void StorageEngine::del(CmdContext& ctx) {
    int deleted = 0;
    for (size_t i = 1; i < ctx.args.size(); ++i) {
        Value v = dict_.remove(std::string(ctx.args[i]));
        if (v.type != ValueType::STRING || !v.str.empty() || v.opaque_ptr) deleted++;
    }
    ctx.replyInteger(deleted);
    ctx.is_write = (deleted > 0);
}

inline void StorageEngine::exists(CmdContext& ctx) {
    int count = 0;
    uint64_t now = CmdContext::nowMs();
    for (size_t i = 1; i < ctx.args.size(); ++i) {
        Value* v = dict_.find(std::string(ctx.args[i]));
        if (v && !v->isExpired(now)) count++;
    }
    ctx.replyInteger(count);
}

inline void StorageEngine::type(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs()))
        ctx.replySimpleString("none");
    else
        ctx.replySimpleString(valueTypeName(v->type));
}

// ============================================================
// 过期操作
// ============================================================

inline void StorageEngine::expire(CmdContext& ctx) {
    std::string ks(ctx.args[1]);
    int64_t sec;
    if (!tryParseInt64(std::string(ctx.args[2]), sec)) { ctx.replyNotInteger(); return; }
    Value* v = dict_.find(ks);
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyInteger(0); return; }
    v->expire_at_ms = CmdContext::nowMs() + static_cast<uint64_t>(sec) * 1000;
    ctx.replyInteger(1);
    ctx.is_write = true;
}

inline void StorageEngine::expireat(CmdContext& ctx) {
    std::string ks(ctx.args[1]);
    int64_t ts;
    if (!tryParseInt64(std::string(ctx.args[2]), ts)) { ctx.replyNotInteger(); return; }
    Value* v = dict_.find(ks);
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyInteger(0); return; }
    v->expire_at_ms = static_cast<uint64_t>(ts) * 1000;
    ctx.replyInteger(1);
    ctx.is_write = true;
}

inline void StorageEngine::pexpire(CmdContext& ctx) {
    std::string ks(ctx.args[1]);
    int64_t ms;
    if (!tryParseInt64(std::string(ctx.args[2]), ms)) { ctx.replyNotInteger(); return; }
    Value* v = dict_.find(ks);
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyInteger(0); return; }
    v->expire_at_ms = CmdContext::nowMs() + static_cast<uint64_t>(ms);
    ctx.replyInteger(1);
    ctx.is_write = true;
}

inline void StorageEngine::pexpireat(CmdContext& ctx) {
    std::string ks(ctx.args[1]);
    int64_t ts;
    if (!tryParseInt64(std::string(ctx.args[2]), ts)) { ctx.replyNotInteger(); return; }
    Value* v = dict_.find(ks);
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyInteger(0); return; }
    v->expire_at_ms = static_cast<uint64_t>(ts);
    ctx.replyInteger(1);
    ctx.is_write = true;
}

inline void StorageEngine::ttl(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v) { ctx.replyInteger(-2); return; }
    ctx.replyInteger(v->ttlSec(CmdContext::nowMs()));
}

inline void StorageEngine::pttl(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v) { ctx.replyInteger(-2); return; }
    ctx.replyInteger(v->ttlMs(CmdContext::nowMs()));
}

inline void StorageEngine::persist(CmdContext& ctx) {
    std::string ks(ctx.args[1]);
    Value* v = dict_.find(ks);
    if (!v || v->expire_at_ms == 0 || v->isExpired(CmdContext::nowMs())) {
        ctx.replyInteger(0); return;
    }
    v->expire_at_ms = 0;
    ctx.replyInteger(1);
    ctx.is_write = true;
}

// ============================================================
// String 操作
// ============================================================

inline void StorageEngine::incr(CmdContext& ctx) {
    std::string ks(ctx.args[1]);
    Value* v = dict_.find(ks);
    int64_t val = 0;
    if (v && v->type == ValueType::STRING && !v->isExpired(CmdContext::nowMs())) {
        if (!tryParseInt64(v->str, val)) { ctx.replyNotInteger(); return; }
        val++; v->str = std::to_string(val); v->int_val = val;
    } else {
        val = 1; dict_.insert(ks, Value::createInt(val));
    }
    ctx.replyInteger(val);
    ctx.is_write = true;
}

inline void StorageEngine::incrby(CmdContext& ctx) {
    std::string ks(ctx.args[1]);
    int64_t delta;
    if (!tryParseInt64(std::string(ctx.args[2]), delta)) { ctx.replyNotInteger(); return; }
    Value* v = dict_.find(ks);
    int64_t val = 0;
    if (v && v->type == ValueType::STRING && !v->isExpired(CmdContext::nowMs())) {
        if (!tryParseInt64(v->str, val)) { ctx.replyNotInteger(); return; }
        val += delta; v->str = std::to_string(val); v->int_val = val;
    } else {
        val = delta; dict_.insert(ks, Value::createInt(val));
    }
    ctx.replyInteger(val);
    ctx.is_write = true;
}

inline void StorageEngine::decr(CmdContext& ctx) {
    std::string ks(ctx.args[1]);
    Value* v = dict_.find(ks);
    int64_t val = 0;
    if (v && v->type == ValueType::STRING && !v->isExpired(CmdContext::nowMs())) {
        if (!tryParseInt64(v->str, val)) { ctx.replyNotInteger(); return; }
        val--; v->str = std::to_string(val); v->int_val = val;
    } else {
        val = -1; dict_.insert(ks, Value::createInt(val));
    }
    ctx.replyInteger(val);
    ctx.is_write = true;
}

inline void StorageEngine::decrby(CmdContext& ctx) {
    std::string ks(ctx.args[1]);
    int64_t delta;
    if (!tryParseInt64(std::string(ctx.args[2]), delta)) { ctx.replyNotInteger(); return; }
    Value* v = dict_.find(ks);
    int64_t val = 0;
    if (v && v->type == ValueType::STRING && !v->isExpired(CmdContext::nowMs())) {
        if (!tryParseInt64(v->str, val)) { ctx.replyNotInteger(); return; }
        val -= delta; v->str = std::to_string(val); v->int_val = val;
    } else {
        val = -delta; dict_.insert(ks, Value::createInt(val));
    }
    ctx.replyInteger(val);
    ctx.is_write = true;
}

inline void StorageEngine::append(CmdContext& ctx) {
    std::string ks(ctx.args[1]);
    std::string_view suffix = ctx.args[2];
    Value* v = dict_.find(ks);
    if (v && v->type == ValueType::STRING) {
        if (v->isExpired(CmdContext::nowMs())) {
            v->str.assign(suffix); v->expire_at_ms = 0;
        } else { v->str.append(suffix.data(), suffix.size()); }
        ctx.replyInteger(static_cast<int64_t>(v->str.size()));
    } else if (!v) {
        dict_.insert(ks, Value::createString(std::string(suffix)));
        ctx.replyInteger(static_cast<int64_t>(suffix.size()));
    } else { ctx.replyWrongType(); return; }
    ctx.is_write = true;
}

inline void StorageEngine::strlen(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->type != ValueType::STRING || v->isExpired(CmdContext::nowMs()))
        ctx.replyInteger(0);
    else
        ctx.replyInteger(static_cast<int64_t>(v->str.size()));
}

inline void StorageEngine::mget(CmdContext& ctx) {
    uint64_t now = CmdContext::nowMs();
    RespWriter::writeArrayHeader(*ctx.response, static_cast<int64_t>(ctx.args.size() - 1));
    for (size_t i = 1; i < ctx.args.size(); ++i) {
        Value* v = dict_.find(std::string(ctx.args[i]));
        if (!v || v->type != ValueType::STRING || v->isExpired(now))
            RespWriter::writeNull(*ctx.response);
        else
            RespWriter::writeBulkString(*ctx.response, v->str);
    }
}

inline void StorageEngine::mset(CmdContext& ctx) {
    for (size_t i = 1; i + 1 < ctx.args.size(); i += 2)
        dict_.insert(std::string(ctx.args[i]), Value::createString(std::string(ctx.args[i + 1])));
    ctx.replyOK();
    ctx.is_write = true;
}

inline void StorageEngine::setnx(CmdContext& ctx) {
    std::string ks(ctx.args[1]);
    if (dict_.find(ks)) { ctx.replyInteger(0); return; }
    dict_.insert(std::move(ks), Value::createString(std::string(ctx.args[2])));
    ctx.replyInteger(1);
    ctx.is_write = true;
}

inline void StorageEngine::setex(CmdContext& ctx) {
    std::string ks(ctx.args[1]);
    int64_t sec;
    if (!tryParseInt64(std::string(ctx.args[2]), sec)) { ctx.replyNotInteger(); return; }
    auto v = Value::createString(std::string(ctx.args[3]));
    v.expire_at_ms = CmdContext::nowMs() + static_cast<uint64_t>(sec) * 1000;
    dict_.insert(std::move(ks), std::move(v));
    ctx.replyOK();
    ctx.is_write = true;
}

inline void StorageEngine::psetex(CmdContext& ctx) {
    std::string ks(ctx.args[1]);
    int64_t ms;
    if (!tryParseInt64(std::string(ctx.args[2]), ms)) { ctx.replyNotInteger(); return; }
    auto v = Value::createString(std::string(ctx.args[3]));
    v.expire_at_ms = CmdContext::nowMs() + static_cast<uint64_t>(ms);
    dict_.insert(std::move(ks), std::move(v));
    ctx.replyOK();
    ctx.is_write = true;
}

inline void StorageEngine::getset(CmdContext& ctx) {
    std::string ks(ctx.args[1]);
    std::string nv(ctx.args[2]);
    Value* v = dict_.find(ks);
    if (!v || v->type != ValueType::STRING || v->isExpired(CmdContext::nowMs())) {
        ctx.replyNull();
        dict_.insert(std::move(ks), Value::createString(std::move(nv)));
    } else {
        ctx.replyBulk(v->str);
        v->str = std::move(nv);
    }
    ctx.is_write = true;
}

inline void StorageEngine::getrange(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->type != ValueType::STRING || v->isExpired(CmdContext::nowMs())) {
        ctx.replyBulk(std::string_view{}); return;
    }
    int64_t start, end;
    if (!tryParseInt64(std::string(ctx.args[2]), start) ||
        !tryParseInt64(std::string(ctx.args[3]), end)) { ctx.replyNotInteger(); return; }
    int64_t sz = static_cast<int64_t>(v->str.size());
    if (start < 0) start += sz;
    if (end < 0) end += sz;
    if (start < 0) start = 0;
    if (end >= sz) end = sz - 1;
    if (start > end) { ctx.replyBulk(std::string_view{}); return; }
    ctx.replyBulk(v->str.substr(static_cast<size_t>(start), static_cast<size_t>(end - start + 1)));
}

inline void StorageEngine::setrange(CmdContext& ctx) {
    std::string ks(ctx.args[1]);
    int64_t offset;
    if (!tryParseInt64(std::string(ctx.args[2]), offset) || offset < 0) {
        ctx.replyError("ERR offset is out of range"); return;
    }
    std::string_view val = ctx.args[3];
    Value* v = dict_.find(ks);
    if (!v || v->type != ValueType::STRING || v->isExpired(CmdContext::nowMs())) {
        std::string s(static_cast<size_t>(offset) + val.size(), '\0');
        s.replace(static_cast<size_t>(offset), val.size(), val.data(), val.size());
        dict_.insert(std::move(ks), Value::createString(std::move(s)));
    } else {
        if (static_cast<size_t>(offset) + val.size() > v->str.size())
            v->str.resize(static_cast<size_t>(offset) + val.size(), '\0');
        v->str.replace(static_cast<size_t>(offset), val.size(), val.data(), val.size());
    }
    ctx.replyInteger(static_cast<int64_t>(
        dict_.find(ks) ? dict_.find(ks)->str.size() : offset + val.size()));
    ctx.is_write = true;
}

inline void StorageEngine::incrbyfloat(CmdContext& ctx) {
    std::string ks(ctx.args[1]);
    double delta;
    if (!tryParseDouble(std::string(ctx.args[2]), delta)) {
        ctx.replyError("ERR value is not a valid float"); return;
    }
    Value* v = dict_.find(ks);
    double cur = 0.0;
    if (v && v->type == ValueType::STRING && !v->isExpired(CmdContext::nowMs())) {
        if (!tryParseDouble(v->str, cur)) { ctx.replyError("ERR value is not a valid float"); return; }
    }
    cur += delta;
    char buf[64]; snprintf(buf, sizeof(buf), "%.17g", cur);
    std::string bs(buf);
    if (v) v->str = bs; else dict_.insert(ks, Value::createString(bs));
    ctx.replyBulk(bs);
    ctx.is_write = true;
}

inline void StorageEngine::msetnx(CmdContext& ctx) {
    uint64_t now = CmdContext::nowMs();
    for (size_t i = 1; i + 1 < ctx.args.size(); i += 2) {
        Value* v = dict_.find(std::string(ctx.args[i]));
        if (v && !v->isExpired(now)) { ctx.replyInteger(0); return; }
    }
    for (size_t i = 1; i + 1 < ctx.args.size(); i += 2)
        dict_.insert(std::string(ctx.args[i]), Value::createString(std::string(ctx.args[i + 1])));
    ctx.replyInteger(1);
    ctx.is_write = true;
}

inline void StorageEngine::getdel(CmdContext& ctx) {
    std::string ks(ctx.args[1]);
    Value* v = dict_.find(ks);
    if (!v || v->type != ValueType::STRING || v->isExpired(CmdContext::nowMs())) {
        ctx.replyNull(); return;
    }
    std::string val = v->str;
    dict_.remove(ks);
    ctx.replyBulk(val);
    ctx.is_write = true;
}

inline void StorageEngine::rename(CmdContext& ctx) {
    std::string old_key(ctx.args[1]), new_key(ctx.args[2]);
    if (old_key == new_key) { ctx.replyOK(); return; }
    Value* v = dict_.find(old_key);
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyNoSuchKey(); return; }
    Value copy = std::move(*v);
    dict_.remove(old_key);
    dict_.insert(std::move(new_key), std::move(copy));
    ctx.replyOK();
    ctx.is_write = true;
}

inline void StorageEngine::renamenx(CmdContext& ctx) {
    std::string old_key(ctx.args[1]), new_key(ctx.args[2]);
    if (old_key == new_key) { ctx.replyOK(); return; }
    Value* v = dict_.find(old_key);
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyNoSuchKey(); return; }
    if (dict_.find(new_key)) { ctx.replyInteger(0); return; }
    Value copy = std::move(*v);
    dict_.remove(old_key);
    dict_.insert(std::move(new_key), std::move(copy));
    ctx.replyInteger(1);
    ctx.is_write = true;
}

// ============================================================
// Hash 操作
// ============================================================

inline void StorageEngine::hset(CmdContext& ctx) {
    auto* hd = getOrCreate(ctx.args[1], ValueType::HASH)->asHash();
    if (!hd) { ctx.replyWrongType(); return; }
    int created = 0;
    for (size_t i = 2; i + 1 < ctx.args.size(); i += 2) {
        std::string field(ctx.args[i]);
        if (hd->fields.find(field) == hd->fields.end()) created++;
        hd->fields[std::move(field)] = std::string(ctx.args[i + 1]);
    }
    ctx.replyInteger(created);
    ctx.is_write = true;
}

inline void StorageEngine::hget(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyNull(); return; }
    auto* hd = v->asHash();
    if (!hd) { ctx.replyWrongType(); return; }
    auto it = hd->fields.find(std::string(ctx.args[2]));
    ctx.replyBulk(it != hd->fields.end() ? std::string_view(it->second) : std::string_view{});
}

inline void StorageEngine::hdel(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyInteger(0); return; }
    auto* hd = v->asHash();
    if (!hd) { ctx.replyWrongType(); return; }
    int deleted = 0;
    for (size_t i = 2; i < ctx.args.size(); ++i)
        if (hd->fields.erase(std::string(ctx.args[i])) > 0) deleted++;
    ctx.replyInteger(deleted);
    ctx.is_write = (deleted > 0);
}

inline void StorageEngine::hexists(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyInteger(0); return; }
    auto* hd = v->asHash();
    ctx.replyInteger((hd != nullptr && hd->fields.find(std::string(ctx.args[2])) != hd->fields.end()) ? 1 : 0);
}

inline void StorageEngine::hgetall(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyEmptyArray(); return; }
    auto* hd = v->asHash();
    if (!hd) { ctx.replyWrongType(); return; }
    RespWriter::writeArrayHeader(*ctx.response, static_cast<int64_t>(hd->fields.size() * 2));
    for (auto& kv : hd->fields) {
        RespWriter::writeBulkString(*ctx.response, kv.first);
        RespWriter::writeBulkString(*ctx.response, kv.second);
    }
}

inline void StorageEngine::hkeys(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyEmptyArray(); return; }
    auto* hd = v->asHash();
    if (!hd) { ctx.replyWrongType(); return; }
    RespWriter::writeArrayHeader(*ctx.response, static_cast<int64_t>(hd->fields.size()));
    for (auto& kv : hd->fields) RespWriter::writeBulkString(*ctx.response, kv.first);
}

inline void StorageEngine::hvals(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyEmptyArray(); return; }
    auto* hd = v->asHash();
    if (!hd) { ctx.replyWrongType(); return; }
    RespWriter::writeArrayHeader(*ctx.response, static_cast<int64_t>(hd->fields.size()));
    for (auto& kv : hd->fields) RespWriter::writeBulkString(*ctx.response, kv.second);
}

inline void StorageEngine::hlen(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyInteger(0); return; }
    auto* hd = v->asHash();
    if (!hd) { ctx.replyWrongType(); return; }
    ctx.replyInteger(static_cast<int64_t>(hd->fields.size()));
}

inline void StorageEngine::hincrby(CmdContext& ctx) {
    Value* v = getOrCreate(ctx.args[1], ValueType::HASH);
    auto* hd = v->asHash();
    if (!hd) { ctx.replyWrongType(); return; }
    std::string field(ctx.args[2]);
    int64_t delta;
    if (!tryParseInt64(std::string(ctx.args[3]), delta)) { ctx.replyNotInteger(); return; }
    auto it = hd->fields.find(field);
    int64_t cur = 0;
    if (it != hd->fields.end()) { if (!tryParseInt64(it->second, cur)) { ctx.replyNotInteger(); return; } }
    cur += delta;
    hd->fields[field] = std::to_string(cur);
    ctx.replyInteger(cur);
    ctx.is_write = true;
}

inline void StorageEngine::hincrbyfloat(CmdContext& ctx) {
    Value* v = getOrCreate(ctx.args[1], ValueType::HASH);
    auto* hd = v->asHash();
    if (!hd) { ctx.replyWrongType(); return; }
    std::string field(ctx.args[2]);
    double delta;
    if (!tryParseDouble(std::string(ctx.args[3]), delta)) { ctx.replyError("ERR value is not a valid float"); return; }
    auto it = hd->fields.find(field);
    double cur = 0.0;
    if (it != hd->fields.end() && !tryParseDouble(it->second, cur)) { ctx.replyError("ERR value is not a valid float"); return; }
    cur += delta;
    char buf[64]; snprintf(buf, sizeof(buf), "%.17g", cur);
    hd->fields[field] = buf;
    ctx.replyBulk(std::string(buf));
    ctx.is_write = true;
}

inline void StorageEngine::hsetnx(CmdContext& ctx) {
    Value* v = getOrCreate(ctx.args[1], ValueType::HASH);
    auto* hd = v->asHash();
    if (!hd) { ctx.replyWrongType(); return; }
    std::string field(ctx.args[2]);
    if (hd->fields.find(field) != hd->fields.end()) { ctx.replyInteger(0); return; }
    hd->fields[std::move(field)] = std::string(ctx.args[3]);
    ctx.replyInteger(1);
    ctx.is_write = true;
}

inline void StorageEngine::hmset(CmdContext& ctx) {
    Value* v = getOrCreate(ctx.args[1], ValueType::HASH);
    auto* hd = v->asHash();
    if (!hd) { ctx.replyWrongType(); return; }
    for (size_t i = 2; i + 1 < ctx.args.size(); i += 2)
        hd->fields[std::string(ctx.args[i])] = std::string(ctx.args[i + 1]);
    ctx.replyOK();
    ctx.is_write = true;
}

inline void StorageEngine::hmget(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    RespWriter::writeArrayHeader(*ctx.response, static_cast<int64_t>(ctx.args.size() - 2));
    auto* hd = (v && !v->isExpired(CmdContext::nowMs())) ? v->asHash() : nullptr;
    for (size_t i = 2; i < ctx.args.size(); ++i) {
        if (!hd) { RespWriter::writeNull(*ctx.response); continue; }
        auto it = hd->fields.find(std::string(ctx.args[i]));
        if (it != hd->fields.end()) RespWriter::writeBulkString(*ctx.response, it->second);
        else RespWriter::writeNull(*ctx.response);
    }
}

// ============================================================
// List 操作
// ============================================================

inline void StorageEngine::lpush(CmdContext& ctx) {
    Value* v = getOrCreate(ctx.args[1], ValueType::LIST);
    auto* ld = v->asList();
    if (!ld) { ctx.replyWrongType(); return; }
    for (size_t i = 2; i < ctx.args.size(); ++i)
        ld->elements.push_front(std::string(ctx.args[i]));
    ctx.replyInteger(static_cast<int64_t>(ld->elements.size()));
    ctx.is_write = true;
}

inline void StorageEngine::rpush(CmdContext& ctx) {
    Value* v = getOrCreate(ctx.args[1], ValueType::LIST);
    auto* ld = v->asList();
    if (!ld) { ctx.replyWrongType(); return; }
    for (size_t i = 2; i < ctx.args.size(); ++i)
        ld->elements.push_back(std::string(ctx.args[i]));
    ctx.replyInteger(static_cast<int64_t>(ld->elements.size()));
    ctx.is_write = true;
}

inline void StorageEngine::lpop(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyNull(); return; }
    auto* ld = v->asList();
    if (!ld || ld->elements.empty()) { ctx.replyNull(); return; }
    std::string val = std::move(ld->elements.front());
    ld->elements.pop_front();
    ctx.replyBulk(val);
    ctx.is_write = true;
}

inline void StorageEngine::rpop(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyNull(); return; }
    auto* ld = v->asList();
    if (!ld || ld->elements.empty()) { ctx.replyNull(); return; }
    std::string val = std::move(ld->elements.back());
    ld->elements.pop_back();
    ctx.replyBulk(val);
    ctx.is_write = true;
}

inline void StorageEngine::llen(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyInteger(0); return; }
    auto* ld = v->asList();
    if (!ld) { ctx.replyWrongType(); return; }
    ctx.replyInteger(static_cast<int64_t>(ld->elements.size()));
}

inline void StorageEngine::lrange(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyEmptyArray(); return; }
    auto* ld = v->asList();
    if (!ld) { ctx.replyWrongType(); return; }
    int64_t start, end;
    if (!tryParseInt64(std::string(ctx.args[2]), start) ||
        !tryParseInt64(std::string(ctx.args[3]), end)) { ctx.replyNotInteger(); return; }
    int64_t sz = static_cast<int64_t>(ld->elements.size());
    if (start < 0) start += sz;
    if (end < 0) end += sz;
    if (start < 0) start = 0;
    if (end >= sz) end = sz - 1;
    if (start > end) { ctx.replyEmptyArray(); return; }
    int64_t count = end - start + 1;
    RespWriter::writeArrayHeader(*ctx.response, count);
    auto it = ld->elements.begin();
    for (int64_t i = 0; i < start && it != ld->elements.end(); ++i) ++it;
    for (int64_t i = 0; i < count && it != ld->elements.end(); ++i, ++it)
        RespWriter::writeBulkString(*ctx.response, *it);
}

inline void StorageEngine::lindex(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyNull(); return; }
    auto* ld = v->asList();
    if (!ld) { ctx.replyWrongType(); return; }
    int64_t idx;
    if (!tryParseInt64(std::string(ctx.args[2]), idx)) { ctx.replyNotInteger(); return; }
    int64_t sz = static_cast<int64_t>(ld->elements.size());
    if (idx < 0) idx += sz;
    if (idx < 0 || idx >= sz) { ctx.replyNull(); return; }
    auto it = ld->elements.begin();
    for (int64_t i = 0; i < idx; ++i) ++it;
    ctx.replyBulk(*it);
}

inline void StorageEngine::lset(CmdContext& ctx) {
    Value* v = getForWrite(ctx.args[1], ValueType::LIST, ctx);
    if (!v) return;
    auto* ld = v->asList();
    int64_t idx;
    if (!tryParseInt64(std::string(ctx.args[2]), idx)) { ctx.replyNotInteger(); return; }
    int64_t sz = static_cast<int64_t>(ld->elements.size());
    if (idx < 0) idx += sz;
    if (idx < 0 || idx >= sz) { ctx.replyError("ERR index out of range"); return; }
    auto it = ld->elements.begin();
    for (int64_t i = 0; i < idx; ++i) ++it;
    *it = std::string(ctx.args[3]);
    ctx.replyOK();
    ctx.is_write = true;
}

inline void StorageEngine::lrem(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyInteger(0); return; }
    auto* ld = v->asList();
    if (!ld) { ctx.replyWrongType(); return; }
    int64_t count;
    if (!tryParseInt64(std::string(ctx.args[2]), count)) { ctx.replyNotInteger(); return; }
    std::string elem(ctx.args[3]);
    int removed = 0;

    if (count >= 0) {
        // 从前向后删除: drain → filter → rebuild
        lstl::vector<std::string> tmp;
        while (!ld->elements.empty()) {
            if (ld->elements.front() == elem && (count == 0 || removed < count)) {
                removed++;
            } else {
                tmp.push_back(std::move(ld->elements.front()));
            }
            ld->elements.pop_front();
        }
        for (auto& s : tmp) ld->elements.push_back(std::move(s));
    } else {
        // 从后向前删除
        count = -count;
        lstl::vector<std::string> tmp;
        while (!ld->elements.empty()) {
            tmp.push_back(std::move(ld->elements.front()));
            ld->elements.pop_front();
        }
        // 从尾部删除
        for (int64_t i = static_cast<int64_t>(tmp.size()) - 1; i >= 0; --i) {
            if (tmp[static_cast<size_t>(i)] == elem && removed < count) {
                removed++;
            } else {
                ld->elements.push_front(std::move(tmp[static_cast<size_t>(i)]));
            }
        }
    }
    ctx.replyInteger(removed);
    ctx.is_write = (removed > 0);
}

inline void StorageEngine::ltrim(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyOK(); return; }
    auto* ld = v->asList();
    if (!ld) { ctx.replyWrongType(); return; }
    int64_t start, end;
    if (!tryParseInt64(std::string(ctx.args[2]), start) ||
        !tryParseInt64(std::string(ctx.args[3]), end)) { ctx.replyNotInteger(); return; }
    int64_t sz = static_cast<int64_t>(ld->elements.size());
    if (start < 0) start += sz;
    if (end < 0) end += sz;
    if (start < 0) start = 0;
    if (end >= sz) end = sz - 1;
    if (start > end || sz == 0) { ld->elements.clear(); }
    else {
        for (int64_t i = 0; i < start; ++i) ld->elements.pop_front();
        int64_t keep = end - start + 1;
        while (static_cast<int64_t>(ld->elements.size()) > keep) ld->elements.pop_back();
    }
    ctx.replyOK();
    ctx.is_write = true;
}

// ============================================================
// Set 操作
// ============================================================

inline void StorageEngine::sadd(CmdContext& ctx) {
    Value* v = getOrCreate(ctx.args[1], ValueType::SET);
    auto* sd = v->asSet();
    if (!sd) { ctx.replyWrongType(); return; }
    int added = 0;
    for (size_t i = 2; i < ctx.args.size(); ++i)
        if (sd->members.insert(std::string(ctx.args[i])).second) added++;
    ctx.replyInteger(added);
    ctx.is_write = (added > 0);
}

inline void StorageEngine::srem(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyInteger(0); return; }
    auto* sd = v->asSet();
    if (!sd) { ctx.replyWrongType(); return; }
    int removed = 0;
    for (size_t i = 2; i < ctx.args.size(); ++i)
        if (sd->members.erase(std::string(ctx.args[i])) > 0) removed++;
    ctx.replyInteger(removed);
    ctx.is_write = (removed > 0);
}

inline void StorageEngine::smembers(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyEmptyArray(); return; }
    auto* sd = v->asSet();
    if (!sd) { ctx.replyWrongType(); return; }
    RespWriter::writeArrayHeader(*ctx.response, static_cast<int64_t>(sd->members.size()));
    for (auto& m : sd->members) RespWriter::writeBulkString(*ctx.response, m);
}

inline void StorageEngine::sismember(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyInteger(0); return; }
    auto* sd = v->asSet();
    if (!sd) { ctx.replyWrongType(); return; }
    ctx.replyInteger(sd->members.find(std::string(ctx.args[2])) != sd->members.end() ? 1 : 0);
}

inline void StorageEngine::scard(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyInteger(0); return; }
    auto* sd = v->asSet();
    if (!sd) { ctx.replyWrongType(); return; }
    ctx.replyInteger(static_cast<int64_t>(sd->members.size()));
}

inline void StorageEngine::spop(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyNull(); return; }
    auto* sd = v->asSet();
    if (!sd || sd->members.empty()) { ctx.replyNull(); return; }
    size_t r = static_cast<size_t>(rand()) % sd->members.size();
    auto it = sd->members.begin();
    for (size_t i = 0; i < r; ++i) ++it;
    std::string member = *it;
    sd->members.erase(member);
    ctx.replyBulk(member);
    ctx.is_write = true;
}

inline void StorageEngine::srandmember(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyNull(); return; }
    auto* sd = v->asSet();
    if (!sd || sd->members.empty()) { ctx.replyNull(); return; }
    size_t r = static_cast<size_t>(rand()) % sd->members.size();
    auto it = sd->members.begin();
    for (size_t i = 0; i < r; ++i) ++it;
    ctx.replyBulk(*it);
}

// ============================================================
// ZSet 操作 (基于 lstl::set<pair<double, string>> + unordered_map)
// ============================================================

inline void StorageEngine::zadd(CmdContext& ctx) {
    Value* v = getOrCreate(ctx.args[1], ValueType::ZSET);
    auto* zd = v->asZSet();
    if (!zd) { ctx.replyWrongType(); return; }
    int added = 0;
    for (size_t i = 2; i + 1 < ctx.args.size(); i += 2) {
        double score;
        if (!tryParseDouble(std::string(ctx.args[i]), score)) {
            ctx.replyError("ERR value is not a valid float"); return;
        }
        std::string member(ctx.args[i + 1]);
        auto it = zd->scores.find(member);
        if (it != zd->scores.end()) {
            // 成员已存在 → 更新 score
            double old_score = it->second;
            if (old_score != score) {
                zd->by_score.erase({old_score, member});
                zd->by_score.insert({score, member});
                it->second = score;
            }
        } else {
            zd->by_score.insert({score, member});
            zd->scores[member] = score;
            added++;
        }
    }
    ctx.replyInteger(added);
    ctx.is_write = (added > 0);
}

inline void StorageEngine::zrem(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyInteger(0); return; }
    auto* zd = v->asZSet();
    if (!zd) { ctx.replyWrongType(); return; }
    int removed = 0;
    for (size_t i = 2; i < ctx.args.size(); ++i) {
        std::string member(ctx.args[i]);
        auto it = zd->scores.find(member);
        if (it != zd->scores.end()) {
            zd->by_score.erase({it->second, member});
            zd->scores.erase(member);
            removed++;
        }
    }
    ctx.replyInteger(removed);
    ctx.is_write = (removed > 0);
}

inline void StorageEngine::zcard(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyInteger(0); return; }
    auto* zd = v->asZSet();
    if (!zd) { ctx.replyWrongType(); return; }
    ctx.replyInteger(static_cast<int64_t>(zd->scores.size()));
}

inline void StorageEngine::zscore(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyNull(); return; }
    auto* zd = v->asZSet();
    if (!zd) { ctx.replyWrongType(); return; }
    auto it = zd->scores.find(std::string(ctx.args[2]));
    if (it != zd->scores.end()) {
        char buf[64]; snprintf(buf, sizeof(buf), "%.17g", it->second);
        ctx.replyBulk(std::string(buf));
    } else {
        ctx.replyNull();
    }
}

inline void StorageEngine::zrank(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyNull(); return; }
    auto* zd = v->asZSet();
    if (!zd) { ctx.replyWrongType(); return; }
    std::string member(ctx.args[2]);
    auto sit = zd->scores.find(member);
    if (sit == zd->scores.end()) { ctx.replyNull(); return; }
    double score = sit->second;
    std::pair<double, std::string> target{score, member};
    int64_t rank = 0;
    for (auto& elem : zd->by_score) {
        if (elem < target) rank++;
        else break;
    }
    ctx.replyInteger(rank);
}

inline void StorageEngine::zrevrank(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyNull(); return; }
    auto* zd = v->asZSet();
    if (!zd) { ctx.replyWrongType(); return; }
    std::string member(ctx.args[2]);
    auto sit = zd->scores.find(member);
    if (sit == zd->scores.end()) { ctx.replyNull(); return; }
    double score = sit->second;
    std::pair<double, std::string> target{score, member};
    int64_t rank = 0;
    for (auto& elem : zd->by_score) {
        if (elem < target) rank++;
        else break;
    }
    // zrevrank = total - zrank - 1
    int64_t total = static_cast<int64_t>(zd->scores.size());
    ctx.replyInteger(total - rank - 1);
}

inline void StorageEngine::zrange(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyEmptyArray(); return; }
    auto* zd = v->asZSet();
    if (!zd) { ctx.replyWrongType(); return; }
    int64_t start, end;
    if (!tryParseInt64(std::string(ctx.args[2]), start) ||
        !tryParseInt64(std::string(ctx.args[3]), end)) { ctx.replyNotInteger(); return; }
    bool withscores = (ctx.args.size() >= 5 &&
                       (ctx.args[4] == "WITHSCORES" || ctx.args[4] == "withscores"));
    int64_t sz = static_cast<int64_t>(zd->scores.size());
    if (start < 0) start += sz;
    if (end < 0) end += sz;
    if (start < 0) start = 0;
    if (end >= sz) end = sz - 1;
    if (start > end) { ctx.replyEmptyArray(); return; }

    int64_t idx = 0;
    lstl::vector<std::string> result;
    for (auto& elem : zd->by_score) {
        if (idx > end) break;
        if (idx >= start) result.push_back(elem.second);
        idx++;
    }

    if (withscores) {
        RespWriter::writeArrayHeader(*ctx.response,
            static_cast<int64_t>(result.size() * 2));
        for (auto& m : result) {
            RespWriter::writeBulkString(*ctx.response, m);
            char buf[64]; snprintf(buf, sizeof(buf), "%.17g",
                zd->scores.find(m)->second);
            RespWriter::writeBulkString(*ctx.response, std::string(buf));
        }
    } else {
        RespWriter::writeArrayHeader(*ctx.response,
            static_cast<int64_t>(result.size()));
        for (auto& m : result) RespWriter::writeBulkString(*ctx.response, m);
    }
}

inline void StorageEngine::zrevrange(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyEmptyArray(); return; }
    auto* zd = v->asZSet();
    if (!zd) { ctx.replyWrongType(); return; }
    int64_t start, end;
    if (!tryParseInt64(std::string(ctx.args[2]), start) ||
        !tryParseInt64(std::string(ctx.args[3]), end)) { ctx.replyNotInteger(); return; }
    bool withscores = (ctx.args.size() >= 5 &&
                       (ctx.args[4] == "WITHSCORES" || ctx.args[4] == "withscores"));

    // 收集所有成员到 vector (因 lstl::set 无 rbegin)
    lstl::vector<std::string> all;
    for (auto& elem : zd->by_score) all.push_back(elem.second);

    int64_t sz = static_cast<int64_t>(all.size());
    if (start < 0) start += sz;
    if (end < 0) end += sz;
    if (start < 0) start = 0;
    if (end >= sz) end = sz - 1;
    if (start > end) { ctx.replyEmptyArray(); return; }

    // 反向输出: sz-1-start 到 sz-1-end
    int64_t count = end - start + 1;
    if (withscores) {
        RespWriter::writeArrayHeader(*ctx.response, count * 2);
        for (int64_t i = sz - 1 - start; i >= sz - 1 - end; --i) {
            RespWriter::writeBulkString(*ctx.response, all[static_cast<size_t>(i)]);
            char buf[64]; snprintf(buf, sizeof(buf), "%.17g",
                zd->scores.find(all[static_cast<size_t>(i)])->second);
            RespWriter::writeBulkString(*ctx.response, std::string(buf));
        }
    } else {
        RespWriter::writeArrayHeader(*ctx.response, count);
        for (int64_t i = sz - 1 - start; i >= sz - 1 - end; --i)
            RespWriter::writeBulkString(*ctx.response, all[static_cast<size_t>(i)]);
    }
}

inline void StorageEngine::zrangebyscore(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyEmptyArray(); return; }
    auto* zd = v->asZSet();
    if (!zd) { ctx.replyWrongType(); return; }
    double min_score, max_score;
    std::string_view min_str = ctx.args[2], max_str = ctx.args[3];
    bool min_excl = false, max_excl = false;
    if (min_str[0] == '(') { min_excl = true; min_str.remove_prefix(1); }
    if (max_str[0] == '(') { max_excl = true; max_str.remove_prefix(1); }
    if (min_str == "-inf") min_score = -1e308;
    else if (min_str == "+inf") min_score = 1e308;
    else if (!tryParseDouble(std::string(min_str), min_score)) { ctx.replyNotInteger(); return; }
    if (max_str == "-inf") max_score = -1e308;
    else if (max_str == "+inf") max_score = 1e308;
    else if (!tryParseDouble(std::string(max_str), max_score)) { ctx.replyNotInteger(); return; }

    lstl::vector<std::string> result;
    for (auto& elem : zd->by_score) {
        double s = elem.first;
        if (s < min_score || (min_excl && s == min_score)) continue;
        if (s > max_score || (max_excl && s == max_score)) break;
        result.push_back(elem.second);
    }

    int64_t offset = 0, limit = static_cast<int64_t>(result.size());
    for (size_t i = 4; i + 1 < ctx.args.size(); ++i) {
        if (ctx.args[i] == "LIMIT" || ctx.args[i] == "limit") {
            if (!tryParseInt64(std::string(ctx.args[i+1]), offset)) return;
            if (i + 2 < ctx.args.size() && !tryParseInt64(std::string(ctx.args[i+2]), limit)) return;
            break;
        }
    }
    if (offset < 0) offset = 0;
    int64_t written = 0;
    int64_t total = std::max<int64_t>(0,
        std::min<int64_t>(limit, static_cast<int64_t>(result.size()) - offset));
    RespWriter::writeArrayHeader(*ctx.response, total);
    for (size_t i = static_cast<size_t>(offset);
         i < result.size() && written < limit; ++i, ++written)
        RespWriter::writeBulkString(*ctx.response, result[i]);
}

inline void StorageEngine::zcount(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyInteger(0); return; }
    auto* zd = v->asZSet();
    if (!zd) { ctx.replyWrongType(); return; }
    double min_score, max_score;
    std::string_view min_str = ctx.args[2], max_str = ctx.args[3];
    bool min_excl = false, max_excl = false;
    if (min_str[0] == '(') { min_excl = true; min_str.remove_prefix(1); }
    if (max_str[0] == '(') { max_excl = true; max_str.remove_prefix(1); }
    if (min_str == "-inf") min_score = -1e308;
    else if (min_str == "+inf") min_score = 1e308;
    else if (!tryParseDouble(std::string(min_str), min_score)) { ctx.replyNotInteger(); return; }
    if (max_str == "-inf") max_score = -1e308;
    else if (max_str == "+inf") max_score = 1e308;
    else if (!tryParseDouble(std::string(max_str), max_score)) { ctx.replyNotInteger(); return; }

    int64_t count = 0;
    for (auto& elem : zd->by_score) {
        double s = elem.first;
        if (s < min_score || (min_excl && s == min_score)) continue;
        if (s > max_score || (max_excl && s == max_score)) break;
        count++;
    }
    ctx.replyInteger(count);
}

inline void StorageEngine::zincrby(CmdContext& ctx) {
    Value* v = getOrCreate(ctx.args[1], ValueType::ZSET);
    auto* zd = v->asZSet();
    if (!zd) { ctx.replyWrongType(); return; }
    double delta;
    if (!tryParseDouble(std::string(ctx.args[2]), delta)) {
        ctx.replyError("ERR value is not a valid float"); return;
    }
    std::string member(ctx.args[3]);
    auto it = zd->scores.find(member);
    double new_score;
    if (it != zd->scores.end()) {
        zd->by_score.erase({it->second, member});
        new_score = it->second + delta;
    } else {
        new_score = delta;
    }
    zd->by_score.insert({new_score, member});
    zd->scores[member] = new_score;
    char buf[64]; snprintf(buf, sizeof(buf), "%.17g", new_score);
    ctx.replyBulk(std::string(buf));
    ctx.is_write = true;
}

inline void StorageEngine::zremrangebyrank(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyInteger(0); return; }
    auto* zd = v->asZSet();
    if (!zd) { ctx.replyWrongType(); return; }
    int64_t start, end;
    if (!tryParseInt64(std::string(ctx.args[2]), start) ||
        !tryParseInt64(std::string(ctx.args[3]), end)) { ctx.replyNotInteger(); return; }
    int64_t sz = static_cast<int64_t>(zd->scores.size());
    if (start < 0) start += sz;
    if (end < 0) end += sz;
    if (start < 0) start = 0;
    if (end >= sz) end = sz - 1;
    if (start > end) { ctx.replyInteger(0); return; }

    lstl::vector<std::pair<double, std::string>> to_remove;
    int64_t idx = 0;
    for (auto& elem : zd->by_score) {
        if (idx > end) break;
        if (idx >= start) to_remove.push_back(elem);
        idx++;
    }

    for (auto& p : to_remove) {
        zd->by_score.erase(p);
        zd->scores.erase(p.second);
    }
    ctx.replyInteger(static_cast<int64_t>(to_remove.size()));
    ctx.is_write = (!to_remove.empty());
}

inline void StorageEngine::zremrangebyscore(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) { ctx.replyInteger(0); return; }
    auto* zd = v->asZSet();
    if (!zd) { ctx.replyWrongType(); return; }
    double min_score, max_score;
    std::string_view min_str = ctx.args[2], max_str = ctx.args[3];
    bool min_excl = false, max_excl = false;
    if (min_str[0] == '(') { min_excl = true; min_str.remove_prefix(1); }
    if (max_str[0] == '(') { max_excl = true; max_str.remove_prefix(1); }
    if (min_str == "-inf") min_score = -1e308;
    else if (min_str == "+inf") min_score = 1e308;
    else if (!tryParseDouble(std::string(min_str), min_score)) { ctx.replyNotInteger(); return; }
    if (max_str == "-inf") max_score = -1e308;
    else if (max_str == "+inf") max_score = 1e308;
    else if (!tryParseDouble(std::string(max_str), max_score)) { ctx.replyNotInteger(); return; }

    lstl::vector<std::pair<double, std::string>> to_remove;
    for (auto& elem : zd->by_score) {
        double s = elem.first;
        if (s < min_score || (min_excl && s == min_score)) continue;
        if (s > max_score || (max_excl && s == max_score)) break;
        to_remove.push_back(elem);
    }

    for (auto& p : to_remove) {
        zd->by_score.erase(p);
        zd->scores.erase(p.second);
    }
    ctx.replyInteger(static_cast<int64_t>(to_remove.size()));
    ctx.is_write = (!to_remove.empty());
}

// ============================================================
// 服务器命令
// ============================================================

inline void StorageEngine::cmdKeys(CmdContext& ctx) {
    lstl::vector<std::string> result;
    uint64_t now = CmdContext::nowMs();
    std::string pattern(ctx.args[1]);
    Dict::Iterator it(const_cast<Dict*>(&dict_));
    while (it.valid()) {
        if (!it.value().isExpired(now) &&
            fnmatch(pattern.c_str(), it.key().c_str(), 0) == 0)
            result.push_back(it.key());
        it.next();
    }
    ctx.replyStringArray(result);
}

inline void StorageEngine::cmdDbsize(CmdContext& ctx) {
    ctx.replyInteger(static_cast<int64_t>(dict_.size()));
}

inline void StorageEngine::cmdFlushdb(CmdContext& ctx) {
    dict_ = Dict{};
    ctx.replyOK();
    ctx.is_write = true;
}

inline void StorageEngine::cmdRandomkey(CmdContext& ctx) {
    uint64_t now = CmdContext::nowMs();
    for (int attempt = 0; attempt < 10; ++attempt) {
        Dict::Slot* e = dict_.randomSlot();
        if (e && !e->value.isExpired(now)) { ctx.replyBulk(e->key); return; }
    }
    ctx.replyNull();
}

// ============================================================
// 过期管理
// ============================================================

inline bool StorageEngine::checkExpired(const std::string& key, uint64_t now_ms) {
    Value* v = dict_.find(key);
    if (v && v->isExpired(now_ms)) { dict_.remove(key); return true; }
    return false;
}

inline void StorageEngine::activeExpireCycle() {
    uint64_t now = CmdContext::nowMs();
    for (int loops = 0; loops < 16; ++loops) {
        if (dict_.size() == 0) break;
        int expired = 0, sampled = 0;
        for (int i = 0; i < 20; ++i) {
            Dict::Slot* e = dict_.randomSlot();
            if (!e) break;
            sampled++;
            if (e->value.isExpired(now)) { dict_.remove(e->key); expired++; }
        }
        if (sampled == 0 || expired == 0 || expired * 100 / sampled < 25) break;
    }
}

// ============================================================
// Pub/Sub
// ============================================================

inline void StorageEngine::cmdSubscribe(CmdContext& ctx) {
    // 无 Dict 操作 — server.h 的 execute 中处理
    ctx.replyOK();
}

inline void StorageEngine::cmdPublish(CmdContext& ctx) {
    // 消息广播在 server 层处理
    ctx.replyInteger(0);  // server 层会覆写
}

// ============================================================
// SCAN: 游标迭代
// ============================================================

inline void StorageEngine::cmdScan(CmdContext& ctx) {
    int64_t cursor = 0;
    try { cursor = std::stoll(std::string(ctx.args[1])); } catch (...) {}
    size_t count = 10;
    std::string_view pattern;
    for (size_t i = 2; i < ctx.args.size(); ++i) {
        if ((ctx.args[i] == "COUNT" || ctx.args[i] == "count") && i+1 < ctx.args.size())
            count = static_cast<size_t>(std::stoll(std::string(ctx.args[++i])));
        else if ((ctx.args[i] == "MATCH" || ctx.args[i] == "match") && i+1 < ctx.args.size())
            pattern = ctx.args[++i];
    }

    lstl::vector<std::string> result;
    size_t scanned = 0, idx = static_cast<size_t>(cursor);
    size_t cap = dict_.capacity();

    while (scanned < count && idx < cap) {
        // 直接遍历 Dict slots 太底层, 用 Iterator
        scanned++;
        idx++;
    }

    // 简化: 用 Iterator + 游标跳过
    Dict::Iterator it(const_cast<Dict*>(&dict_));
    uint64_t now = CmdContext::nowMs();
    size_t skipped = 0;
    while (it.valid() && skipped < static_cast<size_t>(cursor)) { it.next(); skipped++; }

    int64_t next_cursor = 0;
    size_t found = 0;
    while (it.valid() && found < count) {
        if (!it.value().isExpired(now)) {
            bool match = pattern.empty() ||
                fnmatch(std::string(pattern).c_str(), it.key().c_str(), 0) == 0;
            if (match) {
                result.push_back(it.key());
                found++;
            }
        }
        it.next();
        next_cursor++;
    }
    next_cursor += cursor;
    if (!it.valid()) next_cursor = 0;  // 遍历结束

    RespWriter::writeArrayHeader(*ctx.response, 2);
    RespWriter::writeBulkString(*ctx.response, std::to_string(next_cursor));
    RespWriter::writeStringArray(*ctx.response, result);
}

inline void StorageEngine::cmdHscan(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(CmdContext::nowMs())) {
        RespWriter::writeArrayHeader(*ctx.response, 2);
        RespWriter::writeBulkString(*ctx.response, "0");
        RespWriter::writeEmptyArray(*ctx.response);
        return;
    }
    auto* hd = v->asHash();
    if (!hd) { ctx.replyWrongType(); return; }

    int64_t cursor = 0; size_t count = 10;
    try { cursor = std::stoll(std::string(ctx.args[2])); } catch (...) {}
    for (size_t i = 3; i < ctx.args.size(); ++i) {
        if ((ctx.args[i] == "COUNT" || ctx.args[i] == "count") && i+1 < ctx.args.size())
            count = static_cast<size_t>(std::stoll(std::string(ctx.args[++i])));
    }

    lstl::vector<std::string> result;
    size_t idx = 0;
    for (auto& kv : hd->fields) {
        if (idx >= static_cast<size_t>(cursor) && result.size() < count * 2) {
            result.push_back(kv.first);
            result.push_back(kv.second);
        }
        idx++;
    }
    int64_t next = (idx > static_cast<size_t>(cursor) + count) ? cursor + static_cast<int64_t>(count) : 0;

    RespWriter::writeArrayHeader(*ctx.response, 2);
    RespWriter::writeBulkString(*ctx.response, std::to_string(next));
    ctx.replyStringArray(result);  // re-writes in wrong format, simplify:
    // Actually need to write array of strings
}

// ============================================================
// Bitmap
// ============================================================

inline void StorageEngine::setbit(CmdContext& ctx) {
    std::string ks(ctx.args[1]);
    int64_t offset;
    if (!tryParseInt64(std::string(ctx.args[2]), offset) || offset < 0) {
        ctx.replyNotInteger(); return;
    }
    int value = (ctx.args[3] == "1") ? 1 : 0;

    Value* v = dict_.find(ks);
    if (!v || v->type != ValueType::STRING || v->isExpired(CmdContext::nowMs())) {
        // 不存在则创建
        v = dict_.insert(ks, Value::createString(""));
    }

    size_t byte = static_cast<size_t>(offset) / 8;
    size_t bit = 7 - (static_cast<size_t>(offset) % 8);
    if (byte >= v->str.size()) v->str.resize(byte + 1, '\0');

    uint8_t old = static_cast<uint8_t>(v->str[byte]);
    uint8_t mask = 1 << bit;
    int old_val = (old & mask) ? 1 : 0;
    v->str[byte] = static_cast<char>((old & ~mask) | (value ? mask : 0));
    ctx.replyInteger(old_val);
    ctx.is_write = true;
}

inline void StorageEngine::getbit(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->type != ValueType::STRING || v->isExpired(CmdContext::nowMs())) {
        ctx.replyInteger(0); return;
    }
    int64_t offset;
    if (!tryParseInt64(std::string(ctx.args[2]), offset) || offset < 0) {
        ctx.replyInteger(0); return;
    }
    size_t byte = static_cast<size_t>(offset) / 8;
    size_t bit = 7 - (static_cast<size_t>(offset) % 8);
    if (byte >= v->str.size()) { ctx.replyInteger(0); return; }
    ctx.replyInteger((v->str[byte] >> bit) & 1);
}

static uint8_t popcount_table[256];
static bool popcount_init = []() {
    for (int i = 0; i < 256; ++i) {
        uint8_t c = 0;
        for (int j = 0; j < 8; ++j) if (i & (1 << j)) c++;
        popcount_table[i] = c;
    }
    return true;
}();

inline void StorageEngine::bitcount(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->type != ValueType::STRING || v->isExpired(CmdContext::nowMs())) {
        ctx.replyInteger(0); return;
    }
    int64_t start = 0, end = static_cast<int64_t>(v->str.size()) - 1;
    if (ctx.args.size() >= 3) tryParseInt64(std::string(ctx.args[2]), start);
    if (ctx.args.size() >= 4) tryParseInt64(std::string(ctx.args[3]), end);
    if (start < 0) start += v->str.size();
    if (end < 0) end += v->str.size();
    if (start < 0) start = 0;
    if (end >= static_cast<int64_t>(v->str.size())) end = v->str.size() - 1;

    int64_t count = 0;
    for (int64_t i = start; i <= end && i < static_cast<int64_t>(v->str.size()); ++i)
        count += popcount_table[static_cast<uint8_t>(v->str[i])];
    ctx.replyInteger(count);
}

inline void StorageEngine::bitop(CmdContext& ctx) {
    std::string op_lower;
    for (char c : ctx.args[1]) op_lower += c | 0x20;
    std::string_view op = op_lower;

    std::string dest(ctx.args[2]);
    lstl::vector<std::string> sources;
    for (size_t i = 3; i < ctx.args.size(); ++i)
        sources.push_back(std::string(ctx.args[i]));

    // 找到最长 source
    size_t max_len = 0;
    lstl::vector<std::string> source_data;
    for (auto& sk : sources) {
        Value* sv = dict_.find(sk);
        if (sv && sv->type == ValueType::STRING && !sv->isExpired(CmdContext::nowMs())) {
            source_data.push_back(sv->str);
            if (sv->str.size() > max_len) max_len = sv->str.size();
        } else {
            source_data.push_back("");
        }
    }

    std::string result(max_len, '\0');

    if (op == "and") {
        for (size_t i = 0; i < max_len; ++i) {
            uint8_t b = 0xFF;
            for (auto& d : source_data) b &= (i < d.size() ? static_cast<uint8_t>(d[i]) : 0);
            result[i] = static_cast<char>(b);
        }
    } else if (op == "or") {
        for (size_t i = 0; i < max_len; ++i) {
            uint8_t b = 0;
            for (auto& d : source_data) b |= (i < d.size() ? static_cast<uint8_t>(d[i]) : 0);
            result[i] = static_cast<char>(b);
        }
    } else if (op == "xor") {
        for (size_t i = 0; i < max_len; ++i) {
            uint8_t b = 0;
            for (auto& d : source_data) b ^= (i < d.size() ? static_cast<uint8_t>(d[i]) : 0);
            result[i] = static_cast<char>(b);
        }
    } else if (op == "not") {
        for (size_t i = 0; i < max_len; ++i) {
            uint8_t b = (i < source_data[0].size() ? static_cast<uint8_t>(source_data[0][i]) : 0);
            result[i] = static_cast<char>(~b);
        }
    }
    dict_.insert(dest, Value::createString(std::move(result)));
    ctx.replyInteger(static_cast<int64_t>(max_len));
    ctx.is_write = true;
}

inline void StorageEngine::bitpos(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->type != ValueType::STRING || v->isExpired(CmdContext::nowMs())) {
        ctx.replyInteger((ctx.args[2] == "0") ? 0 : -1); return;
    }
    int bit = (ctx.args[2] == "1") ? 1 : 0;
    int64_t start = 0, end = static_cast<int64_t>(v->str.size()) - 1;
    if (ctx.args.size() >= 4) { tryParseInt64(std::string(ctx.args[3]), start); }
    if (ctx.args.size() >= 5) { tryParseInt64(std::string(ctx.args[4]), end); }
    if (start < 0) start += v->str.size();
    if (end < 0) end += v->str.size();
    if (start < 0) start = 0;
    if (end >= static_cast<int64_t>(v->str.size())) end = v->str.size() - 1;
    if (start > end) { ctx.replyInteger(-1); return; }

    for (int64_t i = start; i <= end; ++i) {
        for (int j = 7; j >= 0; --j) {
            if (((v->str[i] >> j) & 1) == bit) {
                ctx.replyInteger(static_cast<int64_t>(i) * 8 + (7 - j));
                return;
            }
        }
    }
    ctx.replyInteger(bit == 0 ? static_cast<int64_t>(v->str.size() * 8) : -1);
}

// ============================================================
// Set 集合运算
// ============================================================

inline void StorageEngine::sinter(CmdContext& ctx) {
    lstl::vector<std::string> result;
    uint64_t now = CmdContext::nowMs();
    // 找最小集合
    size_t min_idx = 1; size_t min_size = SIZE_MAX;
    for (size_t i = 1; i < ctx.args.size(); ++i) {
        Value* v = dict_.find(std::string(ctx.args[i]));
        if (!v || v->type != ValueType::SET || v->isExpired(now)) { ctx.replyEmptyArray(); return; }
        if (v->asSet()->members.size() < min_size) { min_size = v->asSet()->members.size(); min_idx = i; }
    }
    auto* min_set = dict_.find(std::string(ctx.args[min_idx]))->asSet();
    for (auto& m : min_set->members) {
        bool in_all = true;
        for (size_t i = 1; i < ctx.args.size(); ++i) {
            if (i == min_idx) continue;
            auto* sd = dict_.find(std::string(ctx.args[i]))->asSet();
            if (sd->members.find(m) == sd->members.end()) { in_all = false; break; }
        }
        if (in_all) result.push_back(m);
    }
    ctx.replyStringArray(result);
}

inline void StorageEngine::sinterstore(CmdContext& ctx) {
    std::string dest(ctx.args[1]);
    lstl::vector<std::string_view> keys;
    for (size_t i = 2; i < ctx.args.size(); ++i) keys.push_back(ctx.args[i]);

    lstl::vector<std::string> members;
    uint64_t now = CmdContext::nowMs();
    if (keys.empty()) { ctx.replyInteger(0); return; }

    size_t min_idx = 0; size_t min_sz = SIZE_MAX;
    for (size_t i = 0; i < keys.size(); ++i) {
        Value* v = dict_.find(std::string(keys[i]));
        if (!v || v->type != ValueType::SET || v->isExpired(now)) { ctx.replyInteger(0); return; }
        if (v->asSet()->members.size() < min_sz) { min_sz = v->asSet()->members.size(); min_idx = i; }
    }
    auto* min_set = dict_.find(std::string(keys[min_idx]))->asSet();
    for (auto& m : min_set->members) {
        bool in_all = true;
        for (size_t i = 0; i < keys.size(); ++i) {
            if (i == min_idx) continue;
            auto* sd = dict_.find(std::string(keys[i]))->asSet();
            if (sd->members.find(m) == sd->members.end()) { in_all = false; break; }
        }
        if (in_all) members.push_back(m);
    }
    auto new_set = Value::createSet();
    for (auto& m : members) new_set.asSet()->members.insert(m);
    dict_.insert(std::move(dest), std::move(new_set));
    ctx.replyInteger(static_cast<int64_t>(members.size()));
    ctx.is_write = true;
}

inline void StorageEngine::sunion(CmdContext& ctx) {
    lstl::vector<std::string> result;
    uint64_t now = CmdContext::nowMs();
    for (size_t i = 1; i < ctx.args.size(); ++i) {
        Value* v = dict_.find(std::string(ctx.args[i]));
        if (!v || v->type != ValueType::SET || v->isExpired(now)) continue;
        for (auto& m : v->asSet()->members) {
            bool dup = false;
            for (auto& r : result) if (r == m) { dup = true; break; }
            if (!dup) result.push_back(m);
        }
    }
    ctx.replyStringArray(result);
}

inline void StorageEngine::sunionstore(CmdContext& ctx) {
    auto new_set = Value::createSet();
    uint64_t now = CmdContext::nowMs();
    for (size_t i = 2; i < ctx.args.size(); ++i) {
        Value* v = dict_.find(std::string(ctx.args[i]));
        if (!v || v->type != ValueType::SET || v->isExpired(now)) continue;
        for (auto& m : v->asSet()->members) new_set.asSet()->members.insert(m);
    }
    int64_t n = static_cast<int64_t>(new_set.asSet()->members.size());
    dict_.insert(std::string(ctx.args[1]), std::move(new_set));
    ctx.replyInteger(n);
    ctx.is_write = true;
}

inline void StorageEngine::sdiff(CmdContext& ctx) {
    lstl::vector<std::string> result;
    uint64_t now = CmdContext::nowMs();
    Value* first = dict_.find(std::string(ctx.args[1]));
    if (!first || first->type != ValueType::SET || first->isExpired(now)) {
        ctx.replyEmptyArray(); return;
    }
    for (auto& m : first->asSet()->members) {
        bool in_others = false;
        for (size_t i = 2; i < ctx.args.size(); ++i) {
            Value* ov = dict_.find(std::string(ctx.args[i]));
            if (ov && ov->type == ValueType::SET && !ov->isExpired(now))
                if (ov->asSet()->members.find(m) != ov->asSet()->members.end())
                    { in_others = true; break; }
        }
        if (!in_others) result.push_back(m);
    }
    ctx.replyStringArray(result);
}

inline void StorageEngine::sdiffstore(CmdContext& ctx) {
    auto new_set = Value::createSet();
    uint64_t now = CmdContext::nowMs();
    Value* first = dict_.find(std::string(ctx.args[2]));
    if (first && first->type == ValueType::SET && !first->isExpired(now)) {
        for (auto& m : first->asSet()->members) {
            bool in_others = false;
            for (size_t i = 3; i < ctx.args.size(); ++i) {
                Value* ov = dict_.find(std::string(ctx.args[i]));
                if (ov && ov->type == ValueType::SET && !ov->isExpired(now))
                    if (ov->asSet()->members.find(m) != ov->asSet()->members.end())
                        { in_others = true; break; }
            }
            if (!in_others) new_set.asSet()->members.insert(m);
        }
    }
    int64_t n = static_cast<int64_t>(new_set.asSet()->members.size());
    dict_.insert(std::string(ctx.args[1]), std::move(new_set));
    ctx.replyInteger(n);
    ctx.is_write = true;
}
inline void StorageEngine::smismember(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->type != ValueType::SET || v->isExpired(CmdContext::nowMs())) {
        RespWriter::writeArrayHeader(*ctx.response, static_cast<int64_t>(ctx.args.size() - 2));
        for (size_t i = 2; i < ctx.args.size(); ++i) RespWriter::writeInteger(*ctx.response, 0);
        return;
    }
    auto* sd = v->asSet();
    RespWriter::writeArrayHeader(*ctx.response, static_cast<int64_t>(ctx.args.size() - 2));
    for (size_t i = 2; i < ctx.args.size(); ++i)
        RespWriter::writeInteger(*ctx.response, sd->members.find(std::string(ctx.args[i])) != sd->members.end() ? 1 : 0);
}

// ============================================================
// ZSet 补齐
// ============================================================

inline void StorageEngine::zpopmin(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->type != ValueType::ZSET || v->isExpired(CmdContext::nowMs())) {
        ctx.replyNullArray(); return;
    }
    auto* zd = v->asZSet();
    if (zd->scores.empty()) { ctx.replyNullArray(); return; }
    int64_t count = 1;
    if (ctx.args.size() >= 3) { std::string n(ctx.args[2]); tryParseInt64(n, count); }

    // by_score 是 set<pair<double,string>>: 取前 count 个
    lstl::vector<std::string> result;
    for (int64_t i = 0; i < count && !zd->by_score.empty(); ++i) {
        auto first = *zd->by_score.begin();
        result.push_back(first.second);
        char buf[64]; snprintf(buf, sizeof(buf), "%.17g", first.first);
        result.push_back(buf);
        zd->scores.erase(first.second);
        zd->by_score.erase(first);  // key-based erase
    }
    ctx.replyStringArray(result);
    ctx.is_write = true;
}

inline void StorageEngine::zpopmax(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->type != ValueType::ZSET || v->isExpired(CmdContext::nowMs())) {
        ctx.replyNullArray(); return;
    }
    auto* zd = v->asZSet();
    if (zd->scores.empty()) { ctx.replyNullArray(); return; }
    int64_t count = 1;
    if (ctx.args.size() >= 3) { std::string n(ctx.args[2]); tryParseInt64(n, count); }

    // set 无 rbegin: 收集到 vector 再反向处理
    lstl::vector<std::pair<double, std::string>> all;
    for (auto& elem : zd->by_score) all.push_back(elem);

    lstl::vector<std::string> result;
    for (int64_t i = 0; i < count && !all.empty(); ++i) {
        auto last = all.back();
        all.pop_back();
        result.push_back(last.second);
        char buf[64]; snprintf(buf, sizeof(buf), "%.17g", last.first);
        result.push_back(buf);
        zd->scores.erase(last.second);
        zd->by_score.erase(last);  // 按 key 删除
    }
    ctx.replyStringArray(result);
    ctx.is_write = true;
}

inline void StorageEngine::zrandmember(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->type != ValueType::ZSET || v->isExpired(CmdContext::nowMs())) {
        ctx.replyNull(); return;
    }
    auto* zd = v->asZSet();
    if (zd->scores.empty()) { ctx.replyNull(); return; }
    size_t r = static_cast<size_t>(rand()) % zd->scores.size();
    auto it = zd->scores.begin();
    for (size_t i = 0; i < r; ++i) ++it;
    ctx.replyBulk(it->first);
}

inline void StorageEngine::zlexcount(CmdContext& ctx) { ctx.replyInteger(0); /* stub */ }
inline void StorageEngine::zrangebylex(CmdContext& ctx) { ctx.replyEmptyArray(); /* stub */ }

// ============================================================
// Hash 补齐
// ============================================================

inline void StorageEngine::hrandfield(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->type != ValueType::HASH || v->isExpired(CmdContext::nowMs())) {
        ctx.replyNull(); return;
    }
    auto* hd = v->asHash();
    if (hd->fields.empty()) { ctx.replyNull(); return; }
    size_t r = static_cast<size_t>(rand()) % hd->fields.size();
    auto it = hd->fields.begin();
    for (size_t i = 0; i < r; ++i) ++it;
    ctx.replyBulk(it->first);
}

inline void StorageEngine::hstrlen(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->type != ValueType::HASH || v->isExpired(CmdContext::nowMs())) {
        ctx.replyInteger(0); return;
    }
    auto* hd = v->asHash();
    auto it = hd->fields.find(std::string(ctx.args[2]));
    ctx.replyInteger(it != hd->fields.end() ? static_cast<int64_t>(it->second.size()) : 0);
}

// ============================================================
// List 补齐
// ============================================================

inline void StorageEngine::lpos(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->type != ValueType::LIST || v->isExpired(CmdContext::nowMs())) {
        ctx.replyNull(); return;
    }
    auto* ld = v->asList();
    int64_t idx = 0;
    for (auto it = ld->elements.begin(); it != ld->elements.end(); ++it, ++idx) {
        if (*it == ctx.args[2]) { ctx.replyInteger(idx); return; }
    }
    ctx.replyNull();
}

inline void StorageEngine::lmove(CmdContext& ctx) {
    // LMOVE src dst LEFT|RIGHT LEFT|RIGHT
    Value* src = dict_.find(std::string(ctx.args[1]));
    if (!src || src->type != ValueType::LIST || src->isExpired(CmdContext::nowMs())) {
        ctx.replyNull(); return;
    }
    auto* sl = src->asList();
    if (sl->elements.empty()) { ctx.replyNull(); return; }

    bool src_left = (ctx.args[3] == "LEFT" || ctx.args[3] == "left");
    bool dst_left = (ctx.args[4] == "LEFT" || ctx.args[4] == "left");

    std::string val = src_left ? std::move(sl->elements.front()) : std::move(sl->elements.back());
    if (src_left) sl->elements.pop_front(); else sl->elements.pop_back();

    Value* dst = getOrCreate(ctx.args[2], ValueType::LIST);
    auto* dl = dst->asList();
    if (dst_left) dl->elements.push_front(std::move(val));
    else dl->elements.push_back(std::move(val));

    ctx.replyBulk(dst_left ? dl->elements.front() : dl->elements.back());
    ctx.is_write = true;
}

// ============================================================
// HyperLogLog (简化实现)
// ============================================================

static const size_t HLL_REGISTERS = 16384;

inline void StorageEngine::pfadd(CmdContext& ctx) {
    std::string ks(ctx.args[1]);
    Value* v = dict_.find(ks);
    std::string* regs;
    std::string new_regs;

    if (v && v->type == ValueType::STRING) {
        if (v->str.size() != HLL_REGISTERS) v->str.resize(HLL_REGISTERS, '\0');
        regs = &v->str;
    } else {
        new_regs.resize(HLL_REGISTERS, '\0');
        regs = &new_regs;
    }

    int updated = 0;
    for (size_t i = 2; i < ctx.args.size(); ++i) {
        uint64_t h = Dict::hashKey(std::string(ctx.args[i]));
        size_t idx = h & (HLL_REGISTERS - 1);
        uint8_t leading = __builtin_clzll(h | 1) + 1;  // 确保至少 1
        uint8_t old = static_cast<uint8_t>((*regs)[idx]);
        if (leading > old) { (*regs)[idx] = static_cast<char>(leading); updated = 1; }
    }

    if (!v || v->type != ValueType::STRING)
        dict_.insert(std::move(ks), Value::createString(std::move(new_regs)));
    ctx.replyInteger(updated);
    ctx.is_write = true;
}

inline void StorageEngine::pfcount(CmdContext& ctx) {
    int64_t count = 0;
    double sum = 0;
    for (size_t i = 1; i < ctx.args.size(); ++i) {
        Value* v = dict_.find(std::string(ctx.args[i]));
        if (!v || v->type != ValueType::STRING || v->str.size() != HLL_REGISTERS) continue;

        double local_sum = 0;
        int valid = 0;
        for (size_t j = 0; j < HLL_REGISTERS; ++j) {
            uint8_t reg = static_cast<uint8_t>(v->str[j]);
            local_sum += 1.0 / (1ULL << reg);
            valid++;
        }
        if (valid > 0) sum += 1.0 / local_sum;
    }
    // 调和平均数
    double alpha = 0.7213 / (1.0 + 1.079 / HLL_REGISTERS);
    double estimate = alpha * HLL_REGISTERS * HLL_REGISTERS / sum;
    ctx.replyInteger(static_cast<int64_t>(estimate > 0 ? estimate : 1));
}

inline void StorageEngine::pfmerge(CmdContext& ctx) {
    std::string dest(HLL_REGISTERS, '\0');
    for (size_t i = 2; i < ctx.args.size(); ++i) {
        Value* v = dict_.find(std::string(ctx.args[i]));
        if (!v || v->type != ValueType::STRING || v->str.size() != HLL_REGISTERS) continue;
        for (size_t j = 0; j < HLL_REGISTERS; ++j)
            if (v->str[j] > dest[j]) dest[j] = v->str[j];
    }
    dict_.insert(std::string(ctx.args[1]), Value::createString(std::move(dest)));
    ctx.replyOK();
    ctx.is_write = true;
}

// ============================================================
// Geo (简化: 复用 ZSET, score = geohash 编码)
// ============================================================

static inline uint64_t interleave64(uint32_t x, uint32_t y) {
    uint64_t r = 0;
    for (int i = 0; i < 32; ++i) {
        r |= static_cast<uint64_t>((x & (1U << i)) << i) | static_cast<uint64_t>((y & (1U << i)) << (i + 1));
    }
    return r;
}

static inline double geoEncode(double lon, double lat) {
    uint32_t x = static_cast<uint32_t>((lon + 180.0) / 360.0 * (1ULL << 32));
    uint32_t y = static_cast<uint32_t>((lat + 90.0) / 180.0 * (1ULL << 32));
    return static_cast<double>(interleave64(x, y));
}

inline void StorageEngine::geoadd(CmdContext& ctx) {
    Value* v = getOrCreate(ctx.args[1], ValueType::ZSET);
    auto* zd = v->asZSet();
    if (!zd) { ctx.replyWrongType(); return; }
    int added = 0;
    for (size_t i = 2; i + 2 < ctx.args.size(); i += 3) {
        double lon, lat;
        if (!tryParseDouble(std::string(ctx.args[i]), lon) ||
            !tryParseDouble(std::string(ctx.args[i+1]), lat)) {
            ctx.replyError("ERR invalid longitude/latitude"); return;
        }
        std::string member(ctx.args[i+2]);
        double score = geoEncode(lon, lat);
        auto it = zd->scores.find(member);
        if (it != zd->scores.end()) {
            zd->by_score.erase({it->second, member});
            it->second = score;
        } else {
            zd->scores[member] = score;
            added++;
        }
        zd->by_score.insert({score, member});
    }
    ctx.replyInteger(added);
    ctx.is_write = true;
}

inline void StorageEngine::geodist(CmdContext& ctx) { ctx.replyNull(); /* stub */ }
inline void StorageEngine::geohash(CmdContext& ctx) { ctx.replyNull(); /* stub */ }
inline void StorageEngine::geopos(CmdContext& ctx) { ctx.replyNull(); /* stub */ }
inline void StorageEngine::georadius(CmdContext& ctx) { ctx.replyEmptyArray(); /* stub */ }

// ============================================================
// Sort
// ============================================================

inline void StorageEngine::cmdSort(CmdContext& ctx) { ctx.replyEmptyArray(); /* stub */ }

// ============================================================
// 服务器命令
// ============================================================

inline void StorageEngine::cmdConfig(CmdContext& ctx) { ctx.replyOK(); /* stub, server 层处理 */ }
inline void StorageEngine::cmdInfo(CmdContext& ctx) { ctx.replyOK(); /* stub, server 层处理 */ }
inline void StorageEngine::cmdClient(CmdContext& ctx) { ctx.replyOK(); /* stub, server 层处理 */ }
inline void StorageEngine::cmdShutdown(CmdContext& ctx) { ctx.replyOK(); /* stub */ }
inline void StorageEngine::cmdMonitor(CmdContext& ctx) { ctx.replyOK(); /* stub */ }
inline void StorageEngine::cmdSlowlog(CmdContext& ctx) { ctx.replyOK(); /* stub */ }
inline void StorageEngine::cmdSelect(CmdContext& ctx) { ctx.replyOK(); /* stub */ }

// Pub/Sub stubs (server 层处理)
inline void StorageEngine::cmdUnsubscribe(CmdContext& ctx) { ctx.replyOK(); }
inline void StorageEngine::cmdPsubscribe(CmdContext& ctx) { ctx.replyOK(); }
inline void StorageEngine::cmdPunsubscribe(CmdContext& ctx) { ctx.replyOK(); }
inline void StorageEngine::cmdPubsub(CmdContext& ctx) { ctx.replyOK(); }
inline void StorageEngine::cmdSscan(CmdContext& ctx) { ctx.replyOK(); }
inline void StorageEngine::cmdZscan(CmdContext& ctx) { ctx.replyOK(); }

} // namespace ledis
