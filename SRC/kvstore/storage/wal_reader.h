// wal_reader.h — Write-Ahead Log reader / iterator
#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <cstdint>

#include "kvstore/common/kv_types.h"
#include "kvstore/common/kv_error.h"

namespace zero {
namespace kvstore {

// ============================================================
// WalReader — reads WAL files sequentially
//
// - Open(dir) discovers all WAL segment files and sorts them.
// - Seek(index) positions the reader at (or after) the given Raft log index.
//   Returns true if the index was found and the reader is now positioned
//   at that record; returns false if the index is not in the WAL.
// - ReadNext(&index, &term, &data) reads the next record. Returns true
//   if a valid record was read; false on EOF or unrecoverable corruption.
// - Corrupted records are skipped (with a warning to stderr) so that as
//   much data as possible can be recovered.
// ============================================================

class WalReader {
public:
    WalReader() = default;
    ~WalReader();

    // Non-copyable, non-movable
    WalReader(const WalReader&) = delete;
    WalReader& operator=(const WalReader&) = delete;

    // ---- lifecycle ----
    // Open discovers all WAL_*.log files under dir, opens the first one,
    // and positions at the start.
    Status Open(const std::string& dir);

    // Seek positions the reader at (or just after) the given log index.
    // Returns true if the exact index was found.
    bool Seek(Index index);

    // ReadNext reads the next WAL record. Returns true if a valid record
    // was read; false if EOF was reached or the file could not be read.
    bool ReadNext(Index& index, Term& term, std::string& data);

    // Close releases file handles.
    Status Close();

    // ---- accessors ----
    bool is_open() const { return !segments_.empty(); }

private:
    struct Segment {
        std::string   path;
        int           seq = 0;
        Index         first_index = kInvalidIndex;
        Index         last_index  = kInvalidIndex;
    };

    std::vector<Segment> segments_;
    size_t               current_seg_idx_ = 0;
    std::ifstream        current_file_;

    // Internal: discover and sort WAL segment files
    Status DiscoverSegments(const std::string& dir);

    // Internal: open the segment at segments_[idx]
    Status OpenSegment(size_t idx);

    // Internal: try to read a single record from the current file.
    // Returns true on success. If the record is corrupted, logs a warning
    // and attempts to skip to the next valid magic marker.
    bool ReadOneRecord(Index& index, Term& term, std::string& data);
};

} // namespace kvstore
} // namespace zero
