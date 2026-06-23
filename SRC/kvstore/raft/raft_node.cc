// raft_node.cc — Raft consensus state machine implementation
#include "kvstore/raft/raft_node.h"
#include "kvstore/common/kv_utils.h"
#include <algorithm>
#include <cstring>
#include <sstream>

namespace zero { namespace kvstore {

RaftNode::RaftNode(const RaftConfig& cfg, const NodeId& self_id, const std::vector<RaftPeer>& peers)
    : cfg_(cfg), self_id_(self_id), peers_(peers), rng_(std::random_device{}())
{
    state_.role = RaftRole::Follower;
    election_timer_ms_ = cfg_.RandomizedElectionTimeout();
}

void RaftNode::becomeFollower(RaftTerm term) {
    state_.role = RaftRole::Follower;
    state_.current_term = term;
    state_.voted_for.clear();
    state_.leader_id.clear();
    election_timer_ms_ = cfg_.RandomizedElectionTimeout();
}

void RaftNode::becomeCandidate() {
    state_.role = RaftRole::Candidate;
    state_.current_term++;
    state_.voted_for = self_id_;
    state_.leader_id.clear();
    election_timer_ms_ = cfg_.RandomizedElectionTimeout();
    startElection(false);
}

void RaftNode::becomeLeader() {
    state_.role = RaftRole::Leader;
    state_.leader_id = self_id_;
    RaftIndex last = log_.LastIndex();
    for (auto& p : peers_) {
        p.next_index  = last + 1;
        p.match_index = 0;
    }
    // Append a no-op entry to commit previous entries
    std::string noop = "NOOP";
    log_.Append(last + 1, state_.current_term, noop);
    heartbeat_timer_ms_ = 0;
    sendHeartbeats();
}

void RaftNode::startElection(bool pre_vote) {
    RequestVoteReq req;
    req.term = pre_vote ? state_.current_term : state_.current_term;
    req.candidate_id = self_id_;
    req.last_log_index = log_.LastIndex();
    req.last_log_term  = log_.LastTerm();
    req.pre_vote = pre_vote;

    std::string body;
    req.Serialize(body);

    int votes = 1; // vote for self
    int total = static_cast<int>(peers_.size()) + 1;

    for (auto& peer : peers_) {
        std::string reply;
        Status st = transport_(peer.id, MsgType::RAFT_VOTE_REQ, body, reply);
        if (!st.ok()) continue;

        RequestVoteRsp rsp;
        if (RequestVoteRsp::Parse(reply.data(), reply.size(), rsp)) {
            if (rsp.term > state_.current_term) {
                becomeFollower(rsp.term);
                return;
            }
            if (rsp.vote_granted) votes++;
        }
    }

    if (votes > total / 2) {
        if (pre_vote) {
            // Pre-vote succeeded, now do real vote
            startElection(false);
        } else {
            becomeLeader();
        }
    }
}

void RaftNode::Tick(int64_t elapsed_ms) {
    election_timer_ms_ -= elapsed_ms;
    heartbeat_timer_ms_ -= elapsed_ms;

    if (state_.role == RaftRole::Leader) {
        if (heartbeat_timer_ms_ <= 0) {
            heartbeat_timer_ms_ = cfg_.heartbeat_ms;
            sendHeartbeats();
        }
    } else {
        if (election_timer_ms_ <= 0) {
            election_timer_ms_ = cfg_.RandomizedElectionTimeout();
            becomeCandidate();
        }
    }
}

Status RaftNode::Step(MsgType type, const char* data, size_t len) {
    switch (type) {
        case MsgType::RAFT_VOTE_REQ:   return handleVoteRequest(data, len);
        case MsgType::RAFT_VOTE_RSP:   return handleVoteResponse(data, len);
        case MsgType::RAFT_APPEND_REQ: return handleAppendEntries(data, len);
        case MsgType::RAFT_APPEND_RSP: return handleAppendEntriesResponse(data, len);
        case MsgType::RAFT_SNAP_REQ:   return handleInstallSnapshot(data, len);
        case MsgType::RAFT_SNAP_RSP:   return handleInstallSnapshotResponse(data, len);
        default: return Status::InvalidArg("unknown raft msg type");
    }
}

Status RaftNode::handleVoteRequest(const char* d, size_t n) {
    RequestVoteReq req;
    if (!RequestVoteReq::Parse(d, n, req)) return Status::InvalidArg("bad vote request");

    bool grant = false;
    if (req.term < state_.current_term) {
        grant = false;
    } else {
        if (req.term > state_.current_term) becomeFollower(req.term);

        bool can_vote = state_.voted_for.empty() || state_.voted_for == req.candidate_id;
        bool log_ok = (req.last_log_term > log_.LastTerm()) ||
                      (req.last_log_term == log_.LastTerm() && req.last_log_index >= log_.LastIndex());

        if (can_vote && log_ok && !req.pre_vote) {
            state_.voted_for = req.candidate_id;
            election_timer_ms_ = cfg_.RandomizedElectionTimeout();
            grant = true;
        } else if (can_vote && log_ok && req.pre_vote) {
            grant = true; // pre-vote: don't persist vote
        }
    }

    RequestVoteRsp rsp;
    rsp.term = state_.current_term;
    rsp.vote_granted = grant;
    rsp.Serialize(reply_buf_);
    // Response is sent by caller via transport callback
    return Status::OK();
}

Status RaftNode::handleVoteResponse(const char* d, size_t n) {
    if (state_.role != RaftRole::Candidate) return Status::OK();
    RequestVoteRsp rsp;
    if (!RequestVoteRsp::Parse(d, n, rsp)) return Status::InvalidArg("bad vote response");
    if (rsp.term > state_.current_term) { becomeFollower(rsp.term); return Status::OK(); }
    // Vote counting is done in startElection
    return Status::OK();
}

Status RaftNode::handleAppendEntries(const char* d, size_t n) {
    AppendEntriesReq req;
    if (!AppendEntriesReq::Parse(d, n, req)) return Status::InvalidArg("bad append entries");

    if (req.term < state_.current_term) {
        AppendEntriesRsp rsp; rsp.term = state_.current_term; rsp.success = false;
        return Status::OK();
    }

    // Valid leader
    state_.leader_id = req.leader_id;
    if (req.term > state_.current_term) becomeFollower(req.term);
    election_timer_ms_ = cfg_.RandomizedElectionTimeout(); // reset timer

    // Check prev log match
    if (req.prev_log_index > 0) {
        RaftTerm prev_term = log_.GetTerm(req.prev_log_index);
        if (prev_term != req.prev_log_term) {
            AppendEntriesRsp rsp;
            rsp.term = state_.current_term; rsp.success = false;
            rsp.match_index = 0;
            // Find conflicting term's first index for hint
            rsp.hint_term = prev_term;
            rsp.hint_index = log_.LastIndex();
            return Status::OK();
        }
    }

    // Append new entries
    for (auto& [idx, data] : req.entries) {
        RaftTerm existing_term = log_.GetTerm(idx);
        if (existing_term != 0 && existing_term != req.term) {
            log_.TruncateAfter(idx - 1);
        }
        if (idx > log_.LastIndex()) {
            log_.Append(idx, req.term, data);
        }
    }

    // Update commit index
    if (req.leader_commit > state_.commit_index) {
        state_.commit_index = std::min(req.leader_commit, log_.LastIndex());
        applyCommittedEntries();
    }

    return Status::OK();
}

Status RaftNode::handleAppendEntriesResponse(const char* d, size_t n) {
    AppendEntriesRsp rsp;
    if (!AppendEntriesRsp::Parse(d, n, rsp)) return Status::InvalidArg("bad append entries rsp");
    if (rsp.term > state_.current_term) { becomeFollower(rsp.term); return Status::OK(); }
    if (state_.role != RaftRole::Leader) return Status::OK();

    // Update peer progress
    for (auto& peer : peers_) {
        if (rsp.success) {
            peer.match_index = std::max(peer.match_index, rsp.match_index);
            peer.next_index  = peer.match_index + 1;
        } else {
            if (rsp.hint_index > 0) peer.next_index = rsp.hint_index;
            else if (peer.next_index > 1) peer.next_index--;
        }
    }

    advanceCommitIndex();
    applyCommittedEntries();
    return Status::OK();
}

Status RaftNode::handleInstallSnapshot(const char* d, size_t n) {
    InstallSnapshotReq req;
    if (!InstallSnapshotReq::Parse(d, n, req)) return Status::InvalidArg("bad snapshot");
    if (req.term < state_.current_term) return Status::OK();
    if (req.term > state_.current_term) becomeFollower(req.term);

    if (req.offset == 0) snapshot_buf_.clear();
    snapshot_buf_.append(req.data);

    InstallSnapshotRsp rsp;
    rsp.term = state_.current_term;
    rsp.bytes_received = snapshot_buf_.size();

    if (req.done && engine_) {
        engine_->RestoreSnapshot(snapshot_buf_);
        log_.TruncateBefore(req.last_included_index + 1);
        state_.last_applied = req.last_included_index;
        state_.commit_index = req.last_included_index;
        snapshot_buf_.clear();
    }
    return Status::OK();
}

Status RaftNode::handleInstallSnapshotResponse(const char* d, size_t n) {
    return Status::OK(); // simplified
}

Status RaftNode::Propose(const std::string& cmd, RaftIndex& out_index) {
    if (state_.role != RaftRole::Leader) {
        if (!state_.leader_id.empty())
            return Status::NotLeader(state_.leader_id);
        return Status::NotLeader("unknown");
    }
    RaftIndex idx = log_.LastIndex() + 1;
    log_.Append(idx, state_.current_term, cmd);
    out_index = idx;
    sendHeartbeats(); // immediately replicate
    return Status::OK();
}

void RaftNode::sendHeartbeats() {
    for (auto& peer : peers_) sendAppendEntries(peer);
}

void RaftNode::sendAppendEntries(const RaftPeer& peer) {
    AppendEntriesReq req;
    req.term = state_.current_term;
    req.leader_id = self_id_;

    if (peer.next_index <= log_.LastIndex()) {
        RaftIndex prev_idx = peer.next_index - 1;
        req.prev_log_index = prev_idx;
        req.prev_log_term  = prev_idx > 0 ? log_.GetTerm(prev_idx) : 0;
        {
            std::vector<LogEntry> entries; log_.GetEntries(peer.next_index, cfg_.max_entries_append, entries);
            for (auto& e : entries) req.entries.emplace_back(e.index, e.data);
        };
    } else {
        req.prev_log_index = log_.LastIndex();
        req.prev_log_term  = log_.LastTerm();
    }
    req.leader_commit = state_.commit_index;

    std::string body;
    req.Serialize(body);

    std::string reply;
    Status st = transport_(peer.id, MsgType::RAFT_APPEND_REQ, body, reply);
    if (st.ok()) {
        handleAppendEntriesResponse(reply.data(), reply.size());
    }
}

void RaftNode::advanceCommitIndex() {
    RaftIndex last = log_.LastIndex();
    for (RaftIndex n = state_.commit_index + 1; n <= last; ++n) {
        if (log_.GetTerm(n) != state_.current_term) continue;
        int count = 1; // self
        for (auto& p : peers_) if (p.match_index >= n) count++;
        if (count > static_cast<int>(peers_.size()) / 2) {
            state_.commit_index = n;
        }
    }
}

void RaftNode::applyCommittedEntries() {
    if (!engine_) return;
    while (state_.last_applied < state_.commit_index) {
        RaftIndex idx = state_.last_applied + 1;
        LogEntry entry;
        if (!log_.GetEntry(idx, entry)) break;

        if (entry.type == 1 && !entry.data.empty() && entry.data != "NOOP") {
            // Parse batch from entry data
            Batch batch;
            const char* p = entry.data.data();
            const char* end = p + entry.data.size();
            while (p < end) {
                BatchEntry be;
                be.op = static_cast<BatchOp>(*p++);
                uint64_t klen = ReadU64(p, end);
                be.key.assign(p, klen); p += klen;
                if (be.op == BatchOp::PUT) {
                    uint64_t vlen = ReadU64(p, end);
                    be.val.assign(p, vlen); p += vlen;
                }
                batch.push_back(std::move(be));
            }
            engine_->ApplyBatch(batch);
        }
        state_.last_applied = idx;
        log_.AppliedTo(idx);
    }
}

void RaftNode::AddPeer(const RaftPeer& p) { peers_.push_back(p); }
void RaftNode::RemovePeer(const NodeId& id) {
    peers_.erase(std::remove_if(peers_.begin(), peers_.end(),
        [&](auto& p) { return p.id == id; }), peers_.end());
}

void RaftNode::TriggerSnapshot(RaftIndex idx) {
    if (!engine_) return;
    engine_->Snapshot(snapshot_buf_);
    log_.TruncateBefore(idx);
}

RaftNode::Stats RaftNode::GetStats() const {
    return {state_.role, state_.current_term, state_.leader_id,
            state_.commit_index, state_.last_applied, log_.Size()};
}

}} // namespace
