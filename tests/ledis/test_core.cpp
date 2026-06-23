#include <gtest/gtest.h>
#include "ledis_helpers.h"
#include "ledis/core/dict.h"
#include "ledis/core/value.h"
#include "ledis/core/eviction.h"
#include "ledis/core/command.h"
#include "ledis/core/storage_engine.h"

using namespace ledis;
using namespace kvtest;

class LedisCoreTest : public ::testing::Test {
protected:
    void SetUp() override { ensureCommandTable(); }
    StorageEngine engine;
};

TEST_F(LedisCoreTest, DictInsertFindRemove) {
    Dict d;
    auto* v = d.insert("foo", Value::createString("bar"));
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(d.find("foo")->str, "bar");
    EXPECT_EQ(d.size(), 1u);

    // 触发 resize
    for (int i = 0; i < 2000; ++i)
        d.insert("k" + std::to_string(i), Value::createString("v"));

  Value removed = d.remove("foo");
    EXPECT_EQ(removed.str, "bar");
    EXPECT_EQ(d.find("foo"), nullptr);
}

TEST_F(LedisCoreTest, DictIterator) {
    Dict d;
    d.insert("a", Value::createString("1"));
    d.insert("b", Value::createString("2"));
    int count = 0;
    for (Dict::Iterator it(&d); it.valid(); it.next()) ++count;
    EXPECT_EQ(count, 2);
}

TEST_F(LedisCoreTest, ValueTypes) {
    auto s = Value::createString("s");
    EXPECT_EQ(s.type, ValueType::STRING);
    auto h = Value::createHash();
    EXPECT_EQ(h.type, ValueType::HASH);
    auto l = Value::createList();
    EXPECT_EQ(l.type, ValueType::LIST);
    auto st = Value::createSet();
    EXPECT_EQ(st.type, ValueType::SET);
    auto z = Value::createZSet();
    EXPECT_EQ(z.type, ValueType::ZSET);
    auto stream = Value::createStream();
    EXPECT_EQ(stream.type, ValueType::STREAM);
}

TEST_F(LedisCoreTest, CommandLookupAndArity) {
    const CmdInfo* set = lookupCommand("set");
    ASSERT_NE(set, nullptr);
    EXPECT_TRUE(set->checkArity(3));
    EXPECT_FALSE(set->checkArity(2));

    const CmdInfo* mget = lookupCommand("mget");
    ASSERT_NE(mget, nullptr);
    EXPECT_TRUE(mget->checkArity(2));
    EXPECT_TRUE(mget->checkArity(5));
}

TEST_F(LedisCoreTest, DispatchUnknownAndWrongArgs) {
    auto r1 = runCommand(engine, {"nosuchcmd"});
    EXPECT_NE(r1.find("unknown command"), std::string::npos);

    auto r2 = runCommand(engine, {"set", "onlyone"});
    EXPECT_NE(r2.find("wrong number"), std::string::npos);
}

TEST_F(LedisCoreTest, GetSlotAndHashtag) {
    uint16_t s1 = StorageEngine::getSlot("mykey");
    uint16_t s2 = StorageEngine::getSlot("prefix{tag}suffix");
    uint16_t s3 = StorageEngine::getSlot("{tag}other");
    EXPECT_EQ(s2, s3);
    EXPECT_LT(s1, 16384u);
}

TEST_F(LedisCoreTest, EvictionLRU) {
    Dict d;
    for (int i = 0; i < 50; ++i) {
        auto v = Value::createString("v");
        v.lru = static_cast<uint32_t>(i);
        d.insert("k" + std::to_string(i), std::move(v));
    }
    EvictionManager ev;
    ev.setMaxmemory(100);
    ev.setPolicy(EVICT_ALLKEYS_LRU);
    int n = ev.evict(d, 50);
    EXPECT_GT(n, 0);
}

TEST_F(LedisCoreTest, EvictionPolicyNames) {
    EXPECT_STREQ(evictionPolicyName(EVICT_ALLKEYS_LRU), "allkeys-lru");
    EXPECT_EQ(evictionPolicyFromString("volatile-ttl"), EVICT_VOLATILE_TTL);
    EXPECT_EQ(evictionPolicyFromString("unknown"), EVICT_NOEVICTION);
}

TEST_F(LedisCoreTest, ActiveExpire) {
    runCommand(engine, {"SET", "expkey", "val"});
    runCommand(engine, {"EXPIRE", "expkey", "1"});
    EXPECT_TRUE(engine.checkExpired("expkey", CmdContext::nowMs() + 2000));
    engine.activeExpireCycle();
}
