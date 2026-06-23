#pragma once
#include "kvstore/common/kv_types.h"
#include "kvstore/common/kv_error.h"
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
namespace zero { namespace kvstore {

struct ClusterInfo {
    std::string cluster_id;
    std::vector<NodeId> nodes;
    std::string status; // "healthy", "degraded", "unavailable"
    int shard_count;
    int replica_count;
    uint64_t total_keys;
    std::string ToJson() const;
};

struct NodeInfo {
    NodeId node_id;
    std::string role;
    Term term;
    NodeId leader_id;
    uint64_t key_count;
    int64_t uptime_ms;
    int64_t latency_p50_us;
    int64_t latency_p99_us;
    uint64_t ops_per_sec;
    std::string ToJson() const;
};

struct ShardInfo {
    ShardId id;
    SlotRange slots;
    std::vector<NodeId> members;
    NodeId leader_id;
    uint64_t key_count;
    std::string state; // "active", "migrating", "creating"
    std::string ToJson() const;
};

// Admin command parsing
enum class AdminCmd { STATUS, NODES, SHARDS, JOIN, LEAVE, COMPACT, SNAPSHOT, RECONFIGURE };
AdminCmd ParseAdminCmd(const std::string& cmd);
std::string ExecuteAdminCmd(AdminCmd cmd, const std::string& args);

}} // namespace
