#pragma once

#include <cstdint>
#include <byteswap.h>

namespace zero {

// 编译期检测系统字节序
inline bool IsLittleEndian() {
    static const uint16_t v = 0x0001;
    return *reinterpret_cast<const uint8_t*>(&v) == 0x01;
}

// 字节序转换 (小端 ⇄ 大端)
inline uint8_t  ByteSwap(uint8_t  v) { return v; }
inline int8_t   ByteSwap(int8_t   v) { return v; }
inline uint16_t ByteSwap(uint16_t v) { return bswap_16(v); }
inline int16_t  ByteSwap(int16_t  v) { return static_cast<int16_t>(bswap_16(static_cast<uint16_t>(v))); }
inline uint32_t ByteSwap(uint32_t v) { return bswap_32(v); }
inline int32_t  ByteSwap(int32_t  v) { return static_cast<int32_t>(bswap_32(static_cast<uint32_t>(v))); }
inline uint64_t ByteSwap(uint64_t v) { return bswap_64(v); }
inline int64_t  ByteSwap(int64_t  v) { return static_cast<int64_t>(bswap_64(static_cast<uint64_t>(v))); }

// 网络字节序 = 大端
// Host → Network: 小端机器做 swap, 大端机器直接返回
template<typename T>
T HostToNetwork(T v) {
    if (IsLittleEndian()) return ByteSwap(v);
    return v;
}

template<typename T>
T NetworkToHost(T v) {
    if (IsLittleEndian()) return ByteSwap(v);
    return v;
}

} // namespace zero
