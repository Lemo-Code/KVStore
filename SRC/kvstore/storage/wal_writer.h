#pragma once
#include "kvstore/common/kv_types.h"
#include "kvstore/common/kv_error.h"
#include "kvstore/raft/raft_types.h"
#include <string>
namespace zero { namespace kvstore {
class WalWriter {
public:
    WalWriter();
    ~WalWriter();
    Status Open(const std::string& dir);
    Status Append(RaftIndex index, RaftTerm term, const std::string& data);
    Status Fsync();
    Status Close();
    RaftIndex FirstIndex() const { return first_index_; }
    RaftIndex LastIndex() const { return last_index_; }
    static constexpr size_t kMaxSegmentSize = 64 * 1024 * 1024; // 64 MB
private:
    Status Rotate();
    std::string dir_;
    std::string current_path_;
    int fd_ = -1;
    size_t current_size_ = 0;
    uint32_t segment_seq_ = 0;
    RaftIndex first_index_ = 0;
    RaftIndex last_index_ = 0;
};
}} // namespace
