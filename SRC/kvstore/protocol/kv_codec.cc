// kv_codec.cc — Frame encode/decode with CRC32C
#include "kvstore/protocol/kv_codec.h"
#include "kvstore/common/kv_utils.h"

#include <cstring>
#include <cstdint>
#include <stdexcept>

namespace zero {
namespace kvstore {

namespace {

// Write a little-endian uint32_t to a buffer (output iterator style)
inline void WriteLE32(uint32_t v, char* dst) {
    dst[0] = static_cast<char>(v & 0xFF);
    dst[1] = static_cast<char>((v >> 8) & 0xFF);
    dst[2] = static_cast<char>((v >> 16) & 0xFF);
    dst[3] = static_cast<char>((v >> 24) & 0xFF);
}

inline void WriteLE16(uint16_t v, char* dst) {
    dst[0] = static_cast<char>(v & 0xFF);
    dst[1] = static_cast<char>((v >> 8) & 0xFF);
}

inline uint32_t ReadLE32(const char* p) {
    return static_cast<uint8_t>(p[0]) |
           (static_cast<uint32_t>(static_cast<uint8_t>(p[1])) << 8) |
           (static_cast<uint32_t>(static_cast<uint8_t>(p[2])) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(p[3])) << 24);
}

inline uint16_t ReadLE16(const char* p) {
    return static_cast<uint16_t>(static_cast<uint8_t>(p[0])) |
           (static_cast<uint16_t>(static_cast<uint8_t>(p[1])) << 8);
}

} // anonymous namespace

bool EncodeFrame(MsgType type, const std::string& body, std::string& frame) {
    if (body.size() > kMaxBodySize) {
        return false;
    }

    uint32_t body_len = static_cast<uint32_t>(body.size());

    // Layout: magic(4) + type(2) + body_len(4) + body + crc32c(4)
    // CRC covers: magic(4) + type(2) + body_len(4) + body
    size_t frame_size = kFrameHeaderSize + body.size() + 4; // +4 for CRC32C
    frame.assign(frame_size, '\0');
    char* dst = &frame[0];

    // Write header fields
    WriteLE32(kFrameMagic, dst);                        // byte 0..3
    WriteLE16(static_cast<uint16_t>(type), dst + 4);    // byte 4..5
    WriteLE32(body_len, dst + 6);                       // byte 6..9

    // Copy body
    if (!body.empty()) {
        std::memcpy(dst + 10, body.data(), body.size());
    }

    // Compute CRC32C over: magic(4) + type(2) + body_len(4) + body
    // = first kFrameHeaderSize bytes + body
    uint32_t crc = CRC32C(dst, kFrameHeaderSize + body.size());

    // Write CRC32C at the end
    WriteLE32(crc, dst + kFrameHeaderSize + body.size());

    return true;
}

bool DecodeFrame(const char* data, size_t len, MsgType& type,
                 std::string& body) {
    // Minimum frame: header(10) + CRC32C(4) = 14 bytes
    if (len < kFrameHeaderSize + 4) {
        return false;
    }

    // Check magic
    if (ReadLE32(data) != kFrameMagic) {
        return false;
    }

    // Read type and body_len
    type = static_cast<MsgType>(ReadLE16(data + 4));
    uint32_t body_len = ReadLE32(data + 6);

    // Validate body length doesn't overflow the frame
    if (kFrameHeaderSize + body_len + 4 > len) {
        return false;
    }

    if (body_len > kMaxBodySize) {
        return false;
    }

    // Validate CRC32C over the protected region
    uint32_t expected_crc = ReadLE32(data + kFrameHeaderSize + body_len);
    uint32_t computed_crc = CRC32C(data, kFrameHeaderSize + body_len);

    if (expected_crc != computed_crc) {
        return false;
    }

    // Extract body
    body.assign(data + kFrameHeaderSize, body_len);
    return true;
}

uint32_t FrameCRC32C(const void* data, size_t len) {
    return CRC32C(data, len);
}

} // namespace kvstore
} // namespace zero
