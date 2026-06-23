#include <gtest/gtest.h>
#include "ledis_helpers.h"
#include "temp_file.h"
#include "ledis/cluster/cluster_manager.h"
#include "ledis/cluster/cluster_config.h"

using namespace ledis;
using namespace ledis::cluster;
using namespace kvtest;

class ClusterManagerFixture : public ::testing::Test {
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

TEST_F(ClusterManagerFixture, ConstructBuildsRouter) {
    ClusterConfig cfg;
    cfg.enabled = false;
    cfg.cluster_port = 26379;
    ClusterManager mgr(cfg, &engine);
    EXPECT_NE(mgr.rpc(), nullptr);
    EXPECT_EQ(mgr.topology().selfId(), "");
}

TEST_F(ClusterManagerFixture, StartStopAndClusterCommands) {
    ClusterConfig cfg;
    cfg.enabled = true;
    cfg.cluster_port = 26381;
    cfg.bind_addr = "127.0.0.1";
    kvtest::TempFile nodes_conf("/tmp/kvstore_nodes_XXXXXX");
    cfg.nodes_conf_path = nodes_conf.path();

    ClusterManager mgr(cfg, &engine);
    if (!mgr.start()) return;

    EXPECT_FALSE(clusterCmd(mgr, {"CLUSTER", "MYID"}).empty());
    EXPECT_FALSE(clusterCmd(mgr, {"CLUSTER", "INFO"}).empty());
    EXPECT_FALSE(clusterCmd(mgr, {"CLUSTER", "NODES"}).empty());
    EXPECT_FALSE(clusterCmd(mgr, {"CLUSTER", "SLOTS"}).empty());
    EXPECT_FALSE(clusterCmd(mgr, {"CLUSTER", "KEYSLOT", "foo"}).empty());
    (void)clusterCmd(mgr, {"CLUSTER", "FLUSHSLOTS"});
    (void)clusterCmd(mgr, {"CLUSTER", "ADDSLOTS", "0", "1", "2"});
    (void)clusterCmd(mgr, {"CLUSTER", "DELSLOTS", "2"});
    (void)clusterCmd(mgr, {"CLUSTER", "SETSLOT", "3", "NODE", mgr.topology().selfId().c_str()});
    (void)clusterCmd(mgr, {"CLUSTER", "SETSLOT", "3", "STABLE"});
    (void)clusterCmd(mgr, {"CLUSTER", "FLUSHSLOTS"});
    (void)clusterCmd(mgr, {"CLUSTER", "SAVECONFIG"});
    (void)clusterCmd(mgr, {"CLUSTER", "MEET", "127.0.0.1", "26382"});
    EXPECT_NE(clusterCmd(mgr, {"CLUSTER", "UNKNOWN"}).find("ERR"), std::string::npos);
    EXPECT_FALSE(clusterCmd(mgr, {"CLUSTER"}).empty());

    mgr.tick();
    mgr.processPendingMessages();

    lstl::vector<std::string_view> write_args;
    write_args.push_back("SET");
    write_args.push_back("ck");
    write_args.push_back("cv");
    mgr.onWriteCommand(write_args);

    mgr.stop();
}

TEST_F(ClusterManagerFixture, ReplicateAndFailover) {
    ClusterConfig cfg;
    cfg.enabled = true;
    cfg.cluster_port = 26383;
    cfg.bind_addr = "127.0.0.1";
    ClusterManager mgr(cfg, &engine);
    if (!mgr.start()) return;

    auto& topo = mgr.topology();
    NodeInfo master;
    master.id = "master-node";
    master.is_master = true;
    master.setSlot(10, true);
    topo.addNode(master);

    EXPECT_TRUE(isOK(clusterCmd(mgr, {"CLUSTER", "REPLICATE", "master-node"})));

    NodeInfo* self = topo.selfNodeMut();
    if (self) {
        self->is_master = false;
        self->master_id = "master-node";
    }
    EXPECT_TRUE(isOK(clusterCmd(mgr, {"CLUSTER", "FAILOVER"})));

    EXPECT_TRUE(isOK(clusterCmd(mgr, {"CLUSTER", "FORGET", "master-node"})));
    mgr.stop();
}

TEST_F(ClusterManagerFixture, SerializeDeserializeNodes) {
    ClusterTopology topo;
    NodeInfo node;
    node.id = "node-abc";
    node.ip = "10.0.0.1";
    node.client_port = 6379;
    node.cluster_port = 16379;
    node.setSlot(5, true);
    node.setSlot(6, true);
    std::string s = topo.serializeNode(node);
    NodeInfo out;
    EXPECT_TRUE(topo.deserializeNode(s, out));
    EXPECT_TRUE(out.ownsSlot(5));
}

TEST_F(ClusterManagerFixture, RouteKeyedCommand) {
    ClusterConfig cfg;
    cfg.enabled = true;
    cfg.cluster_port = 26384;
    cfg.bind_addr = "127.0.0.1";
    ClusterManager mgr(cfg, &engine);
    if (!mgr.start()) return;

    clusterCmd(mgr, {"CLUSTER", "ADDSLOTS", "0"});
    std::string resp;
    CmdContext ctx;
    ctx.engine = &engine;
    ctx.response = &resp;
    ctx.args = makeArgs({"SET", "rkey", "rval"});
    mgr.routeCommand(ctx);
    mgr.stop();
}
