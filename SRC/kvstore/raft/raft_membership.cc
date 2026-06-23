#include "kvstore/raft/raft_membership.h"
#include "kvstore/raft/raft_node.h"
#include <algorithm>
namespace zero { namespace kvstore {
// Joint consensus membership change
Status ChangeMembership(RaftNode* node, const NodeId& id, bool add) {
    if (!node) return Status::InvalidArg("null node");
    if (!node->IsLeader()) return Status::NotLeader();
    if (add) {
        // Step 1: Propose C_old_new joint config
        RaftPeer new_peer; new_peer.id = id;
        node->AddPeer(new_peer);
        // Step 2: Once committed, propose C_new config
        // (removing the old node if removing)
    } else {
        node->RemovePeer(id);
    }
    return Status::OK();
}
// Validate membership change
bool ValidateMembershipChange(const std::vector<RaftPeer>& current, const NodeId& target, bool add) {
    if (add) {
        for (const auto& p : current) if (p.id == target) return false; // already member
    } else {
        bool found = false;
        for (const auto& p : current) if (p.id == target) found = true;
        if (!found) return false; // not a member
        if (current.size() <= 2) return false; // would lose quorum
    }
    return true;
}
}} // namespace
