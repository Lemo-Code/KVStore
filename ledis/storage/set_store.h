#pragma once
#include "ledis/storage/value.h"
#include "ledis/cmd/cmd_context.h"
#include <algorithm>
#include <lstl/container/vector.h>

namespace ledis {

class SetStore {
public:
    static void sadd(CmdContext& ctx) {
        auto* v = getOrCreate(ctx);
        if (!v) return;
        auto* s = v->asSet();
        int count = 0;
        for (size_t i = 2; i < ctx.args.size(); ++i)
            count += s->members.insert(std::string(ctx.args[i])).second ? 1 : 0;
        ctx.replyInteger(count);
    }

    static void srem(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyInteger(0); return; }
        auto* s = v->asSet();
        int count = 0;
        for (size_t i = 2; i < ctx.args.size(); ++i)
            count += s->members.erase(std::string(ctx.args[i]));
        ctx.replyInteger(count);
    }

    static void smembers(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyEmptyArray(); return; }
        auto* s = v->asSet();
        RespWriter::writeArrayHeader(*ctx.response_buf, s->members.size());
        for (auto& m : s->members) RespWriter::writeBulkString(*ctx.response_buf, m);
    }

    static void sismember(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyInteger(0); return; }
        ctx.replyInteger(v->asSet()->members.count(std::string(ctx.args[2])) ? 1 : 0);
    }

    static void scard(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        ctx.replyInteger(v ? (int64_t)v->asSet()->members.size() : 0);
    }

    static void sinter(CmdContext& ctx) {
        lstl::unordered_set<std::string> result;
        bool first = true;
        for (size_t i = 1; i < ctx.args.size(); ++i) {
            Value* v = ctx.engine->find(std::string(ctx.args[i]));
            if (!v || v->type != ValueType::SET || v->isExpired(ctx.nowMs())) {
                ctx.replyEmptyArray(); return;
            }
            auto* s = v->asSet();
            if (first) { for (auto& m : s->members) result.insert(m); first = false; }
            else {
                lstl::unordered_set<std::string> tmp;
                for (auto& m : s->members) if (result.count(m)) tmp.insert(m);
                result = std::move(tmp);
            }
        }
        RespWriter::writeArrayHeader(*ctx.response_buf, result.size());
        for (auto& m : result) RespWriter::writeBulkString(*ctx.response_buf, m);
    }

    static void sunion(CmdContext& ctx) {
        lstl::unordered_set<std::string> result;
        for (size_t i = 1; i < ctx.args.size(); ++i) {
            Value* v = ctx.engine->find(std::string(ctx.args[i]));
            if (!v || v->type != ValueType::SET || v->isExpired(ctx.nowMs())) continue;
            auto* s = v->asSet();
            for (auto& m : s->members) result.insert(m);
        }
        RespWriter::writeArrayHeader(*ctx.response_buf, result.size());
        for (auto& m : result) RespWriter::writeBulkString(*ctx.response_buf, m);
    }

    static void srandmember(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v || v->asSet()->members.empty()) { ctx.replyNull(); return; }
        int64_t count = 1;
        if (ctx.args.size() > 2) try { count = std::stoll(std::string(ctx.args[2])); } catch(...) {}

        auto* s = v->asSet();
        auto it = s->members.begin();
        std::advance(it, rand() % s->members.size());
        if (count == 1) {
            ctx.replyBulk(*it);
        } else {
            RespWriter::writeArrayHeader(*ctx.response_buf, count);
            for (int64_t i = 0; i < count; ++i)
                RespWriter::writeBulkString(*ctx.response_buf, *it); // simplified
        }
    }

    static void spop(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v || v->asSet()->members.empty()) { ctx.replyNull(); return; }
        auto* s = v->asSet();
        auto it = s->members.begin();
        std::advance(it, rand() % s->members.size());
        std::string member = *it;
        s->members.erase(*it);
        ctx.replyBulk(member);
    }

    static void smove(CmdContext& ctx) {
        auto* src = getExisting(ctx);
        if (!src) { ctx.replyInteger(0); return; }
        std::string member(ctx.args[3]);
        auto* ss = src->asSet();
        if (!ss->members.count(member)) { ctx.replyInteger(0); return; }
        ss->members.erase(member);

        std::string dst_key(ctx.args[2]);
        Value* dst = ctx.engine->find(dst_key);
        if (!dst || dst->type != ValueType::SET) {
            ctx.engine->insert(dst_key, Value::createSet());
            dst = ctx.engine->find(dst_key);
        }
        if (dst) dst->asSet()->members.insert(std::move(member));
        ctx.replyInteger(1);
    }

    static void smismember(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) {
            RespWriter::writeArrayHeader(*ctx.response_buf,
                static_cast<int64_t>(ctx.args.size() - 2));
            for (size_t i = 0; i < ctx.args.size() - 2; ++i)
                RespWriter::writeInteger(*ctx.response_buf, 0);
            return;
        }
        auto* s = v->asSet();
        RespWriter::writeArrayHeader(*ctx.response_buf,
            static_cast<int64_t>(ctx.args.size() - 2));
        for (size_t i = 2; i < ctx.args.size(); ++i)
            RespWriter::writeInteger(*ctx.response_buf,
                s->members.count(std::string(ctx.args[i])) ? 1 : 0);
    }

    static void sunionstore(CmdContext& ctx) {
        std::string dst_key(ctx.args[1]);
        lstl::unordered_set<std::string> result;
        for (size_t i = 2; i < ctx.args.size(); ++i) {
            Value* v = ctx.engine->find(std::string(ctx.args[i]));
            if (!v || v->type != ValueType::SET) continue;
            for (auto& m : v->asSet()->members) result.insert(m);
        }
        ctx.engine->insert(dst_key, Value::createSet());
        Value* dst = ctx.engine->find(dst_key);
        if (dst) dst->asSet()->members = std::move(result);
        ctx.replyInteger(static_cast<int64_t>(
            dst ? dst->asSet()->members.size() : 0));
    }

    static void sinterstore(CmdContext& ctx) {
        std::string dst_key(ctx.args[1]);
        if (ctx.args.size() < 3) { ctx.replyError("ERR wrong number of arguments"); return; }
        // Get first set
        Value* first = ctx.engine->find(std::string(ctx.args[2]));
        if (!first || first->type != ValueType::SET) { ctx.engine->insert(dst_key, Value::createSet()); ctx.replyInteger(0); return; }
        lstl::unordered_set<std::string> result;
        for (auto& m : first->asSet()->members) result.insert(m);
        for (size_t i = 3; i < ctx.args.size(); ++i) {
            Value* v = ctx.engine->find(std::string(ctx.args[i]));
            if (!v || v->type != ValueType::SET) { ctx.engine->insert(dst_key, Value::createSet()); ctx.replyInteger(0); return; }
            auto* s = v->asSet();
            lstl::unordered_set<std::string> tmp;
            for (auto& m : s->members) if (result.count(m)) tmp.insert(m);
            result = std::move(tmp);
        }
        int64_t count = static_cast<int64_t>(result.size());
        ctx.engine->insert(dst_key, Value::createSet());
        Value* dst = ctx.engine->find(dst_key);
        if (dst) dst->asSet()->members = std::move(result);
        ctx.replyInteger(count);
    }
    static void sdiffstore(CmdContext& ctx) {
        std::string dst_key(ctx.args[1]);
        if (ctx.args.size() < 3) { ctx.replyError("ERR wrong number of arguments"); return; }
        Value* first = ctx.engine->find(std::string(ctx.args[2]));
        if (!first || first->type != ValueType::SET) { ctx.engine->insert(dst_key, Value::createSet()); ctx.replyInteger(0); return; }
        lstl::unordered_set<std::string> result;
        for (auto& m : first->asSet()->members) result.insert(m);
        for (size_t i = 3; i < ctx.args.size(); ++i) {
            Value* v = ctx.engine->find(std::string(ctx.args[i]));
            if (!v || v->type != ValueType::SET) continue;
            for (auto& m : v->asSet()->members) result.erase(m);
        }
        int64_t count = static_cast<int64_t>(result.size());
        ctx.engine->insert(dst_key, Value::createSet());
        Value* dst = ctx.engine->find(dst_key);
        if (dst) dst->asSet()->members = std::move(result);
        ctx.replyInteger(count);
    }

    static void sdiff(CmdContext& ctx) {
        if (ctx.args.size() < 2) { ctx.replyEmptyArray(); return; }
        Value* v0 = ctx.engine->find(std::string(ctx.args[1]));
        if (!v0 || v0->type != ValueType::SET || v0->isExpired(ctx.nowMs())) {
            ctx.replyEmptyArray(); return;
        }
        auto* s0 = v0->asSet();
        lstl::unordered_set<std::string> result(s0->members.begin(), s0->members.end());
        for (size_t i = 2; i < ctx.args.size(); ++i) {
            Value* v = ctx.engine->find(std::string(ctx.args[i]));
            if (!v || v->type != ValueType::SET || v->isExpired(ctx.nowMs())) continue;
            auto* s = v->asSet();
            for (auto& m : s->members) result.erase(m);
        }
        RespWriter::writeArrayHeader(*ctx.response_buf, result.size());
        for (auto& m : result) RespWriter::writeBulkString(*ctx.response_buf, m);
    }

private:
    static Value* getExisting(CmdContext& ctx) {
        std::string key(ctx.args[1]);
        Value* v = ctx.engine->find(key);
        if (!v || v->isExpired(ctx.nowMs())) return nullptr;
        if (v->type != ValueType::SET) { ctx.replyWrongType(); return nullptr; }
        return v;
    }
    static Value* getOrCreate(CmdContext& ctx) {
        std::string key(ctx.args[1]);
        Value* v = ctx.engine->find(key);
        if (v && v->isExpired(ctx.nowMs())) { ctx.engine->remove(key); v = nullptr; }
        if (!v) { ctx.engine->insert(key, Value::createSet()); v = ctx.engine->find(key); }
        if (v && v->type != ValueType::SET) { ctx.replyWrongType(); return nullptr; }
        return v;
    }
};

} // namespace ledis
