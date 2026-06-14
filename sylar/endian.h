#ifndef __SYLAR_ENDIAN_H__
#define __SYLAR_ENDIAN_H__

// 1. 先包含 C 头文件（在命名空间和 extern "C++" 之外）
#include <byteswap.h>
#include <stdint.h>
#include <type_traits>  // C++ 模板头文件，需在 C++ 环境中包含

// 2. 定义字节序宏（独立于代码块）
#define SYLAR_LITTLE_ENDIAN 1
#define SYLAR_BIG_ENDIAN 2

// 3. 用 extern "C++" 明确标记 C++ 代码范围，避免被 extern "C" 影响
#ifdef __cplusplus
extern "C++" {
#endif

namespace sylar {

    // 4. 根据系统字节序定义宏（放在命名空间内）
    #if BYTE_ORDER == BIG_ENDIAN
        #define SYLAR_BYTE_ORDER SYLAR_BIG_ENDIAN
    #else
        #define SYLAR_BYTE_ORDER SYLAR_LITTLE_ENDIAN
    #endif

    // 5. 实现字节交换模板（C++ 特性，必须在命名空间内）
    // SFINAE 机制：根据类型大小选择对应的字节交换函数
    template<class T>
    typename std::enable_if<sizeof(T) == sizeof(uint16_t), T>::type
    byteswap(T value) {
        return (T)bswap_16((uint16_t)value);
    }

    template<class T>
    typename std::enable_if<sizeof(T) == sizeof(uint32_t), T>::type
    byteswap(T value) {
        return (T)bswap_32((uint32_t)value);
    }

    template<class T>
    typename std::enable_if<sizeof(T) == sizeof(uint64_t), T>::type
    byteswap(T value) {
        return (T)bswap_64((uint64_t)value);
    }

    // 6. 根据主机字节序条件交换的模板
    #if SYLAR_BYTE_ORDER == SYLAR_BIG_ENDIAN  // 大端主机
        template<typename T>
        T byteswapOnLittleEndian(T value) {
            return value;  // 大端主机对小端数据不交换
        }

        template<typename T>
        T byteswapOnBigEndian(T value) {
            return byteswap(value);  // 大端主机对大端数据交换（用于特定场景）
        }
    #else  // 小端主机
        template<typename T>
        T byteswapOnLittleEndian(T value) {
            return byteswap(value);  // 小端主机对小端数据交换（用于特定场景）
        }

        template<typename T>
        T byteswapOnBigEndian(T value) {
            return value;  // 小端主机对大端数据不交换
        }
    #endif

}  // namespace sylar

#ifdef __cplusplus
}  // 关闭 extern "C++"
#endif

#endif  // __SYLAR_ENDIAN_H__