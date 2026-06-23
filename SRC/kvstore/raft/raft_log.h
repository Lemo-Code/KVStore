#pragma once
#include "kvstore/raft/raft_types.h"
#include <deque>
namespace zero { namespace kvstore {
class RaftLog {
public:
    RaftIndex LastIndex() const { return entries_.empty() ? 0 : entries_.back().index; }
    RaftTerm  LastTerm() const  { return entries_.empty() ? 0 : entries_.back().term; }
    RaftIndex FirstIndex() const { return entries_.empty() ? 0 : entries_.front().index; }
    RaftTerm  GetTerm(RaftIndex idx) const;
    void Append(RaftIndex idx, RaftTerm term, const std::string& cmd);
    void TruncateAfter(RaftIndex idx);
    void TruncateBefore(RaftIndex idx);
    void GetEntries(RaftIndex start, size_t max_count, std::vector<LogEntry>& out) const;
    bool GetEntry(RaftIndex idx, LogEntry& out) const;
    RaftIndex CommitIndex() const { return commit_index_; }
    RaftIndex LastApplied() const { return last_applied_; }
    void CommitTo(RaftIndex idx) { if (idx > commit_index_) commit_index_ = idx; }
    void AppliedTo(RaftIndex idx) { if (idx > last_applied_) last_applied_ = idx; }
    size_t Size() const { return entries_.size(); }
private:
    std::deque<LogEntry> entries_;
    RaftIndex commit_index_ = 0;
    RaftIndex last_applied_ = 0;
};
}} // namespace
