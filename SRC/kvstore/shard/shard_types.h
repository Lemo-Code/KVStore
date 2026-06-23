#pragma once
#include "kvstore/common/kv_types.h"
#include "kvstore/raft/raft_types.h"
#include <vector>
namespace zero { namespace kvstore {
struct ShardDescriptor {
    ShardId id = 0;
    SlotRange slots{0, 0};
    std::vector<NodeId> members;
    NodeId leader_id;
    RaftIndex raft_group_id = 0;
};
}} // namespace
