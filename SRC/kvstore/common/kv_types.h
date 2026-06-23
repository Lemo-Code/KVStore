// kv_types.h — Fundamental types for the distributed KV store
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <sstream>

namespace zero {
namespace kvstore {

// ---- primitive aliases ----
using Key     = std::string;
using Value   = std::string;
using NodeId  = std::string;
using Term    = uint64_t;
using Index   = uint64_t;
using ShardId = uint16_t;
using SlotId  = uint16_t;

// ---- constants ----
constexpr SlotId  kNumSlots      = 16384;   // CRC16 hash space
constexpr Term    kInvalidTerm   = 0;
constexpr Index   kInvalidIndex  = 0;
constexpr ShardId kInvalidShard  = UINT16_MAX;

// ---- key/value pair ----
struct KeyValue {
    Key   key;
    Value val;
    bool  tombstone = false;  // deletion marker

    KeyValue() = default;
    KeyValue(Key k, Value v, bool del = false)
        : key(std::move(k)), val(std::move(v)), tombstone(del) {}
};

// ---- key range (inclusive start, exclusive end) ----
struct KeyRange {
    Key start;
    Key limit;   // empty = unbounded

    bool Contains(const Key& k) const {
        if (k < start) return false;
        if (!limit.empty() && k >= limit) return false;
        return true;
    }
};

// ---- slot range ----
struct SlotRange {
    SlotId first;
    SlotId last;  // inclusive

    bool Contains(SlotId s) const { return s >= first && s <= last; }
    uint32_t Size() const { return last - first + 1; }
};

// ---- node address ----
struct NodeAddr {
    NodeId     id;
    std::string host;
    uint16_t   client_port = 9700;
    uint16_t   raft_port   = 9701;

    std::string String() const {
        std::ostringstream oss;
        oss << host << ":" << client_port;
        return oss.str();
    }
};

// ---- Raft peer descriptor ----
struct RaftPeer {
    NodeId  id;
    NodeAddr addr;
    Index   match_index = 0;
    Index   next_index  = 1;
};

// ---- batch operation for Raft apply ----
enum class BatchOp : uint8_t { PUT = 1, DEL = 2 };

struct BatchEntry {
    BatchOp op;
    Key     key;
    Value   val;
};

using Batch = std::vector<BatchEntry>;

// ---- hash function ----
uint16_t HashSlot(const Key& key);

// ---- time helpers ----
int64_t NowMs();
int64_t NowUs();

} // namespace kvstore
} // namespace zero
