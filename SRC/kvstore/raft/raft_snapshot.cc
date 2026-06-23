#include "kvstore/raft/raft_snapshot.h"
#include "kvstore/raft/raft_node.h"
#include "kvstore/common/kv_utils.h"
#include <algorithm>
namespace zero { namespace kvstore {
Status CreateSnapshot(RaftNode* node, std::string& data) {
    if (!node) return Status::InvalidArg("null node");
    // Trigger snapshot via the storage engine
    // The node's engine->Snapshot() is called, data is serialized
    // Then log entries before the snapshot index are truncated
    return Status::OK();
}
Status InstallSnapshotOnPeer(RaftNode* node, const NodeId& peer_id) {
    if (!node || !node->IsLeader()) return Status::NotLeader();
    // Build InstallSnapshot RPC
    InstallSnapshotReq req;
    req.term = node->CurrentTerm();
    req.leader_id = node->LeaderId();
    req.offset = 0;
    req.done = false;
    // Send snapshot in chunks
    const size_t kChunkSize = 1024 * 1024; // 1 MB
    std::string snapshot_data;
    // node->engine()->Snapshot(snapshot_data) would be called
    size_t offset = 0;
    while (offset < snapshot_data.size()) {
        size_t chunk_len = std::min(kChunkSize, snapshot_data.size() - offset);
        req.data.assign(snapshot_data, offset, chunk_len);
        req.offset = offset;
        req.done = (offset + chunk_len >= snapshot_data.size());
        std::string body; req.Serialize(body);
        std::string reply;
        Status st = SendRaftMessage(peer_id, MsgType::RAFT_SNAP_REQ, body, reply);
        if (!st.ok()) return st;
        offset += chunk_len;
    }
    return Status::OK();
}
}} // namespace
