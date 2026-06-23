#include <gtest/gtest.h>
#include <memory>
#include "ledis_helpers.h"

using namespace ledis;
using namespace kvtest;

class StorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        ensureCommandTable();
        engine = std::make_unique<StorageEngine>();
    }
    std::unique_ptr<StorageEngine> engine;

    std::string cmd(std::initializer_list<const char*> args) {
        return runCommand(*engine, args);
    }
};

// ---- String ----
TEST_F(StorageTest, StringBasics) {
    EXPECT_TRUE(isOK(cmd({"SET", "s1", "hello"})));
    EXPECT_EQ(extractBulk(cmd({"GET", "s1"})), "hello");
    EXPECT_EQ(extractInteger(cmd({"STRLEN", "s1"})), 5);
    EXPECT_EQ(extractInteger(cmd({"APPEND", "s1", " world"})), 11);
    EXPECT_EQ(extractBulk(cmd({"GET", "s1"})), "hello world");
}

TEST_F(StorageTest, StringIncrDecr) {
    cmd({"SET", "n", "10"});
    EXPECT_EQ(extractInteger(cmd({"INCR", "n"})), 11);
    EXPECT_EQ(extractInteger(cmd({"INCRBY", "n", "5"})), 16);
    EXPECT_EQ(extractInteger(cmd({"DECR", "n"})), 15);
    EXPECT_EQ(extractInteger(cmd({"DECRBY", "n", "3"})), 12);
}

TEST_F(StorageTest, StringMgetMset) {
    cmd({"MSET", "a", "1", "b", "2"});
    auto r = cmd({"MGET", "a", "b", "c"});
    EXPECT_NE(r.find("$1"), std::string::npos);
    EXPECT_TRUE(isNull(cmd({"GET", "c"})));
}

TEST_F(StorageTest, StringSetNxEx) {
    EXPECT_EQ(extractInteger(cmd({"SETNX", "nx", "v"})), 1);
    EXPECT_EQ(extractInteger(cmd({"SETNX", "nx", "v2"})), 0);
    EXPECT_TRUE(isOK(cmd({"SETEX", "ex", "60", "data"})));
    EXPECT_EQ(extractBulk(cmd({"GET", "ex"})), "data");
    EXPECT_TRUE(isOK(cmd({"PSETEX", "pex", "1000", "pd"})));
}

TEST_F(StorageTest, StringGetSetRange) {
    cmd({"SET", "r", "abcdef"});
    EXPECT_EQ(extractBulk(cmd({"GETSET", "r", "xyz"})), "abcdef");
    EXPECT_EQ(extractBulk(cmd({"GETRANGE", "r", "0", "1"})), "xy");
    EXPECT_EQ(extractInteger(cmd({"SETRANGE", "r", "1", "ZZ"})), 3);
    EXPECT_EQ(extractInteger(cmd({"GETDEL", "r"})), 0);
}

TEST_F(StorageTest, StringRename) {
    cmd({"SET", "old", "val"});
    EXPECT_TRUE(isOK(cmd({"RENAME", "old", "new"})));
    EXPECT_TRUE(isNull(cmd({"GET", "old"})));
    EXPECT_EQ(extractBulk(cmd({"GET", "new"})), "val");
    cmd({"SET", "x", "1"});
    EXPECT_EQ(extractInteger(cmd({"RENAMENX", "new", "x"})), 0);
}

// ---- Hash ----
TEST_F(StorageTest, HashOps) {
    EXPECT_EQ(extractInteger(cmd({"HSET", "h", "f1", "v1"})), 1);
    EXPECT_EQ(extractBulk(cmd({"HGET", "h", "f1"})), "v1");
    EXPECT_EQ(extractInteger(cmd({"HEXISTS", "h", "f1"})), 1);
    EXPECT_EQ(extractInteger(cmd({"HLEN", "h"})), 1);
    EXPECT_TRUE(isOK(cmd({"HMSET", "h", "f2", "v2", "f3", "v3"})));
    EXPECT_EQ(extractInteger(cmd({"HINCRBY", "h", "counter", "5"})), 5);
    cmd({"HINCRBYFLOAT", "h", "score", "1.5"});
    EXPECT_EQ(extractInteger(cmd({"HSETNX", "h", "f1", "dup"})), 0);
    EXPECT_EQ(extractInteger(cmd({"HDEL", "h", "f3"})), 1);
    EXPECT_EQ(extractInteger(cmd({"HSTRLEN", "h", "f1"})), 2);
}

// ---- List ----
TEST_F(StorageTest, ListOps) {
    EXPECT_EQ(extractInteger(cmd({"LPUSH", "l", "a", "b"})), 2);
    EXPECT_EQ(extractInteger(cmd({"RPUSH", "l", "c"})), 3);
    EXPECT_EQ(extractInteger(cmd({"LLEN", "l"})), 3);
    EXPECT_EQ(extractBulk(cmd({"LINDEX", "l", "0"})), "b");
    EXPECT_TRUE(isOK(cmd({"LSET", "l", "0", "B"})));
    EXPECT_EQ(extractInteger(cmd({"LREM", "l", "1", "c"})), 1);
    EXPECT_TRUE(isOK(cmd({"LTRIM", "l", "0", "0"})));
    EXPECT_EQ(extractBulk(cmd({"LPOP", "l"})), "B");
}

// ---- Set ----
TEST_F(StorageTest, SetOps) {
    EXPECT_EQ(extractInteger(cmd({"SADD", "s", "a", "b", "c"})), 3);
    EXPECT_EQ(extractInteger(cmd({"SCARD", "s"})), 3);
    EXPECT_EQ(extractInteger(cmd({"SISMEMBER", "s", "a"})), 1);
    EXPECT_EQ(extractInteger(cmd({"SREM", "s", "b"})), 1);
    cmd({"SPOP", "s"});
    cmd({"SRANDMEMBER", "s"});
    cmd({"SMISMEMBER", "s", "a", "z"});
}

TEST_F(StorageTest, SetAlgebra) {
    cmd({"SADD", "s1", "a", "b"});
    cmd({"SADD", "s2", "b", "c"});
    cmd({"SINTER", "s1", "s2"});
    cmd({"SUNION", "s1", "s2"});
    cmd({"SDIFF", "s1", "s2"});
    cmd({"SINTERSTORE", "dst", "s1", "s2"});
    cmd({"SUNIONSTORE", "dst2", "s1", "s2"});
    cmd({"SDIFFSTORE", "dst3", "s1", "s2"});
    EXPECT_EQ(extractInteger(cmd({"SMOVE", "s1", "s2", "a"})), 1);
}

// ---- ZSet ----
TEST_F(StorageTest, ZSetOps) {
    EXPECT_EQ(extractInteger(cmd({"ZADD", "z", "1", "a", "2", "b", "3", "c"})), 3);
    EXPECT_EQ(extractInteger(cmd({"ZCARD", "z"})), 3);
    EXPECT_EQ(extractBulk(cmd({"ZSCORE", "z", "a"})), "1");
    EXPECT_EQ(extractInteger(cmd({"ZRANK", "z", "a"})), 0);
    EXPECT_EQ(extractInteger(cmd({"ZREVRANK", "z", "c"})), 0);
    cmd({"ZRANGE", "z", "0", "-1"});
    cmd({"ZREVRANGE", "z", "0", "-1"});
    cmd({"ZRANGEBYSCORE", "z", "1", "2"});
    cmd({"ZCOUNT", "z", "1", "3"});
    cmd({"ZINCRBY", "z", "5", "a"});
    cmd({"ZREM", "z", "c"});
    cmd({"ZREMRANGEBYRANK", "z", "0", "0"});
    cmd({"ZREMRANGEBYSCORE", "z", "0", "1"});
    cmd({"ZPOPMIN", "z", "1"});
    cmd({"ZPOPMAX", "z", "1"});
    cmd({"ZRANDMEMBER", "z", "1"});
    cmd({"ZLEXCOUNT", "z", "-", "+"});
    cmd({"ZRANGEBYLEX", "z", "-", "+"});
    cmd({"ZREMRANGEBYLEX", "z", "-", "+"});
    cmd({"ZREVRANGEBYSCORE", "z", "10", "0"});
}

TEST_F(StorageTest, ZSetStore) {
    cmd({"ZADD", "za", "1", "a", "2", "b"});
    cmd({"ZADD", "zb", "2", "b", "3", "c"});
    cmd({"ZINTER", "2", "za", "zb", "WEIGHTS", "1", "1"});
    cmd({"ZUNION", "2", "za", "zb"});
    cmd({"ZDIFF", "2", "za", "zb"});
    cmd({"ZINTERSTORE", "zdst", "2", "za", "zb"});
    cmd({"ZUNIONSTORE", "zdst2", "2", "za", "zb"});
    cmd({"ZDIFFSTORE", "zdst3", "2", "za", "zb"});
}

// ---- Bitmap ----
TEST_F(StorageTest, BitmapOps) {
    EXPECT_EQ(extractInteger(cmd({"SETBIT", "bm", "0", "1"})), 0);
    EXPECT_EQ(extractInteger(cmd({"GETBIT", "bm", "0"})), 1);
    cmd({"BITCOUNT", "bm"});
    cmd({"BITOP", "AND", "bmdst", "bm", "bm"});
    cmd({"BITPOS", "bm", "1"});
}

// ---- HyperLogLog ----
TEST_F(StorageTest, HyperLogLog) {
    EXPECT_EQ(extractInteger(cmd({"PFADD", "hll", "a", "b", "c"})), 1);
    EXPECT_GE(extractInteger(cmd({"PFCOUNT", "hll"})), 0);
    cmd({"PFADD", "hll2", "d"});
    cmd({"PFMERGE", "hll_merged", "hll", "hll2"});
}

// ---- Geo ----
TEST_F(StorageTest, GeoOps) {
    cmd({"GEOADD", "geo", "116.4", "39.9", "beijing"});
    cmd({"GEOADD", "geo", "121.5", "31.2", "shanghai"});
    cmd({"GEODIST", "geo", "beijing", "shanghai", "km"});
    cmd({"GEOHASH", "geo", "beijing"});
    cmd({"GEOPOS", "geo", "beijing"});
    cmd({"GEORADIUS", "geo", "116.4", "39.9", "1000", "km"});
}

// ---- Stream ----
TEST_F(StorageTest, StreamOps) {
    auto id = extractBulk(cmd({"XADD", "mystream", "*", "f1", "v1"}));
    EXPECT_FALSE(id.empty());
    EXPECT_EQ(extractInteger(cmd({"XLEN", "mystream"})), 1);
    cmd({"XRANGE", "mystream", "-", "+"});
    cmd({"XGROUP", "CREATE", "mystream", "grp", "0"});
    cmd({"XREADGROUP", "GROUP", "grp", "c1", "COUNT", "1", "STREAMS", "mystream", ">"});
    cmd({"XPENDING", "mystream", "grp"});
    cmd({"XACK", "mystream", "grp", id.c_str()});
    cmd({"XDEL", "mystream", id.c_str()});
}

// ---- Server / Keys ----
TEST_F(StorageTest, ServerOps) {
    cmd({"SET", "k1", "v"});
    cmd({"SET", "k2", "v"});
    EXPECT_GE(extractInteger(cmd({"DBSIZE"})), 2);
    cmd({"KEYS", "*"});
    cmd({"RANDOMKEY"});
    cmd({"EXISTS", "k1", "k2", "missing"});
    EXPECT_EQ(extractSimple(cmd({"TYPE", "k1"})), "string");
    cmd({"TTL", "k1"});
    cmd({"PTTL", "k1"});
    cmd({"EXPIREAT", "k1", "9999999999"});
    cmd({"PEXPIREAT", "k2", "9999999999000"});
    cmd({"PERSIST", "k1"});
    cmd({"EXPIRETIME", "k1"});
    cmd({"PEXPIRETIME", "k1"});
    cmd({"TOUCH", "k1", "k2"});
    cmd({"OBJECT", "ENCODING", "k1"});
    cmd({"MEMORY", "USAGE", "k1"});
    cmd({"TIME"});
    cmd({"INFO", "server"});
    cmd({"COMMAND"});
    cmd({"CONFIG", "GET", "maxmemory"});
    cmd({"CLIENT", "LIST"});
    cmd({"SLOWLOG", "LEN"});
    cmd({"SELECT", "0"});
    cmd({"HELLO", "3"});
    cmd({"FLUSHDB"});
    EXPECT_EQ(extractInteger(cmd({"DBSIZE"})), 0);
}

// ---- Scan ----
TEST_F(StorageTest, ScanOps) {
    for (int i = 0; i < 20; ++i)
        cmd({"SET", ("sk" + std::to_string(i)).c_str(), "v"});
    cmd({"SCAN", "0"});
    cmd({"HSET", "sh", "f", "v"});
    cmd({"HSCAN", "sh", "0"});
    cmd({"SADD", "ss", "m"});
    cmd({"SSCAN", "ss", "0"});
    cmd({"ZADD", "sz", "1", "m"});
    cmd({"ZSCAN", "sz", "0"});
}

// ---- Sort / Copy ----
TEST_F(StorageTest, SortAndCopy) {
    cmd({"LPUSH", "sortl", "3", "1", "2"});
    cmd({"SORT", "sortl"});
    cmd({"SET", "cp", "val"});
    cmd({"COPY", "cp", "cp2"});
}

// ---- Pub/Sub (engine level publish) ----
TEST_F(StorageTest, Publish) {
    EXPECT_EQ(extractInteger(cmd({"PUBLISH", "ch", "msg"})), 0);
    cmd({"PUBSUB", "CHANNELS"});
}

// ---- Expire edge ----
TEST_F(StorageTest, ExpireVariants) {
    cmd({"SET", "e", "v"});
    cmd({"EXPIRE", "e", "100"});
    cmd({"PEXPIRE", "e", "100000"});
}

// ---- List extras ----
TEST_F(StorageTest, ListExtras) {
    cmd({"RPUSH", "le", "a", "b", "a", "c"});
    cmd({"LPOS", "le", "a"});
    cmd({"LMOVE", "le", "le2", "LEFT", "RIGHT"});
}
