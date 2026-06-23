#include <gtest/gtest.h>
#include "ledis/cluster/cluster_types.h"
#include "ledis/cluster/cluster_topology.h"
#include "ledis/cluster/cluster_config.h"
#include "ledis/core/storage_engine.h"

using namespace ledis::cluster;

TEST(ClusterTypesTest, NodeSlotBitmap) {
    NodeInfo node;
    node.id = "node1";
    node.setSlot(0, true);
    node.setSlot(100, true);
    node.setSlot(16383, true);
    EXPECT_TRUE(node.ownsSlot(0));
    EXPECT_TRUE(node.ownsSlot(100));
    EXPECT_FALSE(node.ownsSlot(50));
    EXPECT_EQ(node.slotCount(), 3);

    lstl::vector<uint16_t> slots;
    node.getSlotList(slots);
    EXPECT_EQ(slots.size(), 3u);

    node.clearSlots();
    EXPECT_EQ(node.slotCount(), 0);
    EXPECT_STREQ(nodeStateName(NodeState::ACTIVE), "active");
}

TEST(ClusterTopologyTest, SlotAssignment) {
    ClusterTopology topo;
    topo.setSelfId("self");
    NodeInfo self;
    self.id = "self";
    self.is_master = true;
    topo.addNode(self);

    topo.assignSlots("self", {0, 1, 2});
    EXPECT_TRUE(topo.isLocalSlot(0));
    EXPECT_TRUE(topo.isLocalSlot(1));

    topo.assignAllSlotsToSelf();
    EXPECT_TRUE(topo.isFullCoverage());
    EXPECT_TRUE(topo.isLocalKey("anykey"));

    uint16_t slot = ClusterTopology::keySlot("testkey");
    EXPECT_LT(slot, HASH_SLOTS);
    EXPECT_EQ(slot, ledis::StorageEngine::getSlot("testkey"));
}

TEST(ClusterTopologyTest, SerializeRoundTrip) {
    ClusterTopology topo;
    NodeInfo node;
    node.id = "abc123";
    node.ip = "127.0.0.1";
    node.client_port = 6379;
    node.cluster_port = 16379;
    node.state = NodeState::ACTIVE;
    node.epoch = 5;
    node.is_master = true;
    node.setSlot(10, true);
    node.setSlot(11, true);
    node.setSlot(20, true);

    std::string serialized = topo.serializeNode(node);
    NodeInfo parsed;
    EXPECT_TRUE(topo.deserializeNode(serialized, parsed));
    EXPECT_EQ(parsed.id, node.id);
    EXPECT_EQ(parsed.client_port, 6379);
    EXPECT_TRUE(parsed.ownsSlot(10));
    EXPECT_TRUE(parsed.ownsSlot(20));
    EXPECT_FALSE(parsed.ownsSlot(15));
}

TEST(ClusterTopologyTest, NodeManagement) {
    ClusterTopology topo;
    topo.setSelfId("n1");
    NodeInfo n1, n2;
    n1.id = "n1"; n1.is_master = true; n1.epoch = 1;
    n2.id = "n2"; n2.is_master = true; n2.epoch = 2;
    topo.addNode(n1);
    topo.addNode(n2);
    EXPECT_NE(topo.getNode("n1"), nullptr);
    EXPECT_NE(topo.getNode("n2"), nullptr);

    topo.updateState("n2", NodeState::PFAIL);
    EXPECT_EQ(topo.getNode("n2")->state, NodeState::PFAIL);

    topo.setSlot(5, "n1", true);
    topo.removeNode("n2");
    EXPECT_EQ(topo.getNode("n2"), nullptr);
    topo.bumpEpoch();
    EXPECT_GE(topo.epoch(), 1u);
}

TEST(ClusterConfigTest, Defaults) {
    ClusterConfig cfg;
    EXPECT_FALSE(cfg.enabled);
    EXPECT_EQ(cfg.port, 6379);
    EXPECT_EQ(cfg.gossip_interval_ms, 1000);
    EXPECT_TRUE(cfg.require_full_coverage);
}
