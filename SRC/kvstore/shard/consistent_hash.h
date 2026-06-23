#pragma once
#include "kvstore/common/kv_types.h"
#include "kvstore/shard/shard_types.h"
#include <vector>
#include <map>
namespace zero { namespace kvstore {
class ConsistentHash {
public:
    void AddNode(const NodeId& id, int vnodes = 16);
    void RemoveNode(const NodeId& id);
    NodeId GetNode(const Key& key) const;
    ShardId GetShard(const Key& key) const;
    std::vector<ShardDescriptor> BuildShards(int shard_count, int replica_count, const std::vector<NodeId>& nodes);
private:
    std::map<SlotId, NodeId> ring_;
};
}} // namespace
