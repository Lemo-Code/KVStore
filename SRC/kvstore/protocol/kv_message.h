// kv_message.h — Message structs with binary serialization
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <utility>
#include <cstring>
#include <stdexcept>

#include "kvstore/common/kv_types.h"
#include "kvstore/protocol/kv_protocol.h"

namespace zero {
namespace kvstore {

// ============================================================
// Serialization helpers (internal)
// ============================================================
namespace detail {

inline void WriteU16(uint16_t v, std::string& out) {
    out.push_back(static_cast<char>(v & 0xFF));
    out.push_back(static_cast<char>((v >> 8) & 0xFF));
}

inline uint16_t ReadU16(const char*& p, const char* end) {
    if (p + 2 > end) throw std::runtime_error("ReadU16: buffer underrun");
    uint16_t v = static_cast<uint8_t>(p[0]) |
                 (static_cast<uint16_t>(static_cast<uint8_t>(p[1])) << 8);
    p += 2;
    return v;
}

inline void WriteU32(uint32_t v, std::string& out) {
    out.push_back(static_cast<char>(v & 0xFF));
    out.push_back(static_cast<char>((v >> 8) & 0xFF));
    out.push_back(static_cast<char>((v >> 16) & 0xFF));
    out.push_back(static_cast<char>((v >> 24) & 0xFF));
}

inline uint32_t ReadU32(const char*& p, const char* end) {
    if (p + 4 > end) throw std::runtime_error("ReadU32: buffer underrun");
    uint32_t v = static_cast<uint8_t>(p[0]) |
                 (static_cast<uint32_t>(static_cast<uint8_t>(p[1])) << 8) |
                 (static_cast<uint32_t>(static_cast<uint8_t>(p[2])) << 16) |
                 (static_cast<uint32_t>(static_cast<uint8_t>(p[3])) << 24);
    p += 4;
    return v;
}

inline void WriteU64(uint64_t v, std::string& out) {
    WriteU32(static_cast<uint32_t>(v & 0xFFFFFFFF), out);
    WriteU32(static_cast<uint32_t>(v >> 32), out);
}

inline uint64_t ReadU64(const char*& p, const char* end) {
    uint64_t lo = ReadU32(p, end);
    uint64_t hi = ReadU32(p, end);
    return (hi << 32) | lo;
}

inline void WriteStr(const std::string& s, std::string& out) {
    WriteU32(static_cast<uint32_t>(s.size()), out);
    out.append(s);
}

inline std::string ReadStr(const char*& p, const char* end) {
    uint32_t len = ReadU32(p, end);
    if (p + len > end) throw std::runtime_error("ReadStr: buffer underrun");
    std::string s(p, len);
    p += len;
    return s;
}

inline void WriteBool(bool v, std::string& out) {
    out.push_back(v ? '\x01' : '\x00');
}

inline bool ReadBool(const char*& p, const char* end) {
    if (p + 1 > end) throw std::runtime_error("ReadBool: buffer underrun");
    bool v = (*p != '\x00');
    p += 1;
    return v;
}

} // namespace detail

// ============================================================
// Raft messages
// ============================================================

//
// RequestVoteReq — sent by candidate during election
//
struct KvGetReq {
    Key key;

    void Serialize(std::string& out) const {
        detail::WriteStr(key, out);
    }
    bool Deserialize(const char* data, size_t len) {
        const char* p = data;
        try { key = detail::ReadStr(p, data + len); return true; }
        catch (const std::exception&) { return false; }
    }
};

struct KvGetRsp {
    Value value;
    bool  found = false;

    void Serialize(std::string& out) const {
        detail::WriteBool(found, out);
        if (found) detail::WriteStr(value, out);
    }
    bool Deserialize(const char* data, size_t len) {
        const char* p = data;
        const char* end = data + len;
        try {
            found = detail::ReadBool(p, end);
            if (found) value = detail::ReadStr(p, end);
            return true;
        } catch (const std::exception&) { return false; }
    }
};

//
// KvPutReq / KvPutRsp
//
struct KvPutReq {
    Key   key;
    Value value;

    void Serialize(std::string& out) const {
        detail::WriteStr(key, out);
        detail::WriteStr(value, out);
    }
    bool Deserialize(const char* data, size_t len) {
        const char* p = data;
        const char* end = data + len;
        try {
            key   = detail::ReadStr(p, end);
            value = detail::ReadStr(p, end);
            return true;
        } catch (const std::exception&) { return false; }
    }
};

struct KvPutRsp {
    bool ok = false;

    void Serialize(std::string& out) const {
        detail::WriteBool(ok, out);
    }
    bool Deserialize(const char* data, size_t len) {
        if (len < 1) return false;
        ok = (data[0] != '\x00');
        return true;
    }
};

//
// KvDeleteReq / KvDeleteRsp
//
struct KvDeleteReq {
    Key key;

    void Serialize(std::string& out) const {
        detail::WriteStr(key, out);
    }
    bool Deserialize(const char* data, size_t len) {
        const char* p = data;
        try { key = detail::ReadStr(p, data + len); return true; }
        catch (const std::exception&) { return false; }
    }
};

struct KvDeleteRsp {
    bool ok = false;

    void Serialize(std::string& out) const {
        detail::WriteBool(ok, out);
    }
    bool Deserialize(const char* data, size_t len) {
        if (len < 1) return false;
        ok = (data[0] != '\x00');
        return true;
    }
};

//
// KvScanReq / KvScanRsp
//
struct KvScanReq {
    Key    start;
    Key    limit;          // empty = unbounded
    size_t max_count = 100;

    void Serialize(std::string& out) const {
        detail::WriteStr(start, out);
        detail::WriteStr(limit, out);
        detail::WriteU32(static_cast<uint32_t>(max_count), out);
    }
    bool Deserialize(const char* data, size_t len) {
        const char* p = data;
        const char* end = data + len;
        try {
            start     = detail::ReadStr(p, end);
            limit     = detail::ReadStr(p, end);
            max_count = detail::ReadU32(p, end);
            return true;
        } catch (const std::exception&) { return false; }
    }
};

struct KvScanRsp {
    std::vector<KeyValue> results;

    void Serialize(std::string& out) const {
        detail::WriteU32(static_cast<uint32_t>(results.size()), out);
        for (const auto& kv : results) {
            detail::WriteStr(kv.key, out);
            detail::WriteStr(kv.val, out);
        }
    }
    bool Deserialize(const char* data, size_t len) {
        const char* p = data;
        const char* end = data + len;
        try {
            uint32_t count = detail::ReadU32(p, end);
            results.clear();
            results.reserve(count);
            for (uint32_t i = 0; i < count; ++i) {
                Key k = detail::ReadStr(p, end);
                Value v = detail::ReadStr(p, end);
                results.emplace_back(std::move(k), std::move(v));
            }
            return true;
        } catch (const std::exception&) { return false; }
    }
};

//
// KvBatchReq / KvBatchRsp
//
struct KvBatchReq {
    Batch batch;

    void Serialize(std::string& out) const {
        detail::WriteU32(static_cast<uint32_t>(batch.size()), out);
        for (const auto& be : batch) {
            detail::WriteU64(static_cast<uint64_t>(static_cast<uint8_t>(be.op)), out);
            detail::WriteStr(be.key, out);
            detail::WriteStr(be.val, out);
        }
    }
    bool Deserialize(const char* data, size_t len) {
        const char* p = data;
        const char* end = data + len;
        try {
            uint32_t count = detail::ReadU32(p, end);
            batch.clear();
            batch.reserve(count);
            for (uint32_t i = 0; i < count; ++i) {
                uint64_t op_raw = detail::ReadU64(p, end);
                BatchOp op = static_cast<BatchOp>(static_cast<uint8_t>(op_raw));
                Key k = detail::ReadStr(p, end);
                Value v = detail::ReadStr(p, end);
                batch.push_back({op, std::move(k), std::move(v)});
            }
            return true;
        } catch (const std::exception&) { return false; }
    }
};

struct KvBatchRsp {
    bool    ok        = false;
    int32_t processed = 0;  // number of entries applied

    void Serialize(std::string& out) const {
        detail::WriteBool(ok, out);
        detail::WriteU32(static_cast<uint32_t>(processed), out);
    }
    bool Deserialize(const char* data, size_t len) {
        const char* p = data;
        const char* end = data + len;
        try {
            ok = detail::ReadBool(p, end);
            processed = static_cast<int32_t>(detail::ReadU32(p, end));
            return true;
        } catch (const std::exception&) { return false; }
    }
};

// ============================================================
// Admin messages
// ============================================================

//
// AdminStatusRsp — node status response
//
struct AdminStatusRsp {
    NodeId   node_id;
    std::string role;        // "leader", "follower", "candidate"
    Term     term      = 0;
    NodeId   leader_id;
    int      shard_count = 0;
    size_t   key_count   = 0;
    int64_t  uptime_ms   = 0;

    void Serialize(std::string& out) const {
        detail::WriteStr(node_id, out);
        detail::WriteStr(role, out);
        detail::WriteU64(term, out);
        detail::WriteStr(leader_id, out);
        detail::WriteU32(static_cast<uint32_t>(shard_count), out);
        detail::WriteU64(static_cast<uint64_t>(key_count), out);
        detail::WriteU64(static_cast<uint64_t>(uptime_ms), out);
    }
    bool Deserialize(const char* data, size_t len) {
        const char* p = data;
        const char* end = data + len;
        try {
            node_id     = detail::ReadStr(p, end);
            role        = detail::ReadStr(p, end);
            term        = detail::ReadU64(p, end);
            leader_id   = detail::ReadStr(p, end);
            shard_count = static_cast<int>(detail::ReadU32(p, end));
            key_count   = static_cast<size_t>(detail::ReadU64(p, end));
            uptime_ms   = static_cast<int64_t>(detail::ReadU64(p, end));
            return true;
        } catch (const std::exception&) { return false; }
    }
};

// ---- convenience: determine MsgType from a message object ----
// (useful when encoding frames)

inline MsgType MessageType(const KvGetReq&)           { return MsgType::KV_GET; }
inline MsgType MessageType(const KvGetRsp&)           { return MsgType::KV_GET; }
inline MsgType MessageType(const KvPutReq&)           { return MsgType::KV_PUT; }
inline MsgType MessageType(const KvPutRsp&)           { return MsgType::KV_PUT; }
inline MsgType MessageType(const KvDeleteReq&)        { return MsgType::KV_DELETE; }
inline MsgType MessageType(const KvDeleteRsp&)        { return MsgType::KV_DELETE; }
inline MsgType MessageType(const KvScanReq&)          { return MsgType::KV_SCAN; }
inline MsgType MessageType(const KvScanRsp&)          { return MsgType::KV_SCAN; }
inline MsgType MessageType(const KvBatchReq&)         { return MsgType::KV_BATCH; }
inline MsgType MessageType(const KvBatchRsp&)         { return MsgType::KV_BATCH; }
inline MsgType MessageType(const AdminStatusRsp&)     { return MsgType::ADMIN_STATUS; }

} // namespace kvstore
} // namespace zero
