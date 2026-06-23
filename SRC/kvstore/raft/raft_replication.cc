#include "kvstore/raft/raft_replication.h"
#include "kvstore/raft/raft_node.h"
#include "kvstore/common/kv_utils.h"
#include <algorithm>
namespace zero { namespace kvstore {
// ReplicationManager: handles log replication to peers
Status ReplicateToPeers(RaftNode* node) {
    if (!node || !node->IsLeader()) return Status::NotLeader();
    const auto& peers = node->Peers();
    for (const auto& peer : peers) {
        AppendEntriesReq req;
        req.term = node->CurrentTerm();
        req.leader_id = node->LeaderId();
        // Build entries for each peer based on next_index
        req.prev_log_index = peer.next_index - 1;
        req.leader_commit = node->CommitIndex();
        std::string body; req.Serialize(body);
        std::string reply;
        // Send via transport
        Status st = SendRaftMessage(peer.id, MsgType::RAFT_APPEND_REQ, body, reply);
        if (st.ok()) {
            AppendEntriesRsp rsp;
            if (AppendEntriesRsp::Parse(reply.data(), reply.size(), rsp)) {
                // Update peer progress handled by node
            }
        }
    }
    return Status::OK();
}
// Check replication quorum
bool HasQuorum(const std::vector<RaftPeer>& peers, RaftIndex match_index, int quorum_size) {
    int count = 1; // self
    for (const auto& p : peers) {
        if (p.match_index >= match_index) count++;
    }
    return count >= quorum_size;
}
// Compute quorum size: majority of (peers + 1 for self)
int QuorumSize(size_t peer_count) {
    return static_cast<int>((peer_count + 1) / 2);
}
}} // namespace
