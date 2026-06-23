#include "kvstore/raft/raft_log.h"
namespace zero { namespace kvstore {
RaftTerm RaftLog::GetTerm(RaftIndex idx) const {
    for (auto& e : entries_) if (e.index == idx) return e.term;
    return 0;
}
void RaftLog::Append(RaftIndex idx, RaftTerm term, const std::string& cmd) {
    LogEntry e; e.index = idx; e.term = term; e.type = 1; e.data = cmd;
    if (!entries_.empty() && idx <= entries_.back().index) {
        // Find insertion point
        while (!entries_.empty() && entries_.back().index >= idx) entries_.pop_back();
    }
    entries_.push_back(std::move(e));
}
void RaftLog::TruncateAfter(RaftIndex idx) {
    while (!entries_.empty() && entries_.back().index > idx) entries_.pop_back();
}
void RaftLog::TruncateBefore(RaftIndex idx) {
    while (!entries_.empty() && entries_.front().index < idx) entries_.pop_front();
}
void RaftLog::GetEntries(RaftIndex start, size_t max, std::vector<LogEntry>& out) const {
    for (auto& e : entries_) { if (e.index >= start && out.size() < max) out.push_back(e); }
}
bool RaftLog::GetEntry(RaftIndex idx, LogEntry& out) const {
    for (auto& e : entries_) if (e.index == idx) { out = e; return true; }
    return false;
}
}} // namespace
