// kv_protocol.h — Message type enum and wire format constants
#pragma once

#include <cstdint>
#include <cstddef>

namespace zero {
namespace kvstore {

// ============================================================
// MsgType — message type enumeration
// ============================================================
enum class MsgType : uint16_t {
    // ---- KV operations ----
    KV_GET    = 100,
    KV_PUT    = 101,
    KV_DELETE = 102,
    KV_SCAN   = 103,
    KV_BATCH  = 104,

    // ---- Raft consensus ----
    RAFT_VOTE_REQ   = 200,
    RAFT_VOTE_RSP   = 201,
    RAFT_APPEND_REQ = 202,
    RAFT_APPEND_RSP = 203,
    RAFT_SNAP_REQ   = 204,
    RAFT_SNAP_RSP   = 205,

    // ---- Admin ----
    ADMIN_STATUS = 300,
    ADMIN_JOIN   = 301,
    ADMIN_LEAVE  = 302,
};

// ============================================================
// Wire format constants
//
// Frame layout (little-endian unless noted):
//   [4B magic = 0x4B563031 "KV01"][2B msg_type][4B body_len][body][4B CRC32C]
//   — total header = 4 + 2 + 4 + 4 = 14 bytes
// ============================================================

// "KV01" stored as little-endian uint32: 'K'|('V'<<8)|('0'<<16)|('1'<<24)
constexpr uint32_t kFrameMagic     = 0x3130564B;

// Header size excluding the trailing CRC32C (which is computed over header+body)
//   magic(4) + type(2) + body_len(4) = 10 bytes
constexpr size_t kFrameHeaderSize = 10;

// Total frame overhead: header(10) + trailing CRC32C(4) = 14 bytes
constexpr size_t kFrameOverhead   = 14;

// Maximum body size to avoid excessive allocations
constexpr size_t kMaxBodySize     = 64 * 1024 * 1024;  // 64 MiB

// Snapshot chunk size hint
constexpr size_t kMaxSnapshotChunk = 1024 * 1024;       // 1 MiB per chunk

// ---- helper to get a human-readable type name ----
inline const char* MsgTypeName(MsgType t) {
    switch (t) {
        case MsgType::KV_GET:          return "KV_GET";
        case MsgType::KV_PUT:          return "KV_PUT";
        case MsgType::KV_DELETE:       return "KV_DELETE";
        case MsgType::KV_SCAN:         return "KV_SCAN";
        case MsgType::KV_BATCH:        return "KV_BATCH";
        case MsgType::RAFT_VOTE_REQ:   return "RAFT_VOTE_REQ";
        case MsgType::RAFT_VOTE_RSP:   return "RAFT_VOTE_RSP";
        case MsgType::RAFT_APPEND_REQ: return "RAFT_APPEND_REQ";
        case MsgType::RAFT_APPEND_RSP: return "RAFT_APPEND_RSP";
        case MsgType::RAFT_SNAP_REQ:   return "RAFT_SNAP_REQ";
        case MsgType::RAFT_SNAP_RSP:   return "RAFT_SNAP_RSP";
        case MsgType::ADMIN_STATUS:    return "ADMIN_STATUS";
        case MsgType::ADMIN_JOIN:      return "ADMIN_JOIN";
        case MsgType::ADMIN_LEAVE:     return "ADMIN_LEAVE";
        default:                       return "UNKNOWN";
    }
}

} // namespace kvstore
} // namespace zero
