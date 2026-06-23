// zero Endian conversion utilities
// Provides byte-order detection, byte swapping, host<->network conversion,
// and efficient load/store of little-endian and big-endian integers.
//
// All functions are constexpr-friendly and noexcept.
// Uses compiler builtins (__builtin_bswap) for maximum performance.
#pragma once

#include <cstdint>
#include <cstring>

namespace zero {

// ============================================================
// Compile-time endianness detection
// ============================================================

constexpr bool is_little_endian() noexcept {
    return (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__);
}

constexpr bool is_big_endian() noexcept {
    return (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__);
}

// ============================================================
// Raw byte-swap operations (always swap regardless of host order)
// ============================================================

inline uint16_t byteswap16(uint16_t x) noexcept {
    return __builtin_bswap16(x);
}

inline uint32_t byteswap32(uint32_t x) noexcept {
    return __builtin_bswap32(x);
}

inline uint64_t byteswap64(uint64_t x) noexcept {
    return __builtin_bswap64(x);
}

// ============================================================
// Host-to-network / Network-to-host (identity on big-endian)
// ============================================================

inline uint16_t hton16(uint16_t host_val) noexcept {
    if constexpr (is_little_endian()) {
        return byteswap16(host_val);
    }
    return host_val;
}

inline uint32_t hton32(uint32_t host_val) noexcept {
    if constexpr (is_little_endian()) {
        return byteswap32(host_val);
    }
    return host_val;
}

inline uint64_t hton64(uint64_t host_val) noexcept {
    if constexpr (is_little_endian()) {
        return byteswap64(host_val);
    }
    return host_val;
}

inline uint16_t ntoh16(uint16_t net_val) noexcept {
    return hton16(net_val);  // Symmetric
}

inline uint32_t ntoh32(uint32_t net_val) noexcept {
    return hton32(net_val);
}

inline uint64_t ntoh64(uint64_t net_val) noexcept {
    return hton64(net_val);
}

// Legacy aliases
inline uint16_t htons(uint16_t x) noexcept { return hton16(x); }
inline uint32_t htonl(uint32_t x) noexcept { return hton32(x); }
inline uint16_t ntohs(uint16_t x) noexcept { return ntoh16(x); }
inline uint32_t ntohl(uint32_t x) noexcept { return ntoh32(x); }

// Generic template version (selects correct size at compile time)
template <typename T>
T hton(T val) noexcept {
    static_assert(sizeof(T) == 1 || sizeof(T) == 2 ||
                   sizeof(T) == 4 || sizeof(T) == 8,
                  "hton only supports 1/2/4/8 byte types");
    if constexpr (is_little_endian()) {
        if constexpr (sizeof(T) == 1) return val;
        if constexpr (sizeof(T) == 2) return static_cast<T>(byteswap16(static_cast<uint16_t>(val)));
        if constexpr (sizeof(T) == 4) return static_cast<T>(byteswap32(static_cast<uint32_t>(val)));
        if constexpr (sizeof(T) == 8) return static_cast<T>(byteswap64(static_cast<uint64_t>(val)));
    }
    return val;
}

template <typename T>
T ntoh(T val) noexcept {
    return hton(val);  // Symmetric
}

// ============================================================
// Little-endian load / store (safe for unaligned access)
// ============================================================

inline uint16_t load_le16(const void* src) noexcept {
    uint16_t val;
    std::memcpy(&val, src, sizeof(val));
    if constexpr (is_big_endian()) {
        return byteswap16(val);
    }
    return val;
}

inline uint32_t load_le32(const void* src) noexcept {
    uint32_t val;
    std::memcpy(&val, src, sizeof(val));
    if constexpr (is_big_endian()) {
        return byteswap32(val);
    }
    return val;
}

inline uint64_t load_le64(const void* src) noexcept {
    uint64_t val;
    std::memcpy(&val, src, sizeof(val));
    if constexpr (is_big_endian()) {
        return byteswap64(val);
    }
    return val;
}

inline void store_le16(void* dst, uint16_t val) noexcept {
    if constexpr (is_big_endian()) {
        val = byteswap16(val);
    }
    std::memcpy(dst, &val, sizeof(val));
}

inline void store_le32(void* dst, uint32_t val) noexcept {
    if constexpr (is_big_endian()) {
        val = byteswap32(val);
    }
    std::memcpy(dst, &val, sizeof(val));
}

inline void store_le64(void* dst, uint64_t val) noexcept {
    if constexpr (is_big_endian()) {
        val = byteswap64(val);
    }
    std::memcpy(dst, &val, sizeof(val));
}

// ============================================================
// Big-endian load / store (safe for unaligned access)
// ============================================================

inline uint16_t load_be16(const void* src) noexcept {
    uint16_t val;
    std::memcpy(&val, src, sizeof(val));
    if constexpr (is_little_endian()) {
        return byteswap16(val);
    }
    return val;
}

inline uint32_t load_be32(const void* src) noexcept {
    uint32_t val;
    std::memcpy(&val, src, sizeof(val));
    if constexpr (is_little_endian()) {
        return byteswap32(val);
    }
    return val;
}

inline uint64_t load_be64(const void* src) noexcept {
    uint64_t val;
    std::memcpy(&val, src, sizeof(val));
    if constexpr (is_little_endian()) {
        return byteswap64(val);
    }
    return val;
}

inline void store_be16(void* dst, uint16_t val) noexcept {
    if constexpr (is_little_endian()) {
        val = byteswap16(val);
    }
    std::memcpy(dst, &val, sizeof(val));
}

inline void store_be32(void* dst, uint32_t val) noexcept {
    if constexpr (is_little_endian()) {
        val = byteswap32(val);
    }
    std::memcpy(dst, &val, sizeof(val));
}

inline void store_be64(void* dst, uint64_t val) noexcept {
    if constexpr (is_little_endian()) {
        val = byteswap64(val);
    }
    std::memcpy(dst, &val, sizeof(val));
}

// ============================================================
// Signed integer load/store (convert to unsigned, swap, convert back)
// ============================================================

inline int16_t load_le_i16(const void* src) noexcept {
    return static_cast<int16_t>(load_le16(src));
}

inline int32_t load_le_i32(const void* src) noexcept {
    return static_cast<int32_t>(load_le32(src));
}

inline int64_t load_le_i64(const void* src) noexcept {
    return static_cast<int64_t>(load_le64(src));
}

inline int16_t load_be_i16(const void* src) noexcept {
    return static_cast<int16_t>(load_be16(src));
}

inline int32_t load_be_i32(const void* src) noexcept {
    return static_cast<int32_t>(load_be32(src));
}

inline int64_t load_be_i64(const void* src) noexcept {
    return static_cast<int64_t>(load_be64(src));
}

inline void store_le_i16(void* dst, int16_t val) noexcept {
    store_le16(dst, static_cast<uint16_t>(val));
}

inline void store_le_i32(void* dst, int32_t val) noexcept {
    store_le32(dst, static_cast<uint32_t>(val));
}

inline void store_le_i64(void* dst, int64_t val) noexcept {
    store_le64(dst, static_cast<uint64_t>(val));
}

inline void store_be_i16(void* dst, int16_t val) noexcept {
    store_be16(dst, static_cast<uint16_t>(val));
}

inline void store_be_i32(void* dst, int32_t val) noexcept {
    store_be32(dst, static_cast<uint32_t>(val));
}

inline void store_be_i64(void* dst, int64_t val) noexcept {
    store_be64(dst, static_cast<uint64_t>(val));
}

} // namespace zero
