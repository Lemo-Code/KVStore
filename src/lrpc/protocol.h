#pragma once
// ============================================================
// lrpc/protocol.h — RPC 二进制协议定义
// ============================================================
//
// Wire format (大端/网络字节序):
//   [4B] Magic:  0x5250434B ("RPCK")
//   [4B] FrameLen: 总帧长 (含 Magic + FrameLen 自身)
//   [4B] CallId:   调用 ID (0 = 单向通知，无需响应)
//   [2B] MsgType:  消息类型
//   [1B] Flags:    bit0=is_response, bit1=is_error
//   [1B] Reserved
//   [4B] BodyLen:  body 长度
//   [HeaderBlock]  键值对头 (可选)
//   [Body]         载荷
//
// HeaderBlock 格式:
//   [2B] HeaderCount
//   For each header:
//     [2B] KeyLen + Key + [4B] ValLen + Val
//

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <lstl/container/vector.h>
#include <arpa/inet.h>

namespace lrpc {

// ---- 协议常量 ----
static constexpr uint32_t RPC_MAGIC       = 0x5250434B;  // "RPCK"
static constexpr size_t   FRAME_HDR_SIZE  = 20;           // Magic+Len+CallId+Type+Flags+Resv+BodyLen

// ---- 消息类型 (集群预定义) ----
enum class MsgType : uint16_t {
    // Gossip
    PING           = 0,
    PONG           = 1,
    MEET           = 2,
    FAIL           = 3,

    // 命令转发
    FORWARD        = 10,
    FORWARD_REPLY  = 11,

    // 配置同步
    UPDATE_CONFIG  = 20,

    // 复制
    REPL_CONF      = 30,
    REPL_ACK       = 31,

    // 迁移
    MIGRATE_START  = 40,
    MIGRATE_DATA   = 41,
    MIGRATE_ACK    = 42,
    MIGRATE_COMMIT = 43,

    // Pub/Sub 广播
    PUBLISH        = 50,

    MAX = 65535
};

// ---- 标志位 ----
static constexpr uint8_t FLAG_RESPONSE = 0x01;
static constexpr uint8_t FLAG_ERROR    = 0x02;
static constexpr uint8_t FLAG_ONEWAY   = 0x04;

// ---- 帧头 (20 字节) ----
#pragma pack(push, 1)
struct FrameHeader {
    uint32_t magic;
    uint32_t frame_len;
    uint32_t call_id;
    uint16_t msg_type;
    uint8_t  flags;
    uint8_t  reserved;
    uint32_t body_len;
};
#pragma pack(pop)

static_assert(sizeof(FrameHeader) == FRAME_HDR_SIZE, "FrameHeader must be 20 bytes");

// ---- 编码/解码辅助 ----
inline void encodeU32(uint8_t* buf, uint32_t v) {
    uint32_t n = htonl(v);
    memcpy(buf, &n, 4);
}
inline uint32_t decodeU32(const uint8_t* buf) {
    uint32_t n;
    memcpy(&n, buf, 4);
    return ntohl(n);
}
inline void encodeU16(uint8_t* buf, uint16_t v) {
    uint16_t n = htons(v);
    memcpy(buf, &n, 2);
}
inline uint16_t decodeU16(const uint8_t* buf) {
    uint16_t n;
    memcpy(&n, buf, 2);
    return ntohs(n);
}

// ---- 帧头编解码 ----
inline void encodeFrameHeader(uint8_t* buf, const FrameHeader& hdr) {
    encodeU32(buf,      hdr.magic);
    encodeU32(buf + 4,  hdr.frame_len);
    encodeU32(buf + 8,  hdr.call_id);
    encodeU16(buf + 12, hdr.msg_type);
    buf[14] = hdr.flags;
    buf[15] = hdr.reserved;
    encodeU32(buf + 16, hdr.body_len);
}

inline bool decodeFrameHeader(const uint8_t* buf, size_t len, FrameHeader& hdr) {
    if (len < FRAME_HDR_SIZE) return false;
    hdr.magic     = decodeU32(buf);
    if (hdr.magic != RPC_MAGIC) return false;
    hdr.frame_len = decodeU32(buf + 4);
    if (hdr.frame_len < FRAME_HDR_SIZE) return false;
    hdr.call_id   = decodeU32(buf + 8);
    hdr.msg_type  = decodeU16(buf + 12);
    hdr.flags     = buf[14];
    hdr.reserved  = buf[15];
    hdr.body_len  = decodeU32(buf + 16);
    return true;
}

// ---- Header 块编解码 ----
// 返回编码后的字节数
inline size_t encodeHeaders(uint8_t* buf, size_t cap,
                            const lstl::vector<std::pair<std::string, std::string>>& headers) {
    size_t pos = 0;
    // header count (2 bytes)
    if (pos + 2 > cap) return 0;
    encodeU16(buf + pos, static_cast<uint16_t>(headers.size()));
    pos += 2;
    for (auto& h : headers) {
        // key len (2B) + key + val len (4B) + val
        uint16_t klen = static_cast<uint16_t>(h.first.size());
        uint32_t vlen = static_cast<uint32_t>(h.second.size());
        if (pos + 2 + klen + 4 + vlen > cap) return 0;
        encodeU16(buf + pos, klen); pos += 2;
        if (klen) { memcpy(buf + pos, h.first.data(), klen); pos += klen; }
        encodeU32(buf + pos, vlen); pos += 4;
        if (vlen) { memcpy(buf + pos, h.second.data(), vlen); pos += vlen; }
    }
    return pos;
}

inline size_t decodeHeaders(const uint8_t* buf, size_t len,
                            lstl::vector<std::pair<std::string, std::string>>& headers) {
    size_t pos = 0;
    if (pos + 2 > len) return 0;
    uint16_t count = decodeU16(buf + pos); pos += 2;
    for (uint16_t i = 0; i < count; ++i) {
        if (pos + 2 > len) return 0;
        uint16_t klen = decodeU16(buf + pos); pos += 2;
        if (pos + klen > len) return 0;
        std::string key(reinterpret_cast<const char*>(buf + pos), klen); pos += klen;
        if (pos + 4 > len) return 0;
        uint32_t vlen = decodeU32(buf + pos); pos += 4;
        if (pos + vlen > len) return 0;
        std::string val(reinterpret_cast<const char*>(buf + pos), vlen); pos += vlen;
        headers.push_back(std::make_pair(std::move(key), std::move(val)));
    }
    return pos;
}

// 计算 header 块编码后的大小
inline size_t headerBlockSize(const lstl::vector<std::pair<std::string, std::string>>& headers) {
    size_t sz = 2;  // count
    for (auto& h : headers)
        sz += 2 + h.first.size() + 4 + h.second.size();
    return sz;
}

// ---- 帧总大小计算 ----
inline size_t frameSize(size_t bodyLen, size_t headerBlockSz) {
    return FRAME_HDR_SIZE + headerBlockSz + bodyLen;
}

} // namespace lrpc
