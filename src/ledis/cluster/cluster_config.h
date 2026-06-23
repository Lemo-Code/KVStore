#pragma once
// ============================================================
// cluster_config.h — 集群配置
// ============================================================

#include <string>
#include <lstl/container/vector.h>

namespace ledis {
namespace cluster {

struct ClusterConfig {
    bool enabled = false;

    std::string bind_addr    = "0.0.0.0";
    int         port         = 6379;       // 客户端端口
    int         cluster_port = 0;          // 集群间端口 (默认 port + 10000)

    std::string node_id;                   // 节点 ID (空则自动生成)
    std::string nodes_conf_path;           // nodes.conf 路径 (空 = 不持久化)

    // 种子节点: "ip:port,ip:port,..."
    lstl::vector<std::string> seeds;

    int  replicas           = 0;           // 副本数 (0 = 无复制)
    int  gossip_interval_ms = 1000;        // Gossip 间隔
    int  node_timeout_ms    = 15000;       // 故障超时
    bool require_full_coverage = true;     // 全部槽位覆盖才接受写入
};

} // namespace cluster
} // namespace ledis
