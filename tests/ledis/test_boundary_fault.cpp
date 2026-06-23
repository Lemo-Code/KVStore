#include <gtest/gtest.h>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include "fault_helpers.h"
#include "ledis_helpers.h"
#include "temp_file.h"
#include "ledis/protocol/resp_parser.h"
#include "ledis/protocol/resp_writer.h"
#include "ledis/core/eviction.h"
#include "ledis/cluster/cluster_topology.h"
#include "ledis/cluster/cluster_manager.h"
#include "ledis/cluster/cluster_config.h"
#include "ledis/replication/aof_writer.h"

using namespace ledis;
using namespace ledis::cluster;
using namespace kvtest;

namespace {

RespParser::Result feedAll(RespParser& p, const std::string& data) {
    size_t consumed = 0;
    return p.feed(data.data(), data.size(), consumed);
}

bool hasErr(const std::string& resp, const char* needle) {
    return resp.find(needle) != std::string::npos;
}

} // namespace

// ---- RESP 协议边界 / 故障注入 ----

TEST(RespBoundaryFaultTest, EmptyAndZeroLengthFeed) {
    RespParser p;
    size_t consumed = 99;
    EXPECT_EQ(p.feed("", 0, consumed), RespParser::Result::NEED_MORE);
    EXPECT_EQ(consumed, 0u);
}

TEST(RespBoundaryFaultTest, TruncatedArrayHeader) {
    const char* partial = "*3\r";
    RespParser p;
    size_t consumed = 0;
    EXPECT_EQ(p.feed(partial, 3, consumed), RespParser::Result::NEED_MORE);
    EXPECT_FALSE(p.errorMsg().empty() == false && p.isIdle());
}

TEST(RespBoundaryFaultTest, InvalidArrayLengthToken) {
    RespParser p;
    size_t consumed = 0;
    auto r = feedAll(p, "*abc\r\n");
    EXPECT_EQ(r, RespParser::Result::ERROR);
    EXPECT_NE(p.errorMsg().find("invalid array length"), std::string::npos);
}

TEST(RespBoundaryFaultTest, NullArrayRejected) {
    RespParser p;
    auto r = feedAll(p, "*-1\r\n");
    EXPECT_EQ(r, RespParser::Result::ERROR);
    EXPECT_NE(p.errorMsg().find("null array"), std::string::npos);
}

TEST(RespBoundaryFaultTest, InvalidBulkLength) {
    RespParser p;
    auto r = feedAll(p, "$xyz\r\n");
    EXPECT_EQ(r, RespParser::Result::ERROR);
    EXPECT_NE(p.errorMsg().find("invalid bulk length"), std::string::npos);
}

TEST(RespBoundaryFaultTest, BulkDataTruncated) {
    RespParser p;
    size_t consumed = 0;
    auto r = p.feed("*1\r\n$5\r\nab", 11, consumed);
    EXPECT_EQ(r, RespParser::Result::NEED_MORE);
    EXPECT_TRUE(p.args().empty());
}

TEST(RespBoundaryFaultTest, EmptyArrayCommand) {
    RespParser p;
    auto r = feedAll(p, "*0\r\n");
    EXPECT_EQ(r, RespParser::Result::NEED_MORE);
    EXPECT_TRUE(p.args().empty());
}

TEST(RespBoundaryFaultTest, IntegerAtTopLevel) {
    RespParser p;
    auto r = feedAll(p, ":42\r\n");
    EXPECT_EQ(r, RespParser::Result::OK);
    ASSERT_EQ(p.args().size(), 1u);
    EXPECT_EQ(p.args()[0], "42");
}

TEST(RespBoundaryFaultTest, ErrorLineAtTopLevel) {
    RespParser p;
    auto r = feedAll(p, "-ERR boom\r\n");
    EXPECT_EQ(r, RespParser::Result::OK);
    EXPECT_EQ(p.args()[0], "ERR boom");
}

TEST(RespBoundaryFaultTest, OversizedBulkIncremental) {
    std::string cmd = "*1\r\n$5\r\n";
    RespParser p;
    size_t consumed = 0;
    EXPECT_EQ(p.feed(cmd.data(), cmd.size(), consumed), RespParser::Result::NEED_MORE);
    EXPECT_EQ(p.feed("hello\r\n", 7, consumed), RespParser::Result::OK);
    ASSERT_EQ(p.args().size(), 1u);
    EXPECT_EQ(p.args()[0], "hello");
}

// ---- 存储引擎边界 / 故障注入 ----

class StorageBoundaryFaultTest : public ::testing::Test {
protected:
    void SetUp() override { ensureCommandTable(); }
    StorageEngine engine;
    std::string cmd(std::initializer_list<const char*> a) {
        return runCommand(engine, a);
    }
};

TEST_F(StorageBoundaryFaultTest, ArityAndUnknownCommand) {
    EXPECT_TRUE(hasErr(cmd({"nosuch"}), "unknown command"));
    EXPECT_TRUE(hasErr(cmd({"set"}), "wrong number"));
    EXPECT_TRUE(hasErr(cmd({"get"}), "wrong number"));
    EXPECT_TRUE(hasErr(cmd({"mget"}), "wrong number"));
}

TEST_F(StorageBoundaryFaultTest, IncrTypeAndRangeFaults) {
    cmd({"SET", "s", "not-a-number"});
    EXPECT_TRUE(hasErr(cmd({"INCR", "s"}), "not an integer"));
    EXPECT_TRUE(hasErr(cmd({"INCRBY", "s", "1"}), "not an integer"));
    EXPECT_TRUE(hasErr(cmd({"INCRBYFLOAT", "s", "x"}), "not a valid float"));
    EXPECT_TRUE(hasErr(cmd({"INCRBY", "s", "bad"}), "not an integer"));
}

TEST_F(StorageBoundaryFaultTest, ListIndexBoundary) {
    cmd({"RPUSH", "l", "only"});
    EXPECT_TRUE(isNull(cmd({"LINDEX", "l", "5"})));
    EXPECT_TRUE(isNull(cmd({"LINDEX", "l", "-5"})));
    EXPECT_TRUE(hasErr(cmd({"LSET", "l", "9", "x"}), "index out of range"));
}

TEST_F(StorageBoundaryFaultTest, WrongTypePropagation) {
    cmd({"SET", "str", "v"});
    EXPECT_TRUE(hasErr(cmd({"LPUSH", "str", "x"}), "WRONGTYPE"));
    EXPECT_TRUE(hasErr(cmd({"HGET", "str", "f"}), "WRONGTYPE"));
    cmd({"LPUSH", "lst", "x"});
    EXPECT_TRUE(isNull(cmd({"GET", "lst"})));
    EXPECT_TRUE(hasErr(cmd({"HGET", "lst", "f"}), "WRONGTYPE"));
}

TEST_F(StorageBoundaryFaultTest, HashFloatAndMissingField) {
    cmd({"HSET", "h", "n", "1"});
    EXPECT_TRUE(hasErr(cmd({"HINCRBYFLOAT", "h", "n", "bad"}), "not a valid float"));
    EXPECT_EQ(extractBulk(cmd({"HGET", "h", "missing"})), "");
}

TEST_F(StorageBoundaryFaultTest, ZSetScoreBoundary) {
    EXPECT_TRUE(hasErr(cmd({"ZADD", "z", "1.2.3", "m"}), "not a valid float"));
    cmd({"ZADD", "z", "1", "a"});
    EXPECT_TRUE(hasErr(cmd({"ZRANGEBYSCORE", "z", "x", "y"}), "not an integer"));
}

TEST_F(StorageBoundaryFaultTest, ExpireAndTtlFaults) {
    EXPECT_EQ(extractInteger(cmd({"EXPIRE", "ghost", "10"})), 0);
    EXPECT_TRUE(hasErr(cmd({"EXPIRE", "k", "nottl"}), "not an integer"));
    cmd({"SET", "k", "v"});
    EXPECT_TRUE(hasErr(cmd({"EXPIREAT", "k", "bad"}), "not an integer"));
}

TEST_F(StorageBoundaryFaultTest, BitmapBoundary) {
    cmd({"SET", "b", "x"});
    EXPECT_TRUE(hasErr(cmd({"SETBIT", "b", "-1", "1"}), "not an integer"));
    EXPECT_TRUE(hasErr(cmd({"SETBIT", "b", "bad", "1"}), "not an integer"));
    EXPECT_EQ(extractInteger(cmd({"SETBIT", "b", "0", "2"})), 0);
}

TEST_F(StorageBoundaryFaultTest, RestoreFaults) {
    EXPECT_TRUE(hasErr(cmd({"RESTORE", "rk", "bad", "S:data"}), "invalid ttl"));
}

TEST_F(StorageBoundaryFaultTest, StreamFaultInjection) {
    auto id = extractBulk(cmd({"XADD", "xs", "*", "f", "v"}));
    ASSERT_FALSE(id.empty());
    EXPECT_TRUE(isOK(cmd({"XGROUP", "CREATE", "xs", "g", "0"})));
    EXPECT_TRUE(hasErr(cmd({"XGROUP", "CREATE", "xs", "g", "0"}), "BUSYGROUP"));
    EXPECT_TRUE(hasErr(cmd({"XREADGROUP", "GROUP", "nog", "c", "STREAMS", "xs", "0"}),
                        "NOGROUP"));
    EXPECT_TRUE(hasErr(cmd({"XADD", "xs", id.c_str(), "f", "v2"}), "equal or smaller"));
}

TEST_F(StorageBoundaryFaultTest, GeoInvalidCoordinates) {
    EXPECT_TRUE(hasErr(cmd({"GEOADD", "g", "x", "0", "bad"}), "invalid longitude"));
    cmd({"GEOADD", "g2", "0", "0", "p"});
    EXPECT_TRUE(hasErr(cmd({"GEORADIUS", "g2", "x", "0", "1", "m"}), "invalid lon"));
}

TEST_F(StorageBoundaryFaultTest, SortWrongType) {
    cmd({"SET", "ss", "x"});
    EXPECT_TRUE(hasErr(cmd({"SORT", "ss"}), "WRONGTYPE"));
}

TEST_F(StorageBoundaryFaultTest, BitopUnknownOpStillWrites) {
    cmd({"SET", "b1", "a"});
    EXPECT_EQ(extractInteger(cmd({"BITOP", "UNKNOWN", "bout", "b1"})), 1);
    EXPECT_FALSE(extractBulk(cmd({"GET", "bout"})).empty());
}

// ---- 淘汰策略边界 ----

TEST(EvictionBoundaryFaultTest, EmptyDictNoCrash) {
    Dict d;
    EvictionManager ev;
    ev.setMaxmemory(1);
    ev.setPolicy(EVICT_ALLKEYS_LRU);
    EXPECT_EQ(ev.evict(d, 10), 0);
    ev.setPolicy(EVICT_VOLATILE_TTL);
    EXPECT_EQ(ev.evict(d, 10), 0);
}

TEST(EvictionBoundaryFaultTest, VolatileTtlEvictsUnderPressure) {
    Dict d;
    uint64_t now = CmdContext::nowMs();
    auto soon = Value::createString("soon");
    soon.expire_at_ms = now + 100;
    auto later = Value::createString("later");
    later.expire_at_ms = now + 100000;
    d.insert("soon", std::move(soon));
    d.insert("later", std::move(later));

    EvictionManager ev;
    ev.setMaxmemory(1);
    ev.setPolicy(EVICT_VOLATILE_TTL);
    int n = ev.evict(d, 1);
    EXPECT_GT(n, 0);
    EXPECT_LT(d.size(), 2u);
}

TEST(EvictionBoundaryFaultTest, NoEvictionPolicy) {
    Dict d;
    d.insert("k", Value::createString("v"));
    EvictionManager ev;
    ev.setMaxmemory(1);
    ev.setPolicy(EVICT_NOEVICTION);
    EXPECT_EQ(ev.evict(d, 10), 0);
}

// ---- 集群拓扑 / 命令故障注入 ----

TEST(ClusterBoundaryFaultTest, DeserializeMalformedNode) {
    ClusterTopology topo;
    NodeInfo node;
    EXPECT_FALSE(topo.deserializeNode("", node));
    EXPECT_FALSE(topo.deserializeNode("only-id", node));
    EXPECT_FALSE(topo.deserializeNode("id,ip,notaport,16379,1,0,M,-", node));
}

TEST(ClusterBoundaryFaultTest, SlotBitmapBoundary) {
    NodeInfo node;
    node.setSlot(0, true);
    node.setSlot(16383, true);
    EXPECT_TRUE(node.ownsSlot(0));
    EXPECT_TRUE(node.ownsSlot(16383));
    EXPECT_FALSE(node.ownsSlot(16384));
}

class ClusterManagerFaultTest : public ::testing::Test {
protected:
    void SetUp() override { ensureCommandTable(); }

    std::string clusterCmd(ClusterManager& mgr,
                           std::initializer_list<const char*> args) {
        std::string resp;
        CmdContext ctx;
        ctx.engine = &engine;
        ctx.response = &resp;
        ctx.args = makeArgs(args);
        mgr.routeCommand(ctx);
        return resp;
    }

    StorageEngine engine;
};

TEST_F(ClusterManagerFaultTest, SlotCommandFaults) {
    ClusterConfig cfg;
    cfg.enabled = true;
    cfg.cluster_port = 26401;
    cfg.bind_addr = "127.0.0.1";
    ClusterManager mgr(cfg, &engine);
    if (!mgr.start()) return;

    EXPECT_TRUE(hasErr(clusterCmd(mgr, {"CLUSTER", "ADDSLOTS"}), "No slots"));
    EXPECT_TRUE(hasErr(clusterCmd(mgr, {"CLUSTER", "ADDSLOTS", "99999"}), "out of range"));
    EXPECT_TRUE(hasErr(clusterCmd(mgr, {"CLUSTER", "ADDSLOTS", "-1"}), "out of range"));

    clusterCmd(mgr, {"CLUSTER", "FLUSHSLOTS"});
    EXPECT_TRUE(isOK(clusterCmd(mgr, {"CLUSTER", "ADDSLOTS", "10"})));
    EXPECT_TRUE(hasErr(clusterCmd(mgr, {"CLUSTER", "ADDSLOTS", "10"}), "already assigned"));
    EXPECT_TRUE(hasErr(clusterCmd(mgr, {"CLUSTER", "DELSLOTS", "99"}), "not owned"));

    EXPECT_TRUE(hasErr(clusterCmd(mgr, {"CLUSTER", "SETSLOT", "1"}), "Wrong number"));
    EXPECT_TRUE(hasErr(clusterCmd(mgr, {"CLUSTER", "REPLICATE"}), "Wrong number"));
    EXPECT_TRUE(hasErr(clusterCmd(mgr, {"CLUSTER", "REPLICATE", "ghost"}), "Unknown node"));
    EXPECT_TRUE(hasErr(clusterCmd(mgr, {"CLUSTER", "BOGUS"}), "Unknown CLUSTER"));

    mgr.stop();
}

// ---- AOF 故障注入 ----

TEST(AofFaultTest, StartOnDirectoryFails) {
    kvtest::TempFile dir_marker("/tmp/kvstore_aofdir_XXXXXX");
    std::string dir = dir_marker.path() + ".d";
    ASSERT_EQ(::mkdir(dir.c_str(), 0755), 0);
    AofWriter writer(dir, AofWriter::NO);
    EXPECT_FALSE(writer.start());
    ::rmdir(dir.c_str());
}

TEST(AofFaultTest, AppendWithoutStartIsNoOp) {
    AofWriter writer("/tmp/kvstore_aof_should_not_exist", AofWriter::NO);
    lstl::vector<std::string_view> args{"PING"};
    writer.appendArgs(args);
    EXPECT_LT(writer.fd(), 0);
}

TEST(AofFaultTest, CorruptAofPayloadUnreadableAsCommand) {
    kvtest::TempFile tmp("/tmp/kvstore_aof_bad_XXXXXX");
    writeTextFile(tmp.path(), "not-resp-data\r\n");
    std::ifstream ifs(tmp.path());
    std::string line;
    std::getline(ifs, line);
    RespParser p;
    size_t consumed = 0;
    auto r = p.feed(line.data(), line.size(), consumed);
    EXPECT_EQ(r, RespParser::Result::ERROR);
}
