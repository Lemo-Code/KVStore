#include <gtest/gtest.h>
#include <fstream>
#include "ledis_helpers.h"
#include "ledis/core/eviction.h"

using namespace ledis;
using namespace kvtest;

class StorageExtendedTest : public ::testing::Test {
protected:
    void SetUp() override { ensureCommandTable(); }
    StorageEngine engine;
    std::string cmd(std::initializer_list<const char*> a) {
        return runCommand(engine, a);
    }
};

TEST_F(StorageExtendedTest, StringEdgeCases) {
    cmd({"SET", "e", "1"});
    cmd({"INCRBYFLOAT", "e", "2.5"});
    cmd({"GET", "e"});
    cmd({"MSETNX", "nx1", "a", "nx2", "b"});
    cmd({"INCRBYFLOAT", "nf", "1.1"});
    EXPECT_TRUE(isNull(cmd({"GET", "missing"})));
    cmd({"SET", "empty", ""});
    EXPECT_EQ(extractBulk(cmd({"GET", "empty"})), "");
}

TEST_F(StorageExtendedTest, HashEdgeCases) {
    cmd({"HSET", "h", "f", "v"});
    cmd({"HMGET", "h", "f", "x"});
    cmd({"HINCRBYFLOAT", "h", "score", "0.5"});
    cmd({"HRANDFIELD", "h", "1"});
}

TEST_F(StorageExtendedTest, ListBlockingPop) {
    cmd({"RPUSH", "bl", "item"});
    cmd({"LPOP", "bl"});
    cmd({"BZPOPMIN", "bl", "0"});
    cmd({"BZPOPMAX", "bl2", "0"});
}

TEST_F(StorageExtendedTest, SetMoreOps) {
    cmd({"SADD", "s", "a", "b"});
    cmd({"SRANDMEMBER", "s", "2"});
    cmd({"SPOP", "s", "1"});
}

TEST_F(StorageExtendedTest, ZSetMoreOps) {
    cmd({"ZADD", "z", "1", "a", "2", "b"});
    cmd({"ZREVRANGEBYSCORE", "z", "10", "0", "WITHSCORES"});
    cmd({"ZDIFF", "2", "z", "z"});
    cmd({"ZDIFFSTORE", "zd", "2", "z", "z"});
}

TEST_F(StorageExtendedTest, BitmapEdge) {
    cmd({"SETBIT", "b", "7", "1"});
    cmd({"BITCOUNT", "b", "0", "0"});
    cmd({"BITPOS", "b", "1"});
}

TEST_F(StorageExtendedTest, StreamEdge) {
    auto id = extractBulk(cmd({"XADD", "s", "*", "k", "v"}));
    cmd({"XREAD", "COUNT", "1", "STREAMS", "s", "0"});
    cmd({"XGROUP", "CREATE", "s", "g", "0", "MKSTREAM"});
}

TEST_F(StorageExtendedTest, GeoEdge) {
    cmd({"GEOADD", "g", "0", "0", "origin"});
    cmd({"GEORADIUS", "g", "0", "0", "100", "m", "WITHDIST"});
}

TEST_F(StorageExtendedTest, ServerCommands) {
    cmd({"SET", "obj", "x"});
    cmd({"OBJECT", "REFCOUNT", "obj"});
    cmd({"MEMORY", "STATS"});
    cmd({"COMMAND", "COUNT"});
    cmd({"CONFIG", "SET", "maxmemory", "0"});
    cmd({"CLIENT", "ID"});
    cmd({"SLOWLOG", "GET", "10"});
    cmd({"RESTORE", "restored", "0", "payload"});
    cmd({"COPY", "obj", "obj2"});
    cmd({"UNLINK", "obj2"});
}

TEST_F(StorageExtendedTest, SortVariants) {
    cmd({"SADD", "sorts", "3", "1", "2"});
    cmd({"SORT", "sorts", "ALPHA"});
    cmd({"LPUSH", "sortl", "c", "a", "b"});
    cmd({"SORT", "sortl"});
}

TEST_F(StorageExtendedTest, EvictionAllPolicies) {
    Dict d;
    for (int i = 0; i < 30; ++i) {
        auto v = Value::createString("v");
        v.lru = static_cast<uint32_t>(i);
        v.lfu = static_cast<uint8_t>(i);
        if (i % 2 == 0) v.expire_at_ms = CmdContext::nowMs() + 1000;
        d.insert("k" + std::to_string(i), std::move(v));
    }
    EvictionManager ev;
    ev.setMaxmemory(100);
    for (auto p : {EVICT_ALLKEYS_LFU, EVICT_ALLKEYS_RANDOM,
                   EVICT_VOLATILE_LRU, EVICT_VOLATILE_LFU,
                   EVICT_VOLATILE_RANDOM, EVICT_VOLATILE_TTL}) {
        ev.setPolicy(p);
        ev.evict(d, 50);
    }
    if (auto* v = d.find("k0")) ev.updateLRU(*v);
    if (auto* v = d.find("k1")) ev.updateLFU(*v);
}

TEST_F(StorageExtendedTest, WrongTypeErrors) {
    cmd({"SET", "ws", "hello"});
    EXPECT_NE(cmd({"HGET", "ws", "f"}).find("WRONGTYPE"), std::string::npos);
    cmd({"LPUSH", "wl", "x"});
    EXPECT_NE(cmd({"HGET", "wl", "f"}).find("WRONGTYPE"), std::string::npos);
}

TEST_F(StorageExtendedTest, ScanWithMatch) {
    cmd({"SET", "scan:1", "v"});
    cmd({"SET", "scan:2", "v"});
    cmd({"SCAN", "0", "MATCH", "scan:*"});
}
