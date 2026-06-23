// kv_utils.h — Hash, time, encoding utilities
#pragma once

#include "kvstore/common/kv_types.h"
#include <string>
#include <chrono>

namespace zero {
namespace kvstore {

// ---- hash ----
uint16_t HashSlot(const Key& key);

inline int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

inline int64_t NowUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// ---- varint encoding helpers ----
void EncodeVarint64(uint64_t val, std::string& out);
uint64_t DecodeVarint64(const char*& data, const char* end);

// ---- crc32c (software fallback) ----
uint32_t CRC32C(const void* data, size_t len);
uint32_t CRC32CCombine(uint32_t crc, const void* data, size_t len);

} // namespace kvstore
} // namespace zero
