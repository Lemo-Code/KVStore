#pragma once
#include "ledis/storage/value.h"
#include "ledis/cmd/cmd_context.h"
#include <cfloat>
#include <algorithm>
#include <lstl/container/vector.h>

namespace ledis {

class ZSetStore {
public:
    static void zadd(CmdContext& ctx) {
        auto* v = getOrCreate(ctx);
        if (!v) return;
        auto* z = v->asZSet();
        int added = 0;
        for (size_t i = 2; i + 1 < ctx.args.size(); i += 2) {
            double score;
            try { score = std::stod(std::string(ctx.args[i])); } catch (...) { continue; }
            std::string member(ctx.args[i+1]);

            auto it = z->scores.find(member);
            if (it != z->scores.end()) {
                z->by_score.erase({member, it->second});
                it->second = score;
                z->by_score.insert({member, score});
            } else {
                z->scores[member] = score;
                z->by_score.insert({member, score});
                added++;
            }
        }
        ctx.replyInteger(added);
    }

    static void zrem(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyInteger(0); return; }
        auto* z = v->asZSet();
        int count = 0;
        for (size_t i = 2; i < ctx.args.size(); ++i) {
            std::string member(ctx.args[i]);
            auto it = z->scores.find(member);
            if (it != z->scores.end()) {
                z->by_score.erase({member, it->second});
                z->scores.erase(it->first);
                count++;
            }
        }
        ctx.replyInteger(count);
    }

    static void zscore(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyNull(); return; }
        auto* z = v->asZSet();
        auto it = z->scores.find(std::string(ctx.args[2]));
        if (it != z->scores.end()) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%.17g", it->second);
            ctx.replyBulk(std::string(buf));
        } else ctx.replyNull();
    }

    static void zcard(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        ctx.replyInteger(v ? (int64_t)v->asZSet()->scores.size() : 0);
    }

    static void zrank(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyNull(); return; }
        auto* z = v->asZSet();
        std::string member(ctx.args[2]);
        auto it = z->scores.find(member);
        if (it == z->scores.end()) { ctx.replyNull(); return; }
        auto pos = z->by_score.find({member, it->second});
        int64_t rank = std::distance(z->by_score.begin(), pos);
        ctx.replyInteger(rank);
    }

    static void zrange(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyEmptyArray(); return; }
        auto* z = v->asZSet();
        int64_t start, stop;
        try { start = std::stoll(std::string(ctx.args[2])); stop = std::stoll(std::string(ctx.args[3])); }
        catch (...) { ctx.replyError("ERR value is not an integer"); return; }

        bool withscores = (ctx.args.size() > 4 && std::string(ctx.args[4]) == "WITHSCORES");

        int64_t sz = static_cast<int64_t>(z->by_score.size());
        if (start < 0) start += sz;
        if (stop < 0) stop += sz;
        if (start < 0) start = 0;
        if (stop >= sz) stop = sz - 1;

        int64_t count = (start <= stop) ? (stop - start + 1) : 0;
        RespWriter::writeArrayHeader(*ctx.response_buf, withscores ? count * 2 : count);
        if (count > 0) {
            auto it = z->by_score.begin();
            std::advance(it, start);
            for (int64_t i = 0; i < count; ++i, ++it) {
                RespWriter::writeBulkString(*ctx.response_buf, it->member);
                if (withscores) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "%.17g", it->score);
                    RespWriter::writeBulkString(*ctx.response_buf, std::string(buf));
                }
            }
        }
    }

    static void zincrby(CmdContext& ctx) {
        auto* v = getOrCreate(ctx);
        if (!v) return;
        auto* z = v->asZSet();
        double delta;
        try { delta = std::stod(std::string(ctx.args[2])); } catch (...) {
            ctx.replyError("ERR value is not a valid float"); return;
        }
        std::string member(ctx.args[3]);
        auto it = z->scores.find(member);
        double cur = (it != z->scores.end()) ? it->second : 0.0;
        if (it != z->scores.end()) z->by_score.erase({member, it->second});
        cur += delta;
        z->scores[member] = cur;
        z->by_score.insert({member, cur});
        char buf[64];
        snprintf(buf, sizeof(buf), "%.17g", cur);
        ctx.replyBulk(std::string(buf));
    }

    static void zrevrank(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyNull(); return; }
        auto* z = v->asZSet();
        std::string member(ctx.args[2]);
        auto it = z->scores.find(member);
        if (it == z->scores.end()) { ctx.replyNull(); return; }
        auto pos = z->by_score.find({member, it->second});
        int64_t rank = std::distance(pos, z->by_score.end()) - 1;
        ctx.replyInteger(rank);
    }

    static void zrevrange(CmdContext& ctx) { zrangeReverse(ctx, false); }

    static void zrangebyscore(CmdContext& ctx) { zrangeByScore(ctx, false); }
    static void zrevrangebyscore(CmdContext& ctx) { zrangeByScore(ctx, true); }

    static void zremrangebyrank(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyInteger(0); return; }
        auto* z = v->asZSet();
        int64_t start, stop;
        try { start = std::stoll(std::string(ctx.args[2]));
              stop = std::stoll(std::string(ctx.args[3])); } catch (...) {
            ctx.replyError("ERR value is not an integer"); return;
        }
        size_t old = z->scores.size();
        int64_t sz = static_cast<int64_t>(old);
        if (start < 0) start += sz;
        if (stop < 0) stop += sz;
        if (start < 0) start = 0;
        if (stop >= sz) stop = sz - 1;
        if (start > stop) { ctx.replyInteger(0); return; }
        auto it = z->by_score.begin();
        std::advance(it, start);
        for (int64_t i = start; i <= stop && it != z->by_score.end(); ++i) {
            z->scores.erase(it->member);
            auto key = *it; ++it; z->by_score.erase(key);
        }
        ctx.replyInteger(static_cast<int64_t>(old - z->scores.size()));
    }

    static void zremrangebyscore(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyInteger(0); return; }
        auto* z = v->asZSet();
        double min, max;
        try { min = std::stod(std::string(ctx.args[2]));
              max = std::stod(std::string(ctx.args[3])); } catch (...) {
            ctx.replyError("ERR min or max is not a float"); return;
        }
        size_t old = z->scores.size();
        auto it = z->by_score.begin();
        while (it != z->by_score.end()) {
            if (it->score >= min && it->score <= max) {
                z->scores.erase(it->member);
                auto key = *it; ++it; z->by_score.erase(key);
            } else ++it;
        }
        ctx.replyInteger(static_cast<int64_t>(old - z->scores.size()));
    }

    static void zpopmin(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v || v->asZSet()->by_score.empty()) { ctx.replyEmptyArray(); return; }
        auto* z = v->asZSet();
        int64_t count = 1;
        if (ctx.args.size() > 2) try { count = std::stoll(std::string(ctx.args[2])); } catch(...) {}
        RespWriter::writeArrayHeader(*ctx.response_buf, count * 2);
        for (int64_t i = 0; i < count && !z->by_score.empty(); ++i) {
            auto it = z->by_score.begin();
            std::string member = it->member;
            double score = it->score;
            z->by_score.erase(*it);
            z->scores.erase(member);
            RespWriter::writeBulkString(*ctx.response_buf, member);
            char buf[64]; snprintf(buf, sizeof(buf), "%.17g", score);
            RespWriter::writeBulkString(*ctx.response_buf, std::string(buf));
        }
    }

    static void zpopmax(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v || v->asZSet()->by_score.empty()) { ctx.replyEmptyArray(); return; }
        auto* z = v->asZSet();
        int64_t count = 1;
        if (ctx.args.size() > 2) try { count = std::stoll(std::string(ctx.args[2])); } catch(...) {}
        RespWriter::writeArrayHeader(*ctx.response_buf, count * 2);
        for (int64_t i = 0; i < count && !z->by_score.empty(); ++i) {
            auto it = z->by_score.end(); --it;
            std::string member = it->member;
            double score = it->score;
            z->by_score.erase(*it);
            z->scores.erase(member);
            RespWriter::writeBulkString(*ctx.response_buf, member);
            char buf[64]; snprintf(buf, sizeof(buf), "%.17g", score);
            RespWriter::writeBulkString(*ctx.response_buf, std::string(buf));
        }
    }

    static void zrangeReverse(CmdContext& ctx, bool withscores) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyEmptyArray(); return; }
        auto* z = v->asZSet();
        int64_t start, stop;
        try { start = std::stoll(std::string(ctx.args[2]));
              stop = std::stoll(std::string(ctx.args[3])); } catch (...) {
            ctx.replyError("ERR value is not an integer"); return;
        }
        int64_t sz = static_cast<int64_t>(z->by_score.size());
        if (start < 0) start += sz;
        if (stop < 0) stop += sz;
        if (start < 0) start = 0;
        if (stop >= sz) stop = sz - 1;

        int64_t count = (start <= stop) ? (stop - start + 1) : 0;
        RespWriter::writeArrayHeader(*ctx.response_buf, withscores ? count * 2 : count);
        if (count > 0) {
            auto it = z->by_score.end();
            std::advance(it, -static_cast<int>(start) - 1);
            for (int64_t i = 0; i < count; ++i, ++it) {
                RespWriter::writeBulkString(*ctx.response_buf, it->member);
                if (withscores) {
                    char buf[64]; snprintf(buf, sizeof(buf), "%.17g", it->score);
                    RespWriter::writeBulkString(*ctx.response_buf, std::string(buf));
                }
            }
        }
    }

    static void zrangeByScore(CmdContext& ctx, bool reverse) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyEmptyArray(); return; }
        auto* z = v->asZSet();
        double min, max;
        try { min = std::stod(std::string(ctx.args[2]));
              max = std::stod(std::string(ctx.args[3])); } catch (...) {
            ctx.replyError("ERR min or max is not a float"); return;
        }
        bool withscores = (ctx.args.size() > 4 && std::string(ctx.args[4]) == "WITHSCORES");

        lstl::vector<std::pair<std::string, double>> result;
        for (auto& e : z->by_score) {
            if (e.score >= min && e.score <= max)
                result.push_back({e.member, e.score});
        }
        if (reverse) std::reverse(result.begin(), result.end());

        RespWriter::writeArrayHeader(*ctx.response_buf,
            withscores ? result.size() * 2 : result.size());
        for (auto& [mem, sc] : result) {
            RespWriter::writeBulkString(*ctx.response_buf, mem);
            if (withscores) {
                char buf[64]; snprintf(buf, sizeof(buf), "%.17g", sc);
                RespWriter::writeBulkString(*ctx.response_buf, std::string(buf));
            }
        }
    }

    static void zcount(CmdContext& ctx) {
        auto* v = getExisting(ctx);
        if (!v) { ctx.replyInteger(0); return; }
        auto* z = v->asZSet();
        double min_score, max_score;
        try { min_score = std::stod(std::string(ctx.args[2])); max_score = std::stod(std::string(ctx.args[3])); }
        catch (...) { ctx.replyError("ERR min or max is not a float"); return; }

        // MiniRedis 兼容: (1.0 2.0 表示 1.0 < score < 2.0，边界处理从简
        int64_t count = 0;
        for (auto& e : z->by_score) {
            if (e.score >= min_score && e.score <= max_score) count++;
        }
        ctx.replyInteger(count);
    }

private:
    static Value* getExisting(CmdContext& ctx) {
        std::string key(ctx.args[1]);
        Value* v = ctx.engine->find(key);
        if (!v || v->isExpired(ctx.nowMs())) return nullptr;
        if (v->type != ValueType::ZSET) { ctx.replyWrongType(); return nullptr; }
        return v;
    }
    static Value* getOrCreate(CmdContext& ctx) {
        std::string key(ctx.args[1]);
        Value* v = ctx.engine->find(key);
        if (v && v->isExpired(ctx.nowMs())) { ctx.engine->remove(key); v = nullptr; }
        if (!v) { ctx.engine->insert(key, Value::createZSet()); v = ctx.engine->find(key); }
        if (v && v->type != ValueType::ZSET) { ctx.replyWrongType(); return nullptr; }
        return v;
    }
};

} // namespace ledis
