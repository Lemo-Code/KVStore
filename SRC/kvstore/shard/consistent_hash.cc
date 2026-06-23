#include "kvstore/shard/consistent_hash.h"
#include "kvstore/common/kv_utils.h"
#include <algorithm>
namespace zero { namespace kvstore {
void ConsistentHash::AddNode(const NodeId& id, int vnodes) {
    for (int i = 0; i < vnodes; ++i) {
        std::string vkey = id + ":" + std::to_string(i);
        uint16_t slot = HashSlot(vkey);
        ring_[slot] = id;
    }
}
void ConsistentHash::RemoveNode(const NodeId& id) {
    for (auto it = ring_.begin(); it != ring_.end(); ) {
        if (it->second == id) it = ring_.erase(it); else ++it;
    }
}
NodeId ConsistentHash::GetNode(const Key& key) const {
    if (ring_.empty()) return "";
    uint16_t slot = HashSlot(key);
    auto it = ring_.lower_bound(slot);
    if (it == ring_.end()) it = ring_.begin();
    return it->second;
}
ShardId ConsistentHash::GetShard(const Key& key) const {
    return HashSlot(key) / (kNumSlots / std::max(size_t(1), ring_.size()));
}
std::vector<ShardDescriptor> ConsistentHash::BuildShards(int count, int replicas, const std::vector<NodeId>& nodes) {
    std::vector<ShardDescriptor> shards;
    uint32_t slots_per = kNumSlots / count;
    for (int i = 0; i < count; ++i) {
        ShardDescriptor sd;
        sd.id = i;
        sd.slots.first = i * slots_per;
        sd.slots.last = (i == count - 1) ? kNumSlots - 1 : (i + 1) * slots_per - 1;
        for (int r = 0; r < replicas && r < (int)nodes.size(); ++r)
            sd.members.push_back(nodes[(i + r) % nodes.size()]);
        shards.push_back(sd);
    }
    return shards;
}
}} // namespace
