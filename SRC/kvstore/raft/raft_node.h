#pragma once
#include "kvstore/raft/raft_types.h"
#include "kvstore/protocol/kv_protocol.h"
#include "kvstore/raft/raft_config.h"
#include "kvstore/raft/raft_log.h"
#include "kvstore/common/kv_error.h"
#include "kvstore/storage/storage_engine.h"
#include <vector>
#include <functional>
#include <random>

namespace zero { namespace kvstore {

using TransportFn = std::function<Status(const NodeId& to, MsgType type, const std::string& body, std::string& reply)>;

class RaftNode {
public:
    RaftNode(const RaftConfig& cfg, const NodeId& self_id, const std::vector<RaftPeer>& peers);
    ~RaftNode() = default;

    // ---- lifecycle ----
    void Tick(int64_t elapsed_ms);
    Status Step(MsgType type, const char* data, size_t len);
    Status Propose(const std::string& cmd, RaftIndex& out_index);

    // ---- observers ----
    RaftRole Role() const { return state_.role; }
    RaftTerm CurrentTerm() const { return state_.current_term; }
    NodeId   LeaderId() const { return state_.leader_id; }
    bool     IsLeader() const { return state_.role == RaftRole::Leader; }
    RaftIndex CommitIndex() const { return state_.commit_index; }

    // ---- membership ----
    void AddPeer(const RaftPeer& p);
    void RemovePeer(const NodeId& id);
    const std::vector<RaftPeer>& Peers() const { return peers_; }

    // ---- storage ----
    void SetEngine(IKvEngine* e) { engine_ = e; }

    // ---- transport ----
    void SetTransport(TransportFn fn) { transport_ = std::move(fn); }

    // ---- snapshot ----
    void TriggerSnapshot(RaftIndex applied_index);

    // ---- stats ----
    struct Stats { RaftRole role; RaftTerm term; NodeId leader; RaftIndex commit; RaftIndex applied; size_t log_size; };
    Stats GetStats() const;

private:
    void becomeFollower(RaftTerm term);
    void becomeCandidate();
    void becomeLeader();
    void startElection(bool pre_vote);
    int  quorumSize() const { return static_cast<int>((peers_.size() + 1) / 2); }

    Status handleVoteRequest(const char* d, size_t n);
    Status handleVoteResponse(const char* d, size_t n);
    Status handleAppendEntries(const char* d, size_t n);
    Status handleAppendEntriesResponse(const char* d, size_t n);
    Status handleInstallSnapshot(const char* d, size_t n);
    Status handleInstallSnapshotResponse(const char* d, size_t n);

    void sendHeartbeats();
    void sendAppendEntries(const RaftPeer& peer);
    void advanceCommitIndex();
    void applyCommittedEntries();

    RaftConfig           cfg_;
    NodeId               self_id_;
    RaftState            state_;
    RaftLog              log_;
    std::vector<RaftPeer> peers_;
    IKvEngine*           engine_ = nullptr;
    TransportFn          transport_;
    std::string           reply_buf_;

    int64_t election_timer_ms_ = 0;
    int64_t heartbeat_timer_ms_ = 0;
    std::mt19937 rng_;
    std::string snapshot_buf_;
};

}} // namespace
