#pragma once
#include "ledis/storage/value.h"
#include "ledis/cmd/cmd_context.h"
#include <cstdint>
#include <algorithm>

namespace ledis {

class ListStore {
public:
    static void lpush(CmdContext& ctx) {
        auto* v = getOrCreate(ctx);
        if (!v) return;
        auto* l = v->asList();
        int count = 0;
        for (size_t i = 2; i < ctx.args.size(); ++i, ++count)
            l->elements.push_front(std::string(ctx.args[i]));
        ctx.replyInteger(l->elements.size());
    }

    static void rpush(CmdContext& ctx) {
        auto* v = getOrCreate(ctx);
        if (!v) return;
        auto* l = v->asList();
        for (size_t i = 2; i < ctx.args.size(); ++i)
            l->elements.push_back(std::string(ctx.args[i]));
        ctx.replyInteger(static_cast<int64_t>(l->elements.size()));
    }

    static void lpop(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v || v->asList()->elements.empty()) { ctx.replyNull(); return; }
        auto* l = v->asList();
        std::string val = std::move(l->elements.front());
        l->elements.pop_front();
        ctx.replyBulk(val);
    }

    static void rpop(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v || v->asList()->elements.empty()) { ctx.replyNull(); return; }
        auto* l = v->asList();
        std::string val = std::move(l->elements.back());
        l->elements.pop_back();
        ctx.replyBulk(val);
    }

    static void llen(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        ctx.replyInteger(v ? (int64_t)v->asList()->elements.size() : 0);
    }

    static void lrange(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyEmptyArray(); return; }
        auto* l = v->asList();
        int64_t start, stop;
        try { start = std::stoll(std::string(ctx.args[2])); stop = std::stoll(std::string(ctx.args[3])); }
        catch (...) { ctx.replyError("ERR value is not an integer"); return; }

        int64_t sz = static_cast<int64_t>(l->elements.size());
        if (start < 0) start += sz;
        if (stop < 0) stop += sz;
        if (start < 0) start = 0;
        if (stop >= sz) stop = sz - 1;

        int64_t count = (start <= stop) ? (stop - start + 1) : 0;
        RespWriter::writeArrayHeader(*ctx.response_buf, count);
        if (count > 0) {
            auto it = l->elements.begin();
            std::advance(it, start);
            for (int64_t i = 0; i < count; ++i, ++it)
                RespWriter::writeBulkString(*ctx.response_buf, *it);
        }
    }

    static void lindex(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyNull(); return; }
        auto* l = v->asList();
        int64_t idx;
        try { idx = std::stoll(std::string(ctx.args[2])); } catch (...) {
            ctx.replyError("ERR value is not an integer"); return;
        }
        int64_t sz = static_cast<int64_t>(l->elements.size());
        if (idx < 0) idx += sz;
        if (idx < 0 || idx >= sz) { ctx.replyNull(); return; }
        auto it = l->elements.begin();
        std::advance(it, idx);
        ctx.replyBulk(*it);
    }

    static void lset(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyError("ERR no such key"); return; }
        auto* l = v->asList();
        int64_t idx;
        try { idx = std::stoll(std::string(ctx.args[2])); } catch (...) {
            ctx.replyError("ERR value is not an integer"); return;
        }
        int64_t sz = static_cast<int64_t>(l->elements.size());
        if (idx < 0) idx += sz;
        if (idx < 0 || idx >= sz) { ctx.replyError("ERR index out of range"); return; }
        auto it = l->elements.begin();
        std::advance(it, idx);
        *it = std::string(ctx.args[3]);
        ctx.replyOK();
    }

    static void lpushx(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyInteger(0); return; }
        auto* l = v->asList();
        l->elements.push_front(std::string(ctx.args[2]));
        ctx.replyInteger(static_cast<int64_t>(l->elements.size()));
    }

    static void rpushx(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyInteger(0); return; }
        auto* l = v->asList();
        l->elements.push_back(std::string(ctx.args[2]));
        ctx.replyInteger(static_cast<int64_t>(l->elements.size()));
    }

    static void rpoplpush(CmdContext& ctx) {
        auto* src = getExisting(ctx);
        if (!src || src->asList()->elements.empty()) { ctx.replyNull(); return; }
        auto* sl = src->asList();
        std::string val = std::move(sl->elements.back());
        sl->elements.pop_back();

        std::string dst_key(ctx.args[2]);
        Value* dst = ctx.engine->find(dst_key);
        if (!dst || dst->type != ValueType::LIST) {
            ctx.engine->insert(dst_key, Value::createList());
            dst = ctx.engine->find(dst_key);
        }
        if (dst) dst->asList()->elements.push_front(std::move(val));
        ctx.replyBulk(dst ? dst->asList()->elements.front() : "");
    }

    static void lrem(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyInteger(0); return; }
        auto* l = v->asList();
        int64_t count;
        try { count = std::stoll(std::string(ctx.args[2])); } catch (...) {
            ctx.replyError("ERR value is not an integer"); return;
        }
        std::string val(ctx.args[3]);
        size_t removed = 0;

        if (count == 0) {
            l->elements.remove(val);
            removed = 1;  // approximate
        } else if (count > 0) {
            auto it = l->elements.begin();
            while (it != l->elements.end() && static_cast<int64_t>(removed) < count) {
                if (*it == val) { it = l->elements.erase(it); removed++; }
                else ++it;
            }
        } else {
            count = -count;
            auto it = l->elements.end();
            while (it != l->elements.begin() && static_cast<int64_t>(removed) < count) {
                --it;
                if (*it == val) { it = l->elements.erase(it); removed++; }
            }
        }
        ctx.replyInteger(static_cast<int64_t>(removed));
    }

    static void linsert(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyInteger(-1); return; }
        auto* l = v->asList();
        std::string where(ctx.args[2]);  // BEFORE or AFTER
        std::string pivot(ctx.args[3]);
        std::string val(ctx.args[4]);

        auto it = std::find(l->elements.begin(), l->elements.end(), pivot);
        if (it == l->elements.end()) { ctx.replyInteger(-1); return; }

        if (where == "BEFORE" || where == "before") {
            l->elements.insert(it, std::move(val));
        } else {
            l->elements.insert(std::next(it), std::move(val));
        }
        ctx.replyInteger(static_cast<int64_t>(l->elements.size()));
    }

    // Blocking pop: returns immediately like LPOP, or nil if no elements
    static void blpop(CmdContext& ctx) {
        for (size_t i = 1; i < ctx.args.size(); ++i) {
            std::string key(ctx.args[i]);
            Value* v = ctx.engine->find(key);
            if (v && v->type == ValueType::LIST && !v->isExpired(ctx.nowMs())) {
                auto* l = v->asList();
                if (!l->elements.empty()) {
                    std::string val = std::move(l->elements.front());
                    l->elements.pop_front();
                    // Reply: *2\r\n$<key>\r\n<key>\r\n$<val>\r\n<val>\r\n
                    std::string& buf = *ctx.response_buf;
                    buf = "*2\r\n";
                    RespWriter::writeBulkStringInline(buf, key);
                    RespWriter::writeBulkStringInline(buf, val);
                    return;
                }
            }
        }
        ctx.replyNull();
    }
    static void brpop(CmdContext& ctx) {
        for (size_t i = 1; i < ctx.args.size(); ++i) {
            std::string key(ctx.args[i]);
            Value* v = ctx.engine->find(key);
            if (v && v->type == ValueType::LIST && !v->isExpired(ctx.nowMs())) {
                auto* l = v->asList();
                if (!l->elements.empty()) {
                    std::string val = std::move(l->elements.back());
                    l->elements.pop_back();
                    std::string& buf = *ctx.response_buf;
                    buf = "*2\r\n";
                    RespWriter::writeBulkStringInline(buf, key);
                    RespWriter::writeBulkStringInline(buf, val);
                    return;
                }
            }
        }
        ctx.replyNull();
    }

    static void ltrim(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyOK(); return; }
        auto* l = v->asList();
        int64_t start, stop;
        try { start = std::stoll(std::string(ctx.args[2])); stop = std::stoll(std::string(ctx.args[3])); }
        catch (...) { ctx.replyError("ERR value is not an integer"); return; }

        int64_t sz = static_cast<int64_t>(l->elements.size());
        if (start < 0) start += sz;
        if (stop < 0) stop += sz;
        if (start < 0) start = 0;
        if (start > stop || start >= sz) { l->elements.clear(); ctx.replyOK(); return; }

        // Rebuild list with desired range (lstl::list has no range erase)
        lstl::list<std::string> trimmed;
        auto it = l->elements.begin();
        std::advance(it, start);
        int64_t end_pos = std::min(stop + 1, sz);
        for (int64_t i = start; i < end_pos && it != l->elements.end(); ++i, ++it) {
            trimmed.push_back(*it);
        }
        l->elements.swap(trimmed);
        ctx.replyOK();
    }

private:
    static Value* getExisting(CmdContext& ctx) {
        std::string key(ctx.args[1]);
        Value* v = ctx.engine->find(key);
        if (!v || v->isExpired(ctx.nowMs())) return nullptr;
        if (v->type != ValueType::LIST) { ctx.replyWrongType(); return nullptr; }
        return v;
    }
    static Value* getOrCreate(CmdContext& ctx) {
        std::string key(ctx.args[1]);
        Value* v = ctx.engine->find(key);
        if (v && v->isExpired(ctx.nowMs())) { ctx.engine->remove(key); v = nullptr; }
        if (!v) { ctx.engine->insert(key, Value::createList()); v = ctx.engine->find(key); }
        if (v && v->type != ValueType::LIST) { ctx.replyWrongType(); return nullptr; }
        return v;
    }
};

} // namespace ledis
