#pragma once
#include <lstl/container/unordered_map.h>
#include "ledis/storage/value.h"
#include "ledis/cmd/cmd_context.h"
#include <cstdint>

namespace ledis {

// ============================================================
// HashStore — Hash 类型操作
// ============================================================
// 内部存储: lstl::unordered_map (后续可加 ziplist 优化)
class HashStore {
public:
    static void hset(CmdContext& ctx) {
        auto* v = getOrCreate(ctx);
        if (!v) return;
        auto* h = v->asHash();
        int count = 0;
        for (size_t i = 2; i + 1 < ctx.args.size(); i += 2) {
            std::string field(ctx.args[i]), val(ctx.args[i+1]);
            if (h->fields.find(field) == h->fields.end()) count++;
            h->fields[std::move(field)] = std::move(val);
        }
        ctx.replyInteger(count);
    }

    static void hget(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyNull(); return; }
        auto* h = v->asHash();
        auto it = h->fields.find(std::string(ctx.args[2]));
        if (it != h->fields.end()) ctx.replyBulk(it->second);
        else ctx.replyNull();
    }

    static void hdel(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyInteger(0); return; }
        auto* h = v->asHash();
        int count = 0;
        for (size_t i = 2; i < ctx.args.size(); ++i)
            count += h->fields.erase(std::string(ctx.args[i]));
        ctx.replyInteger(count);
    }

    static void hexists(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyInteger(0); return; }
        auto* h = v->asHash();
        ctx.replyInteger(h->fields.count(std::string(ctx.args[2])) ? 1 : 0);
    }

    static void hgetall(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyEmptyArray(); return; }
        auto* h = v->asHash();
        RespWriter::writeArrayHeader(*ctx.response_buf, h->fields.size() * 2);
        for (auto& [k, val] : h->fields) {
            RespWriter::writeBulkString(*ctx.response_buf, k);
            RespWriter::writeBulkString(*ctx.response_buf, val);
        }
    }

    static void hlen(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyInteger(0); return; }
        ctx.replyInteger(static_cast<int64_t>(v->asHash()->fields.size()));
    }

    static void hkeys(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyEmptyArray(); return; }
        auto* h = v->asHash();
        RespWriter::writeArrayHeader(*ctx.response_buf, h->fields.size());
        for (auto& [k, _] : h->fields)
            RespWriter::writeBulkString(*ctx.response_buf, k);
    }

    static void hvals(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyEmptyArray(); return; }
        auto* h = v->asHash();
        RespWriter::writeArrayHeader(*ctx.response_buf, h->fields.size());
        for (auto& [_, val] : h->fields)
            RespWriter::writeBulkString(*ctx.response_buf, val);
    }

    static void hincrby(CmdContext& ctx) {
        auto* v = getOrCreate(ctx);
        if (!v) return;
        auto* h = v->asHash();
        std::string field(ctx.args[2]);
        int64_t delta;
        try { delta = std::stoll(std::string(ctx.args[3])); } catch (...) {
            ctx.replyError("ERR value is not an integer or out of range"); return;
        }
        int64_t cur = 0;
        auto it = h->fields.find(field);
        if (it != h->fields.end()) {
            try { cur = std::stoll(it->second); } catch (...) {
                ctx.replyError("ERR hash value is not an integer"); return;
            }
        }
        cur += delta;
        h->fields[field] = std::to_string(cur);
        ctx.replyInteger(cur);
    }

    static void hsetnx(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) {
            auto nv = Value::createHash();
            std::string key(ctx.args[1]);
            ctx.engine->insert(key, std::move(nv));
            v = ctx.engine->find(key);
        }
        if (!v) { ctx.replyInteger(0); return; }
        auto* h = v->asHash();
        std::string field(ctx.args[2]);
        if (h->fields.count(field)) { ctx.replyInteger(0); return; }
        h->fields[field] = std::string(ctx.args[3]);
        ctx.replyInteger(1);
    }

    static void hmset(CmdContext& ctx) {
        // HMSET is alias for HSET with multiple fields
        hset(ctx);
    }

    static void hmget(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) {
            RespWriter::writeArrayHeader(*ctx.response_buf,
                static_cast<int64_t>(ctx.args.size() - 2));
            for (size_t i = 0; i < ctx.args.size() - 2; ++i)
                RespWriter::writeNull(*ctx.response_buf);
            return;
        }
        auto* h = v->asHash();
        RespWriter::writeArrayHeader(*ctx.response_buf,
            static_cast<int64_t>(ctx.args.size() - 2));
        for (size_t i = 2; i < ctx.args.size(); ++i) {
            auto it = h->fields.find(std::string(ctx.args[i]));
            if (it != h->fields.end())
                RespWriter::writeBulkString(*ctx.response_buf, it->second);
            else
                RespWriter::writeNull(*ctx.response_buf);
        }
    }

    static void hincrbyfloat(CmdContext& ctx) {
        auto* v = getOrCreate(ctx);
        if (!v) return;
        auto* h = v->asHash();
        std::string field(ctx.args[2]);
        double delta;
        try { delta = std::stod(std::string(ctx.args[3])); } catch (...) {
            ctx.replyError("ERR value is not a valid float"); return;
        }
        double cur = 0.0;
        auto it = h->fields.find(field);
        if (it != h->fields.end()) {
            try { cur = std::stod(it->second); } catch (...) {
                ctx.replyError("ERR hash value is not a float"); return;
            }
        }
        cur += delta;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.17g", cur);
        h->fields[field] = buf;
        ctx.replyBulk(std::string(buf));
    }

    static void hstrlen(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyInteger(0); return; }
        auto* h = v->asHash();
        auto it = h->fields.find(std::string(ctx.args[2]));
        ctx.replyInteger(it != h->fields.end() ? (int64_t)it->second.size() : 0);
    }

private:
    static Value* getExisting(CmdContext& ctx) {
        std::string key(ctx.args[1]);
        Value* v = ctx.engine->find(key);
        if (!v || v->isExpired(ctx.nowMs())) return nullptr;
        if (v->type != ValueType::HASH) { ctx.replyWrongType(); return nullptr; }
        return v;
    }

    static Value* getOrCreate(CmdContext& ctx) {
        std::string key(ctx.args[1]);
        Value* v = ctx.engine->find(key);
        if (v && v->isExpired(ctx.nowMs())) {
            ctx.engine->remove(key);
            v = nullptr;
        }
        if (!v) {
            auto nv = Value::createHash();
            ctx.engine->insert(key, std::move(nv));
            v = ctx.engine->find(key);
        }
        if (v && v->type != ValueType::HASH) { ctx.replyWrongType(); return nullptr; }
        return v;
    }
};

} // namespace ledis
