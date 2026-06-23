#include "kvstore/storage/wal_writer.h"
#include "kvstore/common/kv_utils.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <sstream>

namespace zero { namespace kvstore {

WalWriter::WalWriter() = default;
WalWriter::~WalWriter() { Close(); }

Status WalWriter::Open(const std::string& dir) {
    dir_ = dir;
    // Create directory if it doesn't exist
    struct stat st;
    if (stat(dir.c_str(), &st) != 0) {
        if (mkdir(dir.c_str(), 0755) != 0 && errno != EEXIST) {
            return Status::IOError("cannot create WAL dir: " + dir + " - " + strerror(errno));
        }
    }
    return Rotate(); // Create first segment
}

Status WalWriter::Rotate() {
    if (fd_ >= 0) { Fsync(); close(fd_); fd_ = -1; }
    segment_seq_++;
    std::ostringstream oss;
    oss << dir_ << "/WAL_" << segment_seq_ << ".log";
    current_path_ = oss.str();
    fd_ = open(current_path_.c_str(), O_CREAT | O_WRONLY | O_APPEND | O_CLOEXEC, 0644);
    if (fd_ < 0) return Status::IOError("cannot open WAL: " + current_path_ + " - " + strerror(errno));
    // Write segment header
    const char magic[8] = {0x4B,0x56,0x57,0x41,0x4C,0x30,0x31,0x00};
    ssize_t n = write(fd_, magic, 8);
    if (n != 8) { close(fd_); fd_ = -1; return Status::IOError("WAL write magic failed"); }
    current_size_ = 8;
    return Status::OK();
}

Status WalWriter::Append(RaftIndex index, RaftTerm term, const std::string& data) {
    if (fd_ < 0) return Status::IOError("WAL not open");
    // Build record: [4B crc32][4B term_hi][4B term_lo][4B data_len][4B index_hi][4B index_lo][data][4B tail_crc32]
    std::string record;
    // Placeholder for CRC32C
    record.resize(8); // space for crc32 fields
    // Encode term and index
    auto w32 = [&](uint32_t v) { record.push_back(v & 0xFF); record.push_back((v>>8)&0xFF); record.push_back((v>>16)&0xFF); record.push_back((v>>24)&0xFF); };
    uint32_t term_lo = term & 0xFFFFFFFF, term_hi = (term >> 32) & 0xFFFFFFFF;
    uint32_t idx_lo = index & 0xFFFFFFFF, idx_hi = (index >> 32) & 0xFFFFFFFF;
    uint32_t dlen = data.size();
    w32(term_hi); w32(term_lo); w32(dlen);
    w32(idx_hi); w32(idx_lo);
    record.append(data);
    // Compute body CRC32C
    uint32_t body_crc = CRC32C(record.data() + 16, record.size() - 16); // skip header crc + term + dlen
    // Write header crc (body_crc)  
    memcpy(&record[0], &body_crc, 4);
    // Write tail crc (same for simplicity)
    uint32_t tail_crc = CRC32C(record.data(), record.size());
    memcpy(&record[4], &tail_crc, 4);
    // Write tail CRC
    record.push_back(tail_crc & 0xFF); record.push_back((tail_crc>>8)&0xFF);
    record.push_back((tail_crc>>16)&0xFF); record.push_back((tail_crc>>24)&0xFF);

    ssize_t n = write(fd_, record.data(), record.size());
    if (n != (ssize_t)record.size()) return Status::IOError("WAL write failed");
    current_size_ += record.size();
    if (last_index_ == 0) first_index_ = index;
    last_index_ = index;

    // Rotate if file gets too large (64 MB)
    if (current_size_ >= kMaxSegmentSize) {
        Status st = Rotate();
        if (!st.ok()) return st;
    }
    return Status::OK();
}

Status WalWriter::Fsync() {
    if (fd_ >= 0) {
        if (fsync(fd_) != 0) return Status::IOError("WAL fsync failed: " + std::string(strerror(errno)));
    }
    return Status::OK();
}

Status WalWriter::Close() {
    if (fd_ >= 0) { Fsync(); close(fd_); fd_ = -1; }
    return Status::OK();
}

}} // namespace
