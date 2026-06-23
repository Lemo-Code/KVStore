// wal_reader.cc — WAL reader implementation
#include "kvstore/storage/wal_reader.h"
#include "kvstore/storage/wal_writer.h"
#include "kvstore/common/kv_utils.h"

#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <sstream>
#include <iostream>

namespace zero {
namespace kvstore {

namespace {

// Read a 4-byte little-endian value from a buffer
inline uint32_t ReadLE32(const char* p) {
    return static_cast<uint8_t>(p[0]) |
           (static_cast<uint32_t>(static_cast<uint8_t>(p[1])) << 8) |
           (static_cast<uint32_t>(static_cast<uint8_t>(p[2])) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(p[3])) << 24);
}

// Read an 8-byte little-endian value from a buffer
inline uint64_t ReadLE64(const char* p) {
    uint64_t lo = ReadLE32(p);
    uint64_t hi = ReadLE32(p + 4);
    return (hi << 32) | lo;
}

// Parse sequence number from filename "WAL_000000001.log"
int ParseSegmentSeq(const std::string& name) {
    // Expect "WAL_" prefix and ".log" suffix
    const char* p = name.c_str();
    const char* end = p + name.size();

    // Skip "WAL_"
    if (name.size() < 8) return -1;
    if (std::strncmp(p, "WAL_", 4) != 0) return -1;
    p += 4;

    // Read digits
    int seq = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        seq = seq * 10 + (*p - '0');
        ++p;
    }

    // Ensure ".log" suffix
    if (p >= end || std::strcmp(p, ".log") != 0) return -1;
    return seq;
}

} // anonymous namespace

WalReader::~WalReader() {
    Close();
}

// ============================================================
// Discover WAL segment files
// ============================================================

Status WalReader::DiscoverSegments(const std::string& dir) {
    segments_.clear();

    DIR* dp = ::opendir(dir.c_str());
    if (!dp) {
        // Directory does not exist or cannot be opened
        return Status::OK();  // empty WAL is not an error
    }

    struct dirent* entry;
    while ((entry = ::readdir(dp)) != nullptr) {
        int seq = ParseSegmentSeq(entry->d_name);
        if (seq < 0) continue;

        Segment seg;
        seg.path = dir + "/" + entry->d_name;
        seg.seq  = seq;
        segments_.push_back(std::move(seg));
    }
    ::closedir(dp);

    // Sort by sequence number ascending
    std::sort(segments_.begin(), segments_.end(),
              [](const Segment& a, const Segment& b) { return a.seq < b.seq; });

    return Status::OK();
}

// ============================================================
// Lifecycle
// ============================================================

Status WalReader::Open(const std::string& dir) {
    Close();

    Status s = DiscoverSegments(dir);
    if (!s.ok()) return s;

    if (segments_.empty()) {
        return Status::OK();  // no WAL files yet
    }

    return OpenSegment(0);
}

Status WalReader::OpenSegment(size_t idx) {
    if (idx >= segments_.size()) {
        return Status::IOError("segment index out of range");
    }

    current_file_.close();

    current_file_.open(segments_[idx].path,
                       std::ios::binary | std::ios::in);
    if (!current_file_.is_open()) {
        return Status::IOError("cannot open WAL segment: " +
                               segments_[idx].path);
    }

    current_seg_idx_ = idx;
    return Status::OK();
}

// ============================================================
// Seek
// ============================================================

bool WalReader::Seek(Index target) {
    if (segments_.empty()) return false;

    // Fast path: if we already have first_index metadata, we can skip segments
    // But since we haven't scanned yet, we do a linear scan through records.

    // First, reset to the first segment
    if (!current_file_.is_open() || current_seg_idx_ != 0) {
        Status s = OpenSegment(0);
        if (!s.ok()) return false;
    } else {
        current_file_.clear();
        current_file_.seekg(0, std::ios::beg);
    }

    Index idx;
    Term term;
    std::string data;

    while (true) {
        // Save position before reading
        auto pos = current_file_.tellg();

        if (!ReadOneRecord(idx, term, data)) {
            // Try next segment
            if (current_seg_idx_ + 1 >= segments_.size()) {
                return false; // no more segments
            }
            Status s = OpenSegment(current_seg_idx_ + 1);
            if (!s.ok()) return false;
            continue;
        }

        if (idx == target) {
            // Rewind so ReadNext will return this record
            current_file_.clear();
            current_file_.seekg(pos, std::ios::beg);
            return true;
        }

        if (idx > target) {
            // Went past target — target not in WAL
            // Rewind so the next ReadNext gets this record
            current_file_.clear();
            current_file_.seekg(pos, std::ios::beg);
            return false;
        }
    }
}

// ============================================================
// ReadNext
// ============================================================

bool WalReader::ReadNext(Index& index, Term& term, std::string& data) {
    while (true) {
        if (!current_file_.is_open()) {
            // Try to advance to next segment
            if (current_seg_idx_ + 1 >= segments_.size()) {
                return false; // no more segments
            }
            Status s = OpenSegment(current_seg_idx_ + 1);
            if (!s.ok()) return false;
            continue;
        }

        if (ReadOneRecord(index, term, data)) {
            return true;
        }

        // Check if we hit EOF
        if (current_file_.eof()) {
            // Try next segment
            if (current_seg_idx_ + 1 >= segments_.size()) {
                return false;
            }
            Status s = OpenSegment(current_seg_idx_ + 1);
            if (!s.ok()) return false;
            continue;
        }

        // File is in an error state — try next segment
        if (!current_file_.good()) {
            if (current_seg_idx_ + 1 >= segments_.size()) {
                return false;
            }
            Status s = OpenSegment(current_seg_idx_ + 1);
            if (!s.ok()) return false;
            continue;
        }

        return false;
    }
}

// ============================================================
// ReadOneRecord — parse a single record from the current file stream
// ============================================================

bool WalReader::ReadOneRecord(Index& index, Term& term, std::string& data) {
    // Read magic (8 bytes)
    char magic_buf[8];
    if (!current_file_.read(magic_buf, 8)) {
        return false;
    }
    uint64_t magic = ReadLE64(magic_buf);
    if (magic != 0x4B56414C303100) {
        // Corrupted: try to skip forward one byte at a time looking for magic
        // (simple recovery)
        std::cerr << "[WAL] corrupted record: bad magic, skipping byte"
                  << std::endl;
        // Seek back 7 bytes (we already consumed 8, so go back 7 to advance 1)
        current_file_.clear();
        auto pos = current_file_.tellg();
        if (pos > 0) {
            current_file_.seekg(pos - std::streamoff(7), std::ios::beg);
        }
        return false;
    }

    // Read record_crc32, term, data_len
    char header_rest[12];
    if (!current_file_.read(header_rest, 12)) {
        return false;
    }
    uint32_t record_crc = ReadLE32(header_rest);
    uint32_t rec_term   = ReadLE32(header_rest + 4);
    uint32_t data_len   = ReadLE32(header_rest + 8);

    // Read data
    data.resize(data_len);
    if (data_len > 0) {
        if (!current_file_.read(&data[0], data_len)) {
            return false;
        }
    }

    // Read tail CRC32C
    char tail_buf[4];
    if (!current_file_.read(tail_buf, 4)) {
        return false;
    }
    uint32_t stored_tail_crc = ReadLE32(tail_buf);

    // ---- Validate ----

    // Compute body CRC32C over: term(4B LE) + data_len(4B LE) + data
    char tmp[8];
    {
        uint32_t t = rec_term;
        tmp[0] = static_cast<char>(t & 0xFF);
        tmp[1] = static_cast<char>((t >> 8) & 0xFF);
        tmp[2] = static_cast<char>((t >> 16) & 0xFF);
        tmp[3] = static_cast<char>((t >> 24) & 0xFF);
        t = data_len;
        tmp[4] = static_cast<char>(t & 0xFF);
        tmp[5] = static_cast<char>((t >> 8) & 0xFF);
        tmp[6] = static_cast<char>((t >> 16) & 0xFF);
        tmp[7] = static_cast<char>((t >> 24) & 0xFF);
    }
    uint32_t computed_body_crc = CRC32C(tmp, 8);
    if (data_len > 0) {
        computed_body_crc = CRC32CCombine(computed_body_crc, data.data(), data_len);
    }

    if (computed_body_crc != record_crc) {
        std::cerr << "[WAL] corrupted record: body CRC32C mismatch" << std::endl;
        return false;
    }

    // Compute tail CRC32C over: magic(8) + record_crc(4) + term(4) + data_len(4) + data
    uint32_t computed_tail_crc = CRC32C(magic_buf, 8);
    computed_tail_crc = CRC32CCombine(computed_tail_crc, header_rest, 12);
    if (data_len > 0) {
        computed_tail_crc = CRC32CCombine(computed_tail_crc, data.data(), data_len);
    }

    if (computed_tail_crc != stored_tail_crc) {
        std::cerr << "[WAL] corrupted record: tail CRC32C mismatch"
                  << std::endl;
        return false;
    }

    // ---- Success ----
    term = rec_term;
    index = 0; // Index is not explicitly stored in the WAL record header,
               // it would be serialized within `data`. The caller typically
               // reconstructs the index from context (Raft log position).
    return true;
}

// ============================================================
// Close
// ============================================================

Status WalReader::Close() {
    if (current_file_.is_open()) {
        current_file_.close();
    }
    segments_.clear();
    current_seg_idx_ = 0;
    return Status::OK();
}

} // namespace kvstore
} // namespace zero
