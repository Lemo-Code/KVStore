#pragma once
#include "kvstore/raft/raft_types.h"
#include "kvstore/protocol/kv_protocol.h"
#include "kvstore/common/kv_error.h"
namespace zero { namespace kvstore {
class RaftNode;
Status ReplicateToPeers(RaftNode* node);
Status CreateSnapshot(RaftNode* node, std::string& data);
Status InstallSnapshotOnPeer(RaftNode* node, const NodeId& peer_id);
Status ChangeMembership(RaftNode* node, const NodeId& id, bool add);
Status SendRaftMessage(const NodeId& to, MsgType type, const std::string& body, std::string& reply);
}} // namespace
