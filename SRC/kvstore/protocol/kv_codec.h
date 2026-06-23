// kv_codec.h — Frame encode/decode using wire format
#pragma once

#include <string>
#include <cstddef>

#include "kvstore/protocol/kv_protocol.h"

namespace zero {
namespace kvstore {

// ============================================================
// Frame codec — wraps/unwraps message bodies with the header
// ============================================================

// EncodeFrame builds a complete wire frame:
//   [4B magic][2B type][4B body_len][body][4B CRC32C]
//
// The CRC32C covers: magic(4) + type(2) + body_len(4) + body
//
// Returns true on success, false if body exceeds kMaxBodySize.
bool EncodeFrame(MsgType type, const std::string& body, std::string& frame);

// DecodeFrame parses a wire frame, validates magic and CRC32C,
// and extracts the message type and body.
//
// Returns true if the frame is valid, false if magic, CRC32C, or
// size constraints are violated.
bool DecodeFrame(const char* data, size_t len, MsgType& type, std::string& body);

// Helper: encode a typed message struct into a full frame.
// The struct must implement Serialize(std::string&).
template <typename Msg>
bool EncodeMessage(const Msg& msg, std::string& frame) {
    std::string body;
    msg.Serialize(body);
    return EncodeFrame(MessageType(msg), body, frame);
}

// Helper: decode a frame and deserialize into a typed message struct.
// The struct must implement Deserialize(const char*, size_t).
template <typename Msg>
bool DecodeMessage(const char* data, size_t len, Msg& msg) {
    MsgType type;
    std::string body;
    if (!DecodeFrame(data, len, type, body))
        return false;
    return msg.Deserialize(body.data(), body.size());
}

// Convenience: compute CRC32C of a frame's protected region using the
// shared CRC32C utility (defined in common/kv_utils.h).
uint32_t FrameCRC32C(const void* data, size_t len);

} // namespace kvstore
} // namespace zero
