#pragma once
// ============================================================
// cluster_manager.h — 集群总协调器
// ============================================================
//
// 架构:
//   Server 线程 (fiber)           RPC I/O 线程
//   ┌─────────────────┐          ┌──────────────────┐
//   │ ClusterManager  │──写──→   │ RpcNode          │
//   │  ├ Topology     │          │  ├ epoll loop     │
//   │  ├ Router       │   ←eventfd─ ├ accept/connect │
//   │  ├ Gossip       │          │  └ read/write     │
//   │  └ Replica      │          └──────────────────┘
//   └─────────────────┘
//
// 关键: Server 线程拥有所有集群状态，RPC I/O 线程只做 I/O。
// 消息从 RPC 线程投递到 server 线程通过队列 + eventfd。
//

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <chrono>

#include <sys/eventfd.h>
#include <unistd.h>

#include <lstl/container/vector.h>

#include "zero/log/log.h"
#include "ledis/core/storage_engine.h"
#include "ledis/core/command.h"
#include "ledis/protocol/resp_parser.h"
#include "ledis/protocol/resp_writer.h"

#include "lrpc/lrpc.h"

#include "ledis/cluster/cluster_types.h"
#include "ledis/cluster/cluster_config.h"
#include "ledis/cluster/cluster_topology.h"
#include "ledis/cluster/cluster_router.h"
#include "ledis/cluster/cluster_gossip.h"

namespace ledis {
namespace cluster {

// ============================================================
// ClusterManager
// ============================================================
class ClusterManager {
public:
    using ptr = std::shared_ptr<ClusterManager>;

    ClusterManager(const ClusterConfig& cfg, StorageEngine* engine,
                   zero::Logger::ptr logger = nullptr);
    ~ClusterManager();

    // ---- 生命周期 ----
    bool start();
    void stop();

    // ---- 命令路由 (server 线程中调用) ----
    // 返回 true 表示命令已被处理 (本地 cluster 命令或已转发)
    bool routeCommand(CmdContext& ctx);

    // ---- 定期维护 (server 线程每次循环调用) ----
    void tick();

    // ---- 处理入站 RPC 消息 (由 RPC I/O 线程投递到队列，server 线程消费) ----
    void processPendingMessages();

    // ---- 写命令后复制 ----
    void onWriteCommand(const lstl::vector<std::string_view>& args);

    // ---- 访问器 ----
    ClusterConfig&       config()    { return config_; }
    ClusterTopology&     topology()  { return topo_; }
    lrpc::RpcNode*       rpc()       { return rpc_.get(); }
    StorageEngine*       engine()    { return engine_; }
    bool                 enabled() const { return config_.enabled; }
    int                  getEventFd() const { return event_fd_; }

    // CLUSTER 命令处理 (由 ClusterRouter::route 调用)
    void handleClusterCommand(CmdContext& ctx);

    // 设置客户端端口 (用于构建节点地址)
    void setClientPort(int p) {
        client_port_ = p;
        if (NodeInfo* self = topo_.selfNodeMut())
            self->client_port = p;
    }

private:
    // ---- 初始化 ----
    void initNodeId();
    void bootstrapAsFirstNode();
    void joinCluster();

    // ---- CLUSTER 命令子实现 ----
    void cmdClusterInfo(CmdContext& ctx);
    void cmdClusterNodes(CmdContext& ctx);
    void cmdClusterSlots(CmdContext& ctx);
    void cmdClusterMeet(CmdContext& ctx);
    void cmdClusterKeyslot(CmdContext& ctx);
    void cmdClusterMyid(CmdContext& ctx);
    void cmdClusterAddslots(CmdContext& ctx);
    void cmdClusterDelslots(CmdContext& ctx);
    void cmdClusterForget(CmdContext& ctx);
    void cmdClusterSetslot(CmdContext& ctx);
    void cmdClusterReplicate(CmdContext& ctx);
    void cmdClusterFailover(CmdContext& ctx);
    void cmdClusterFlushslots(CmdContext& ctx);
    void cmdClusterSaveconfig(CmdContext& ctx);

    // ---- Forward 处理 ----
    std::string executeForwarded(const std::string& serialized_cmd);

    // ---- 持久化 ----
    void saveNodesConf();
    bool loadNodesConf();

    // ---- RPC 消息处理 ----
    void setupRpcHandler();

    // 待处理消息结构
    struct PendingMsg {
        uint16_t    msg_type;
        uint32_t    call_id = 0;    // 非零表示需要 sendResponse 回复
        std::string sender_id;
        std::string body;
    };

    // ---- 成员 ----
    ClusterConfig  config_;
    zero::Logger::ptr log_;
    ClusterTopology topo_;
    std::unique_ptr<lrpc::RpcNode> rpc_;
    ClusterRouter* router_ = nullptr;
    ClusterGossip* gossip_ = nullptr;
    StorageEngine* engine_ = nullptr;

    std::atomic<bool> running_{false};
    int client_port_ = 0;

    // 消息队列 (RPC I/O 线程写入, server 线程读取)
    std::mutex msg_mutex_;
    lstl::vector<PendingMsg> pending_msgs_;

    // eventfd: RPC I/O 线程用此唤醒 server 线程
    int event_fd_ = -1;

    // Gossip 定时
    uint64_t last_gossip_ms_ = 0;

    // 复制偏移
    uint64_t repl_offset_ = 0;
    bool is_replicating_  = false;
    std::string master_id_;
};

} // namespace cluster
} // namespace ledis
