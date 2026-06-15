#pragma once

#include <chrono>
#include <cstdint>
#include <fnmatch.h>
#include <string>
#include <lstl/container/vector.h>
#include <sys/time.h>

#include "ledis/storage/dict.h"
#include "ledis/cmd/cmd_context.h"

namespace ledis {

// ============================================================
// StorageEngine — 单线程存储引擎
// ============================================================
//
// 拥有 Dict，提供所有 KV 操作的实现。
// 在存储线程中运行，所有操作串行化，无需锁。
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

    // ======== 字符串操作 ========
    void incr(CmdContext& ctx);
    void incrby(CmdContext& ctx);
    void decr(CmdContext& ctx);
    void decrby(CmdContext& ctx);
    void append(CmdContext& ctx);
    void strlen(CmdContext& ctx);
    void mget(CmdContext& ctx);
    void mset(CmdContext& ctx);

    // ======== 新增 String 操作 ========
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

    // ======== 服务器命令 ========
    void keys(CmdContext& ctx);
    void dbsize(CmdContext& ctx);
    void flushdb(CmdContext& ctx);
    void randomkey(CmdContext& ctx);

    // ======== 直接 API (供内部使用) ========
    Value* find(const std::string& key);
    void   insert(std::string key, Value value);
    Value  remove(const std::string& key);

    // ======== 过期管理 ========
    bool checkExpired(const std::string& key, uint64_t now_ms);
    void activeExpireCycle();

    // ======== 访问器 ========
    Dict&       dict()       { return dict_; }
    const Dict& dict() const { return dict_; }
    size_t      size() const { return dict_.size(); }

    // ======== 统计 ========
    uint64_t hit_count_  = 0;
    uint64_t miss_count_ = 0;

    static uint64_t nowMs() {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME_COARSE, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1000ULL
             + static_cast<uint64_t>(ts.tv_nsec) / 1000000ULL;
    }

private:
    Dict dict_;

    // 尝试将字符串转换为 int64_t (用于 INCR/DECR)
    bool tryParseInt64(const std::string& s, int64_t& out);
};

// ============================================================
// 实现
// ============================================================

// ---- 基础 KV ----

inline void StorageEngine::set(CmdContext& ctx) {
    // SET key value [EX seconds|PX ms] [NX|XX]
    const auto& args = ctx.args;
    std::string_view key = args[1];
    std::string_view val = args[2];

    // 检查过期选项
    uint64_t expire_ms = 0;
    bool nx = false, xx = false;

    // 解析可选参数
    for (size_t i = 3; i < args.size(); ++i) {
        std::string_view opt = args[i];

        if ((opt == "EX" || opt == "ex") && i + 1 < args.size()) {
            int64_t sec = 0;
            auto s = args[++i];
            try { sec = std::stoll(std::string(s)); } catch (...) { sec = 0; }
            if (sec > 0) expire_ms = ctx.nowMs() + static_cast<uint64_t>(sec) * 1000;
        }
        else if ((opt == "PX" || opt == "px") && i + 1 < args.size()) {
            int64_t ms = 0;
            auto s = args[++i];
            try { ms = std::stoll(std::string(s)); } catch (...) { ms = 0; }
            if (ms > 0) expire_ms = ctx.nowMs() + static_cast<uint64_t>(ms);
        }
        else if (opt == "NX" || opt == "nx") {
            nx = true;
        }
        else if (opt == "XX" || opt == "xx") {
            xx = true;
        }
    }

    // 处理 NX/XX
    Value* existing = dict_.find(std::string(key));
    if (nx && existing) {
        ctx.replyNull();
        return;
    }
    if (xx && !existing) {
        ctx.replyNull();
        return;
    }

    Value v = Value::createString(std::string(val));
    v.expire_at_ms = expire_ms;

    dict_.insert(std::string(key), std::move(v));
    ctx.replyOK();
    hit_count_++;
}

inline void StorageEngine::get(CmdContext& ctx) {
    std::string_view key = ctx.args[1];
    Value* v = dict_.find(std::string(key));

    if (!v || v->type != ValueType::STRING) {
        ctx.replyNull();
        miss_count_++;
        return;
    }

    // 检查过期
    if (v->isExpired(ctx.nowMs())) {
        ctx.replyNull();
        miss_count_++;
        return;
    }

    ctx.replyBulk(v->str);
    hit_count_++;
}

inline void StorageEngine::del(CmdContext& ctx) {
    int deleted = 0;
    for (size_t i = 1; i < ctx.args.size(); ++i) {
        std::string key(ctx.args[i]);
        Value v = dict_.remove(key);
        if (v.type == ValueType::STRING && !v.str.empty()) {
            deleted++;
        } else if (v.opaque_ptr != nullptr) {
            // 复杂类型 (未来清理)
            deleted++;
        }
    }
    ctx.replyInteger(deleted);
}

inline void StorageEngine::exists(CmdContext& ctx) {
    int count = 0;
    uint64_t now = ctx.nowMs();
    for (size_t i = 1; i < ctx.args.size(); ++i) {
        Value* v = dict_.find(std::string(ctx.args[i]));
        if (v && !v->isExpired(now)) {
            count++;
        }
    }
    ctx.replyInteger(count);
}

inline void StorageEngine::type(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->isExpired(ctx.nowMs())) {
        ctx.replySimpleString("none");
    } else {
        ctx.replySimpleString(valueTypeName(v->type));
    }
}

// ---- 过期 ----

inline void StorageEngine::expire(CmdContext& ctx) {
    // EXPIRE key seconds
    std::string key(ctx.args[1]);
    int64_t sec;
    try { sec = std::stoll(std::string(ctx.args[2])); } catch (...) {
        ctx.replyError("ERR value is not an integer or out of range");
        return;
    }
    Value* v = dict_.find(key);
    if (!v) { ctx.replyInteger(0); return; }
    v->expire_at_ms = ctx.nowMs() + static_cast<uint64_t>(sec) * 1000;
    ctx.replyInteger(1);
}

inline void StorageEngine::expireat(CmdContext& ctx) {
    // EXPIREAT key timestamp(s)
    std::string key(ctx.args[1]);
    int64_t ts;
    try { ts = std::stoll(std::string(ctx.args[2])); } catch (...) {
        ctx.replyError("ERR value is not an integer or out of range");
        return;
    }
    Value* v = dict_.find(key);
    if (!v) { ctx.replyInteger(0); return; }
    v->expire_at_ms = static_cast<uint64_t>(ts) * 1000;
    ctx.replyInteger(1);
}

inline void StorageEngine::pexpire(CmdContext& ctx) {
    std::string key(ctx.args[1]);
    int64_t ms;
    try { ms = std::stoll(std::string(ctx.args[2])); } catch (...) {
        ctx.replyError("ERR value is not an integer or out of range");
        return;
    }
    Value* v = dict_.find(key);
    if (!v) { ctx.replyInteger(0); return; }
    v->expire_at_ms = ctx.nowMs() + static_cast<uint64_t>(ms);
    ctx.replyInteger(1);
}

inline void StorageEngine::pexpireat(CmdContext& ctx) {
    // PEXPIREAT key ms-timestamp
    std::string key(ctx.args[1]);
    int64_t ts;
    try { ts = std::stoll(std::string(ctx.args[2])); } catch (...) {
        ctx.replyError("ERR value is not an integer or out of range");
        return;
    }
    Value* v = dict_.find(key);
    if (!v) { ctx.replyInteger(0); return; }
    v->expire_at_ms = static_cast<uint64_t>(ts);
    ctx.replyInteger(1);
}

inline void StorageEngine::ttl(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v) { ctx.replyInteger(-2); return; }
    int64_t t = v->ttlSec(ctx.nowMs());
    ctx.replyInteger(t >= 0 ? t : (t == -1 ? -1 : -2));
}

inline void StorageEngine::pttl(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v) { ctx.replyInteger(-2); return; }
    ctx.replyInteger(v->ttlMs(ctx.nowMs()));
}

inline void StorageEngine::persist(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->expire_at_ms == 0) { ctx.replyInteger(0); return; }
    if (v->isExpired(ctx.nowMs())) { ctx.replyInteger(0); return; }
    v->expire_at_ms = 0;
    ctx.replyInteger(1);
}

// ---- 字符串操作 ----

inline bool StorageEngine::tryParseInt64(const std::string& s, int64_t& out) {
    if (s.empty()) return false;
    try {
        size_t pos;
        out = std::stoll(s, &pos);
        return pos == s.size();
    } catch (...) {
        return false;
    }
}

inline void StorageEngine::incr(CmdContext& ctx) {
    std::string key(ctx.args[1]);
    Value* v = dict_.find(key);
    int64_t val = 0;

    if (v && v->type == ValueType::STRING) {
        if (!tryParseInt64(v->str, val)) {
            ctx.replyError("ERR value is not an integer or out of range");
            return;
        }
        val++;
        v->str = std::to_string(val);
        v->int_val = val;
    } else {
        val = 1;
        auto new_v = Value::createInt(val);
        dict_.insert(key, std::move(new_v));
    }
    ctx.replyInteger(val);
}

inline void StorageEngine::incrby(CmdContext& ctx) {
    std::string key(ctx.args[1]);
    int64_t delta;
    if (!tryParseInt64(std::string(ctx.args[2]), delta)) {
        ctx.replyError("ERR value is not an integer or out of range");
        return;
    }

    Value* v = dict_.find(key);
    int64_t val = 0;
    if (v && v->type == ValueType::STRING) {
        if (!tryParseInt64(v->str, val)) {
            ctx.replyError("ERR value is not an integer or out of range");
            return;
        }
        val += delta;
        v->str = std::to_string(val);
        v->int_val = val;
    } else {
        val = delta;
        auto new_v = Value::createInt(val);
        dict_.insert(key, std::move(new_v));
    }
    ctx.replyInteger(val);
}

inline void StorageEngine::decr(CmdContext& ctx) {
    // 直接复用 incrby -1
    std::string key(ctx.args[1]);
    Value* v = dict_.find(key);
    int64_t val = 0;

    if (v && v->type == ValueType::STRING) {
        if (!tryParseInt64(v->str, val)) {
            ctx.replyError("ERR value is not an integer or out of range");
            return;
        }
        val--;
        v->str = std::to_string(val);
        v->int_val = val;
    } else {
        val = -1;
        auto new_v = Value::createInt(val);
        dict_.insert(key, std::move(new_v));
    }
    ctx.replyInteger(val);
}

inline void StorageEngine::decrby(CmdContext& ctx) {
    std::string key(ctx.args[1]);
    int64_t delta;
    if (!tryParseInt64(std::string(ctx.args[2]), delta)) {
        ctx.replyError("ERR value is not an integer or out of range");
        return;
    }
    return incrby(ctx);  // 用 incrby 的负 delta 实现
}

inline void StorageEngine::append(CmdContext& ctx) {
    std::string key(ctx.args[1]);
    std::string_view suffix = ctx.args[2];

    Value* v = dict_.find(key);
    if (v && v->type == ValueType::STRING) {
        if (v->isExpired(ctx.nowMs())) {
            // key 已过期，相当于创建
            v->str.assign(suffix);
            v->expire_at_ms = 0;
            ctx.replyInteger(static_cast<int64_t>(suffix.size()));
        } else {
            v->str.append(suffix.data(), suffix.size());
            ctx.replyInteger(static_cast<int64_t>(v->str.size()));
        }
    } else if (!v) {
        auto new_v = Value::createString(std::string(suffix));
        dict_.insert(key, std::move(new_v));
        ctx.replyInteger(static_cast<int64_t>(suffix.size()));
    } else {
        ctx.replyWrongType();
    }
}

inline void StorageEngine::strlen(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->type != ValueType::STRING || v->isExpired(ctx.nowMs())) {
        ctx.replyInteger(0);
    } else {
        ctx.replyInteger(static_cast<int64_t>(v->str.size()));
    }
}

inline void StorageEngine::mget(CmdContext& ctx) {
    uint64_t now = ctx.nowMs();
    RespWriter::writeArrayHeader(*ctx.response_buf,
                                  static_cast<int64_t>(ctx.args.size() - 1));

    for (size_t i = 1; i < ctx.args.size(); ++i) {
        Value* v = dict_.find(std::string(ctx.args[i]));
        if (!v || v->type != ValueType::STRING || v->isExpired(now)) {
            RespWriter::writeNull(*ctx.response_buf);
        } else {
            RespWriter::writeBulkString(*ctx.response_buf, v->str);
        }
    }
}

inline void StorageEngine::mset(CmdContext& ctx) {
    // MSET key1 val1 [key2 val2 ...]
    uint64_t now = ctx.nowMs();
    for (size_t i = 1; i + 1 < ctx.args.size(); i += 2) {
        std::string key(ctx.args[i]);
        auto v = Value::createString(std::string(ctx.args[i + 1]));
        dict_.insert(std::move(key), std::move(v));
    }
    ctx.replyOK();
}

// ---- 新增 String 操作 ----

inline void StorageEngine::setnx(CmdContext& ctx) {
    std::string key(ctx.args[1]);
    if (dict_.find(key)) { ctx.replyInteger(0); return; }
    auto v = Value::createString(std::string(ctx.args[2]));
    dict_.insert(std::move(key), std::move(v));
    ctx.replyInteger(1);
}

inline void StorageEngine::setex(CmdContext& ctx) {
    std::string key(ctx.args[1]);
    int64_t sec = 0;
    try { sec = std::stoll(std::string(ctx.args[2])); } catch (...) {
        ctx.replyError("ERR invalid expire time"); return;
    }
    auto v = Value::createString(std::string(ctx.args[3]));
    v.expire_at_ms = ctx.nowMs() + static_cast<uint64_t>(sec) * 1000;
    dict_.insert(std::move(key), std::move(v));
    ctx.replyOK();
}

inline void StorageEngine::psetex(CmdContext& ctx) {
    std::string key(ctx.args[1]);
    int64_t ms = 0;
    try { ms = std::stoll(std::string(ctx.args[2])); } catch (...) {
        ctx.replyError("ERR invalid expire time"); return;
    }
    auto v = Value::createString(std::string(ctx.args[3]));
    v.expire_at_ms = ctx.nowMs() + static_cast<uint64_t>(ms);
    dict_.insert(std::move(key), std::move(v));
    ctx.replyOK();
}

inline void StorageEngine::getset(CmdContext& ctx) {
    std::string key(ctx.args[1]);
    std::string new_val(ctx.args[2]);
    Value* v = dict_.find(key);
    if (!v || v->type != ValueType::STRING || v->isExpired(ctx.nowMs())) {
        ctx.replyNull();
        dict_.insert(key, Value::createString(std::move(new_val)));
        return;
    }
    ctx.replyBulk(v->str);
    v->str = std::move(new_val);
}

inline void StorageEngine::getrange(CmdContext& ctx) {
    Value* v = dict_.find(std::string(ctx.args[1]));
    if (!v || v->type != ValueType::STRING || v->isExpired(ctx.nowMs())) {
        ctx.replyBulk(std::string_view{}); return;
    }
    int64_t start, end;
    try { start = std::stoll(std::string(ctx.args[2]));
          end = std::stoll(std::string(ctx.args[3])); } catch (...) {
        ctx.replyError("ERR value is not an integer"); return;
    }
    int64_t sz = static_cast<int64_t>(v->str.size());
    if (start < 0) start += sz;
    if (end < 0) end += sz;
    if (start < 0) start = 0;
    if (end >= sz) end = sz - 1;
    if (start > end) { ctx.replyBulk(std::string_view{}); return; }
    ctx.replyBulk(v->str.substr(static_cast<size_t>(start),
                                static_cast<size_t>(end - start + 1)));
}

inline void StorageEngine::setrange(CmdContext& ctx) {
    std::string key(ctx.args[1]);
    int64_t offset;
    try { offset = std::stoll(std::string(ctx.args[2])); } catch (...) {
        ctx.replyError("ERR value is not an integer"); return;
    }
    if (offset < 0) { ctx.replyError("ERR offset is out of range"); return; }
    std::string_view val = ctx.args[3];

    Value* v = dict_.find(key);
    if (!v || v->type != ValueType::STRING || v->isExpired(ctx.nowMs())) {
        std::string s(static_cast<size_t>(offset) + val.size(), '\0');
        s.replace(static_cast<size_t>(offset), val.size(), val.data(), val.size());
        dict_.insert(key, Value::createString(std::move(s)));
        ctx.replyInteger(static_cast<int64_t>(offset + val.size()));
        return;
    }
    if (static_cast<size_t>(offset) + val.size() > v->str.size())
        v->str.resize(static_cast<size_t>(offset) + val.size(), '\0');
    v->str.replace(static_cast<size_t>(offset), val.size(), val.data(), val.size());
    ctx.replyInteger(static_cast<int64_t>(v->str.size()));
}

inline void StorageEngine::incrbyfloat(CmdContext& ctx) {
    std::string key(ctx.args[1]);
    double delta;
    try { delta = std::stod(std::string(ctx.args[2])); } catch (...) {
        ctx.replyError("ERR value is not a valid float"); return;
    }
    Value* v = dict_.find(key);
    double cur = 0.0;
    if (v && v->type == ValueType::STRING) {
        try { cur = std::stod(v->str); } catch (...) {
            ctx.replyError("ERR value is not a valid float"); return;
        }
    }
    cur += delta;
    char buf[64];
    snprintf(buf, sizeof(buf), "%.17g", cur);
    if (v) { v->str = buf; } else {
        dict_.insert(key, Value::createString(buf));
    }
    ctx.replyBulk(std::string(buf));
}

inline void StorageEngine::msetnx(CmdContext& ctx) {
    // 先检查是否所有 key 都不存在
    uint64_t now = ctx.nowMs();
    for (size_t i = 1; i + 1 < ctx.args.size(); i += 2) {
        Value* v = dict_.find(std::string(ctx.args[i]));
        if (v && !v->isExpired(now)) { ctx.replyInteger(0); return; }
    }
    for (size_t i = 1; i + 1 < ctx.args.size(); i += 2) {
        dict_.insert(std::string(ctx.args[i]),
                     Value::createString(std::string(ctx.args[i + 1])));
    }
    ctx.replyInteger(1);
}

inline void StorageEngine::getdel(CmdContext& ctx) {
    std::string key(ctx.args[1]);
    Value* v = dict_.find(key);
    if (!v || v->type != ValueType::STRING || v->isExpired(ctx.nowMs())) {
        ctx.replyNull(); return;
    }
    std::string val = v->str;
    dict_.remove(key);
    ctx.replyBulk(val);
}

inline void StorageEngine::rename(CmdContext& ctx) {
    std::string old_key(ctx.args[1]);
    std::string new_key(ctx.args[2]);
    if (old_key == new_key) { ctx.replyOK(); return; }
    Value* v = dict_.find(old_key);
    if (!v || v->isExpired(ctx.nowMs())) {
        ctx.replyError("ERR no such key"); return;
    }
    Value copy = std::move(*v);
    dict_.remove(old_key);
    dict_.insert(std::move(new_key), std::move(copy));
    ctx.replyOK();
}

inline void StorageEngine::renamenx(CmdContext& ctx) {
    std::string old_key(ctx.args[1]);
    std::string new_key(ctx.args[2]);
    if (old_key == new_key) { ctx.replyOK(); return; }
    Value* v = dict_.find(old_key);
    if (!v || v->isExpired(ctx.nowMs())) { ctx.replyError("ERR no such key"); return; }
    if (dict_.find(new_key)) { ctx.replyInteger(0); return; }
    Value copy = std::move(*v);
    dict_.remove(old_key);
    dict_.insert(std::move(new_key), std::move(copy));
    ctx.replyInteger(1);
}

// ---- 服务器命令 ----

inline void StorageEngine::keys(CmdContext& ctx) {
    // KEYS pattern (仅支持 * 通配符的简单实现)
    std::string_view pattern = ctx.args[1];
    lstl::vector<std::string> result;
    uint64_t now = ctx.nowMs();

    Dict::Iterator it(const_cast<Dict*>(&dict_));
    while (it.valid()) {
        Value* v = it.value();
        if (v && !v->isExpired(now)) {
            if (fnmatch(std::string(pattern).c_str(), it.key().c_str(), 0) == 0) {
                result.push_back(it.key());
            }
        }
        it.next();
    }

    ctx.replyStringArray(result);
}

inline void StorageEngine::dbsize(CmdContext& ctx) {
    ctx.replyInteger(static_cast<int64_t>(dict_.size()));
}

inline void StorageEngine::flushdb(CmdContext& ctx) {
    // 简单实现: 重建 Dict
    dict_ = Dict{};
    ctx.replyOK();
}

inline void StorageEngine::randomkey(CmdContext& ctx) {
    Dict::Entry* e = dict_.randomEntry();
    uint64_t now = ctx.nowMs();

    // 跳过已过期的
    int attempts = 10;
    while (e && attempts > 0) {
        if (!e->value.isExpired(now)) break;
        e = dict_.randomEntry();
        attempts--;
    }

    if (e && !e->value.isExpired(now)) {
        ctx.replyBulk(e->key);
    } else {
        ctx.replyNull();
    }
}

// ---- 直接 API ----

inline Value* StorageEngine::find(const std::string& key) {
    return dict_.find(key);
}

inline void StorageEngine::insert(std::string key, Value value) {
    dict_.insert(std::move(key), std::move(value));
}

inline Value StorageEngine::remove(const std::string& key) {
    return dict_.remove(key);
}

inline bool StorageEngine::checkExpired(const std::string& key, uint64_t now_ms) {
    Value* v = dict_.find(key);
    if (v && v->isExpired(now_ms)) {
        dict_.remove(key);
        return true;
    }
    return false;
}

inline void StorageEngine::activeExpireCycle() {
    // 随机采样，删除过期 key
    // 如果 >25% 过期则继续循环
    uint64_t now = nowMs();
    int loops = 0;
    static constexpr int MAX_LOOPS  = 16;
    static constexpr int MAX_SAMPLES = 20;
    static constexpr int EXPIRE_RATIO = 25;  // 25%

    while (loops < MAX_LOOPS) {
        if (dict_.size() == 0) break;

        int expired = 0;
        int sampled = 0;

        for (int i = 0; i < MAX_SAMPLES; ++i) {
            Dict::Entry* e = dict_.randomEntry();
            if (!e) break;
            sampled++;
            if (e->value.isExpired(now)) {
                dict_.remove(e->key);
                expired++;
            }
        }

        if (sampled == 0 || expired == 0) break;

        // 如果过期比例低于阈值，停止
        if (expired * 100 / sampled < EXPIRE_RATIO) break;

        loops++;
    }
}

} // namespace ledis
