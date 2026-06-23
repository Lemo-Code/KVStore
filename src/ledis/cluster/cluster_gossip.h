#pragma once
// ============================================================
// cluster_gossip.h — Gossip 协议 (成员发现 + 故障检测)
// ============================================================

#include <cstdint>
#include <string>
#include <lstl/container/vector.h>
#include <lstl/container/unordered_map.h>

#include "zero/log/log.h"
#include "ledis/cluster/cluster_types.h"
#include "ledis/cluster/cluster_topology.h"

namespace lrpc { class RpcNode; }

namespace ledis {
namespace cluster {

class ClusterManager;

class ClusterGossip {
public:
    ClusterGossip(ClusterTopology* topo, lrpc::RpcNode* rpc,
                  ClusterManager* mgr, int interval_ms, int timeout_ms,
                  zero::Logger::ptr logger = nullptr);

    // 定期维护: 发送 PING, 检查超时
    void tick();

    // 请求加入集群
    void meet(const std::string& ip, int cluster_port);

    // 标记节点故障并广播
    void markFailed(const std::string& node_id);

    // ---- 入站消息处理 (由 ClusterManager 调用) ----
    void onPing(const std::string& sender_id);
    void onPong(const std::string& sender_id, const std::string& body);
    void onMeet(const std::string& sender_id, const std::string& body);
    void onFail(const std::string& sender_id, const std::string& body);

private:
    void sendPing(const std::string& target_id);
    void sendPong(const std::string& target_id);
    void checkTimeouts();
    void selectTargets(lstl::vector<std::string>& targets);

    // 序列化/反序列化 gossip 数据
    std::string serializeGossipData();
    void processGossipData(const std::string& data);

    ClusterTopology* topo_;
    lrpc::RpcNode*   rpc_;
    ClusterManager*  mgr_;
    int interval_ms_;
    int timeout_ms_;
    uint64_t last_tick_ms_ = 0;

    // FAIL 投票: failed_node_id → voting node_ids
    lstl::unordered_map<std::string, lstl::vector<std::string>> fail_votes_;
    zero::Logger::ptr log_;
};

} // namespace cluster
} // namespace ledis
