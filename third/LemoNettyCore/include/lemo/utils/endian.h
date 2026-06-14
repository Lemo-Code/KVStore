#pragma once

#include <byteswap.h>
#include <stdint.h>

#include <type_traits>

#define LEMO_LITTLE_ENDIAN 1
#define LEMO_BIG_ENDIAN 2

#if BYTE_ORDER == BIG_ENDIAN
#define LEMO_BYTE_ORDER LEMO_BIG_ENDIAN
#else
#define LEMO_BYTE_ORDER LEMO_LITTLE_ENDIAN
#endif

namespace lemo {
namespace utils {

template <class T>
typename std::enable_if<sizeof(T) == sizeof(uint16_t), T>::type byteswap(
    T value) {
  return static_cast<T>(bswap_16(static_cast<uint16_t>(value)));
}

template <class T>
typename std::enable_if<sizeof(T) == sizeof(uint32_t), T>::type byteswap(
    T value) {
  return static_cast<T>(bswap_32(static_cast<uint32_t>(value)));
}

template <class T>
typename std::enable_if<sizeof(T) == sizeof(uint64_t), T>::type byteswap(
    T value) {
  return static_cast<T>(bswap_64(static_cast<uint64_t>(value)));
}

#if LEMO_BYTE_ORDER == LEMO_BIG_ENDIAN
template <typename T>
T byteswapOnLittleEndian(T value) {
  return value;
}

template <typename T>
T byteswapOnBigEndian(T value) {
  return byteswap(value);
}
#else
template <typename T>
T byteswapOnLittleEndian(T value) {
  return byteswap(value);
}

template <typename T>
T byteswapOnBigEndian(T value) {
  return value;
}
#endif

}  // namespace utils
}  // namespace lemo
