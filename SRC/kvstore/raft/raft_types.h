// raft_types.h — Raft protocol types
#pragma once
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <utility>
#include "kvstore/common/kv_types.h"

namespace zero { namespace kvstore {

using RaftTerm  = Term;
using RaftIndex = Index;

enum class RaftRole : uint8_t { Follower = 0, Candidate = 1, Leader = 2 };

inline const char* RoleName(RaftRole r) {
    switch (r) { case RaftRole::Follower: return "Follower"; case RaftRole::Candidate: return "Candidate"; case RaftRole::Leader: return "Leader"; }
    return "Unknown";
}

struct RaftState {
    RaftTerm   current_term = 0;
    NodeId     voted_for;
    RaftIndex  commit_index = 0;
    RaftIndex  last_applied = 0;
    NodeId     leader_id;
    RaftRole   role = RaftRole::Follower;
};

struct LogEntry {
    RaftTerm   term  = 0;
    RaftIndex  index = 0;
    uint8_t    type  = 0;   // 1=command, 2=config
    std::string data;

    void Serialize(std::string& out) const;
    static bool Parse(const char*& p, const char* end, LogEntry& e);
};

// ---- RPC Messages ----
struct RequestVoteReq {
    RaftTerm  term = 0;
    NodeId    candidate_id;
    RaftIndex last_log_index = 0;
    RaftTerm  last_log_term  = 0;
    bool      pre_vote       = false;
    void Serialize(std::string& out) const;
    static bool Parse(const char* d, size_t n, RequestVoteReq& m);
};

struct RequestVoteRsp {
    RaftTerm term = 0;
    bool     vote_granted = false;
    void Serialize(std::string& out) const;
    static bool Parse(const char* d, size_t n, RequestVoteRsp& m);
};

struct AppendEntriesReq {
    RaftTerm  term = 0;
    NodeId    leader_id;
    RaftIndex prev_log_index = 0;
    RaftTerm  prev_log_term  = 0;
    std::vector<std::pair<RaftIndex, std::string>> entries;
    RaftIndex leader_commit = 0;
    void Serialize(std::string& out) const;
    static bool Parse(const char* d, size_t n, AppendEntriesReq& m);
};

struct AppendEntriesRsp {
    RaftTerm  term = 0;
    bool      success = false;
    RaftIndex match_index = 0;
    RaftTerm  hint_term   = 0;
    RaftIndex hint_index  = 0;
    void Serialize(std::string& out) const;
    static bool Parse(const char* d, size_t n, AppendEntriesRsp& m);
};

struct InstallSnapshotReq {
    RaftTerm  term = 0;
    NodeId    leader_id;
    RaftIndex last_included_index = 0;
    RaftTerm  last_included_term  = 0;
    uint64_t  offset = 0;
    std::string data;
    bool      done = false;
    void Serialize(std::string& out) const;
    static bool Parse(const char* d, size_t n, InstallSnapshotReq& m);
};

struct InstallSnapshotRsp {
    RaftTerm  term = 0;
    uint64_t  bytes_received = 0;
    void Serialize(std::string& out) const;
    static bool Parse(const char* d, size_t n, InstallSnapshotRsp& m);
};

// ---- Serialization helpers ----
inline void WriteU64(uint64_t v, std::string& out) {
    out.append(reinterpret_cast<const char*>(&v), 8);
}
inline uint64_t ReadU64(const char*& p, const char* end) {
    if (p + 8 > end) return 0;
    uint64_t v; memcpy(&v, p, 8); p += 8; return v;
}
inline void WriteStr(const std::string& s, std::string& out) {
    WriteU64(s.size(), out); out.append(s);
}
inline bool ReadStr(const char*& p, const char* end, std::string& s) {
    uint64_t len = ReadU64(p, end);
    if (p + len > end) return false;
    s.assign(p, len); p += len; return true;
}
inline void WriteBool(bool b, std::string& out) { out.push_back(b ? 1 : 0); }
inline bool ReadBool(const char*& p, const char*) { bool b = (*p++ != 0); return b; }

}} // namespace
