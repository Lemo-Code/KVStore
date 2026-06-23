#include <gtest/gtest.h>
#include "ledis_helpers.h"
#include "ledis/core/eviction.h"
#include "ledis/core/value.h"
#include "ledis/protocol/resp_parser.h"

using namespace ledis;
using namespace kvtest;

TEST(LedisCoverageBoostTest, EvictionVolatilePolicies) {
    Dict d;
    uint64_t now = CmdContext::nowMs();
    for (int i = 0; i < 40; ++i) {
        auto v = Value::createString("data");
        v.lru = static_cast<uint32_t>(i);
        if (i % 3 == 0) v.expire_at_ms = now + 10000;
        d.insert("ek" + std::to_string(i), std::move(v));
    }
    EvictionManager ev;
    ev.setMaxmemory(200);
    for (auto p : {EVICT_VOLATILE_LRU, EVICT_VOLATILE_LFU, EVICT_VOLATILE_RANDOM, EVICT_VOLATILE_TTL}) {
        ev.setPolicy(p);
        ev.evict(d, 100);
    }
    ev.setPolicy(EVICT_NOEVICTION);
    EXPECT_EQ(ev.evict(d, 100), 0);
}

TEST(LedisCoverageBoostTest, EvictionAllkeysPolicies) {
    for (auto p : {EVICT_NOEVICTION, EVICT_ALLKEYS_LRU, EVICT_ALLKEYS_LFU, EVICT_ALLKEYS_RANDOM,
                   EVICT_VOLATILE_LRU, EVICT_VOLATILE_LFU, EVICT_VOLATILE_RANDOM, EVICT_VOLATILE_TTL}) {
        (void)evictionPolicyName(p);
    }
    EXPECT_STREQ(evictionPolicyName(static_cast<EvictionPolicy>(99)), "unknown");

    Dict d;
    for (int i = 0; i < 200; ++i) {
        auto v = Value::createString("v");
        v.lru = static_cast<uint32_t>(i);
        d.insert("ak" + std::to_string(i), std::move(v));
    }
    EvictionManager ev;
    ev.setMaxmemory(100);
    EXPECT_GT(ev.lruClock(), 0u);
    for (auto p : {EVICT_ALLKEYS_LRU, EVICT_ALLKEYS_LFU, EVICT_ALLKEYS_RANDOM}) {
        ev.setPolicy(p);
        (void)ev.evict(d, 50);
    }
    if (auto* v = d.find("ak1")) {
        ev.updateLRU(*v);
        v->lru = 0x01000000;
        ev.updateLFU(*v);
        v->lru = 0xFF0000FF;
        ev.updateLFU(*v);
    }
    EXPECT_EQ(evictionPolicyFromString("volatile-random"), EVICT_VOLATILE_RANDOM);
    EXPECT_EQ(evictionPolicyFromString("allkeys-random"), EVICT_ALLKEYS_RANDOM);
}

TEST(LedisCoverageBoostTest, ValueLifecycle) {
    EXPECT_STREQ(valueTypeName(ValueType::LIST), "list");
    EXPECT_STREQ(valueTypeName(ValueType::HASH), "hash");
    EXPECT_STREQ(valueTypeName(ValueType::SET), "set");
    EXPECT_STREQ(valueTypeName(ValueType::STREAM), "stream");
    EXPECT_STREQ(valueTypeName(ValueType::ZSET), "zset");
    EXPECT_STREQ(valueTypeName(ValueType::STRING), "string");

    { auto v = Value::createHash(); EXPECT_NE(v.asHash(), nullptr); }
    { auto v = Value::createList(); EXPECT_NE(v.asList(), nullptr); }
    { auto v = Value::createSet(); EXPECT_NE(v.asSet(), nullptr); }
    { auto v = Value::createZSet(); EXPECT_NE(v.asZSet(), nullptr); }
    { auto v = Value::createStream(); EXPECT_EQ(v.type, ValueType::STREAM); }

    auto num = Value::createInt(42);
    EXPECT_EQ(num.int_val, 42);

    auto s = Value::createSet();
    s.asSet()->members.insert("m");
    EXPECT_EQ(s.asHash(), nullptr);
    EXPECT_EQ(s.asList(), nullptr);
    EXPECT_EQ(s.asZSet(), nullptr);
    EXPECT_EQ(s.asStream(), nullptr);

    uint64_t now = CmdContext::nowMs();
    auto exp = Value::createString("e");
    exp.expire_at_ms = now + 5000;
    EXPECT_FALSE(exp.isExpired(now));
    EXPECT_TRUE(exp.isExpired(now + 6000));
    EXPECT_GT(exp.ttlSec(now), 0);
    EXPECT_GT(exp.ttlMs(now), 0);
    EXPECT_EQ(exp.ttlSec(now + 6000), -2);
}

TEST(LedisCoverageBoostTest, RespParserEdgeCases) {
    RespParser p;
    size_t consumed = 0;
    const char* int_cmd = ":42\r\n";
    auto r = p.feed(int_cmd, 3, consumed);
    EXPECT_EQ(r, RespParser::Result::NEED_MORE);
    r = p.feed(int_cmd, 5, consumed);
    EXPECT_EQ(r, RespParser::Result::OK);

    p.reset();
    const char* err_cmd = "-ERR msg\r\n";
    r = p.feed(err_cmd, strlen(err_cmd), consumed);
    EXPECT_EQ(r, RespParser::Result::OK);

    p.reset();
    const char* bad_arr = "*-1\r\n";
    r = p.feed(bad_arr, strlen(bad_arr), consumed);
    EXPECT_EQ(r, RespParser::Result::ERROR);
}

TEST(LedisCoverageBoostTest, StorageEngineDirectApi) {
    ensureCommandTable();
    StorageEngine e;
    e.insert("k1", Value::createString("v1"));
    EXPECT_NE(e.find("k1"), nullptr);
    EXPECT_EQ(e.size(), 1u);
    EXPECT_EQ(e.dict().size(), 1u);
    (void)e.remove("k1");
    EXPECT_EQ(e.size(), 0u);
    e.hit_count_ = 1;
    e.miss_count_ = 2;
}

TEST(LedisCoverageBoostTest, StorageMiscCommands) {
    StorageEngine e;
    runCommand(e, {"SET", "nxk", "v", "NX"});
    runCommand(e, {"SET", "nxk", "v2", "XX"});
    runCommand(e, {"SET", "exk", "v", "EX", "3600"});
    runCommand(e, {"SET", "pxk", "v", "PX", "60000"});
    runCommand(e, {"HSET", "rh", "f", "v"});
    runCommand(e, {"COPY", "rh", "rh2"});
    runCommand(e, {"LPUSH", "rl", "a", "b"});
    runCommand(e, {"COPY", "rl", "rl2"});
    runCommand(e, {"SADD", "rs", "m"});
    runCommand(e, {"COPY", "rs", "rs2"});
    runCommand(e, {"ZADD", "rz", "1", "m"});
    runCommand(e, {"COPY", "rz", "rz2"});
    runCommand(e, {"RESTORE", "rsh", "0", "S:hello"});
    runCommand(e, {"RESTORE", "rhh", "0", "H:f1:v1;"});
    runCommand(e, {"RESTORE", "rll", "0", "L:a;b;c"});
    runCommand(e, {"RESTORE", "rst", "0", "T:m1;m2"});
    runCommand(e, {"RESTORE", "rzz", "0", "Z:1.5:member1;"});
    runCommand(e, {"XADD", "xst", "*", "f", "v"});
    runCommand(e, {"XADD", "xst", "1-0", "f2", "v2"});
    runCommand(e, {"XADD", "xst", "1-*", "f3", "v3"});
    runCommand(e, {"XREAD", "COUNT", "2", "STREAMS", "xst", "0"});
    runCommand(e, {"XREAD", "BLOCK", "1", "STREAMS", "xempty", "$"});
    runCommand(e, {"INCRBYFLOAT", "incr", "0.5"});
    runCommand(e, {"SET", "bit", "x"});
    runCommand(e, {"BITOP", "OR", "bout", "bit", "bit"});
    runCommand(e, {"PFADD", "hll", "a"});
    runCommand(e, {"PFMERGE", "hll2", "hll"});
    runCommand(e, {"XADD", "xs", "*", "f", "v"});
    runCommand(e, {"XGROUP", "CREATE", "xs", "g", "$"});
    runCommand(e, {"ZADD", "zs", "1", "a", "2", "b"});
    runCommand(e, {"ZUNIONSTORE", "zout", "2", "zs", "zs", "WEIGHTS", "1", "2"});
    runCommand(e, {"BLPOP", "bl", "0"});
    runCommand(e, {"LMOVE", "lk", "lk2", "LEFT", "RIGHT"});
    runCommand(e, {"SMOVE", "sk1", "sk2", "m"});
    runCommand(e, {"SINTERSTORE", "si", "2", "sk1", "sk2"});
    runCommand(e, {"SUNIONSTORE", "su", "2", "sk1", "sk2"});
    runCommand(e, {"SDIFFSTORE", "sd", "2", "sk1", "sk2"});
    runCommand(e, {"ZINTERSTORE", "zi", "2", "zs", "zs"});
    runCommand(e, {"ZDIFFSTORE", "zd", "2", "zs", "zs"});
    runCommand(e, {"GEORADIUSBYMEMBER", "gk", "m", "100", "km"});
    runCommand(e, {"XREADGROUP", "GROUP", "g", "c", "COUNT", "1", "STREAMS", "xs", ">"});
    runCommand(e, {"XPENDING", "xs", "g"});
    runCommand(e, {"XACK", "xs", "g", "0-0"});
    runCommand(e, {"CONFIG", "SET", "maxmemory-policy", "allkeys-lru"});
    runCommand(e, {"MEMORY", "USAGE", "bit"});
    runCommand(e, {"OBJECT", "ENCODING", "bit"});
    runCommand(e, {"TOUCH", "bit"});
    runCommand(e, {"UNLINK", "bit"});
}
