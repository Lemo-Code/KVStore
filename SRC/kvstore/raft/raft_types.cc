// raft_types.cc — Serialization implementations
#include "kvstore/raft/raft_types.h"
#include <cstring>

namespace zero { namespace kvstore {

void LogEntry::Serialize(std::string& out) const {
    WriteU64(term, out); WriteU64(index, out);
    out.push_back(static_cast<char>(type)); WriteStr(data, out);
}
bool LogEntry::Parse(const char*& p, const char* end, LogEntry& e) {
    e.term = ReadU64(p, end); e.index = ReadU64(p, end);
    if (p >= end) return false; e.type = static_cast<uint8_t>(*p++);
    return ReadStr(p, end, e.data);
}

void RequestVoteReq::Serialize(std::string& out) const {
    WriteU64(term, out); WriteStr(candidate_id, out);
    WriteU64(last_log_index, out); WriteU64(last_log_term, out); WriteBool(pre_vote, out);
}
bool RequestVoteReq::Parse(const char* d, size_t n, RequestVoteReq& m) {
    const char* p = d; const char* end = d + n;
    m.term = ReadU64(p, end); if (!ReadStr(p, end, m.candidate_id)) return false;
    m.last_log_index = ReadU64(p, end); m.last_log_term = ReadU64(p, end); m.pre_vote = ReadBool(p, end);
    return true;
}

void RequestVoteRsp::Serialize(std::string& out) const {
    WriteU64(term, out); WriteBool(vote_granted, out);
}
bool RequestVoteRsp::Parse(const char* d, size_t n, RequestVoteRsp& m) {
    const char* p = d; const char* end = d + n;
    m.term = ReadU64(p, end); m.vote_granted = ReadBool(p, end); return true;
}

void AppendEntriesReq::Serialize(std::string& out) const {
    WriteU64(term, out); WriteStr(leader_id, out);
    WriteU64(prev_log_index, out); WriteU64(prev_log_term, out);
    WriteU64(entries.size(), out);
    for (auto& [idx, data] : entries) { WriteU64(idx, out); WriteStr(data, out); }
    WriteU64(leader_commit, out);
}
bool AppendEntriesReq::Parse(const char* d, size_t n, AppendEntriesReq& m) {
    const char* p = d; const char* end = d + n;
    m.term = ReadU64(p, end); if (!ReadStr(p, end, m.leader_id)) return false;
    m.prev_log_index = ReadU64(p, end); m.prev_log_term = ReadU64(p, end);
    uint64_t count = ReadU64(p, end); m.entries.resize(count);
    for (uint64_t i = 0; i < count; ++i) {
        RaftIndex idx = ReadU64(p, end); std::string data;
        if (!ReadStr(p, end, data)) return false;
        m.entries[i] = {idx, std::move(data)};
    }
    m.leader_commit = ReadU64(p, end); return true;
}

void AppendEntriesRsp::Serialize(std::string& out) const {
    WriteU64(term, out); WriteBool(success, out);
    WriteU64(match_index, out); WriteU64(hint_term, out); WriteU64(hint_index, out);
}
bool AppendEntriesRsp::Parse(const char* d, size_t n, AppendEntriesRsp& m) {
    const char* p = d; const char* end = d + n;
    m.term = ReadU64(p, end); m.success = ReadBool(p, end);
    m.match_index = ReadU64(p, end); m.hint_term = ReadU64(p, end); m.hint_index = ReadU64(p, end);
    return true;
}

void InstallSnapshotReq::Serialize(std::string& out) const {
    WriteU64(term, out); WriteStr(leader_id, out);
    WriteU64(last_included_index, out); WriteU64(last_included_term, out);
    WriteU64(offset, out); WriteStr(data, out); WriteBool(done, out);
}
bool InstallSnapshotReq::Parse(const char* d, size_t n, InstallSnapshotReq& m) {
    const char* p = d; const char* end = d + n;
    m.term = ReadU64(p, end); if (!ReadStr(p, end, m.leader_id)) return false;
    m.last_included_index = ReadU64(p, end); m.last_included_term = ReadU64(p, end);
    m.offset = ReadU64(p, end);
    if (!ReadStr(p, end, m.data)) return false; m.done = ReadBool(p, end);
    return true;
}

void InstallSnapshotRsp::Serialize(std::string& out) const {
    WriteU64(term, out); WriteU64(bytes_received, out);
}
bool InstallSnapshotRsp::Parse(const char* d, size_t n, InstallSnapshotRsp& m) {
    const char* p = d; const char* end = d + n;
    m.term = ReadU64(p, end); m.bytes_received = ReadU64(p, end); return true;
}

}} // namespace
