/**
 * @file    functional.h
 * @brief   Hash functors, comparison operators, and key extraction utilities.
 * @author  lstl team
 * @date    2025
 *
 * Provides the function object types used by lstl containers:
 * - Hash functors:     FNV-1a based hash for integers, pointers, and strings.
 * - Comparison:        less, greater, equal_to, not_equal_to.
 * - Key extraction:    identity, select1st, select2nd (used by associative containers).
 * - String hashing:    cstr_hash and cstr_equal for const char* keys.
 *
 * The hash functors are designed to be fast and well-distributed,
 * using the FNV-1a algorithm (Fowler–Noll–Vo) which has excellent
 * avalanche properties for hash table bucket distribution.
 *
 * @ingroup memory
 */

#pragma once

#include <cstddef>
#include <cstring>
#include <functional>

namespace lstl {

// =========================================================================
// Key extraction functors
// =========================================================================

/**
 * @brief  Identity function object — returns its argument unchanged.
 *
 * Used by set-type containers where the key IS the value.
 *
 * @tparam T  The type to pass through.
 */
template <typename T>
struct identity {
    /**
     * @brief  Returns the argument.
     * @param  x  The value.
     * @return    x (unchanged).
     */
    const T& operator()(const T& x) const { return x; }
};

/**
 * @brief  Extracts the first element of a pair.
 *
 * Used by map-type containers to extract the key from pair<const Key, T>.
 *
 * @tparam Pair  The pair type (typically pair<const Key, T>).
 */
template <typename Pair>
struct select1st {
    /**
     * @brief  Returns the first element.
     * @param  x  The pair.
     * @return    x.first
     */
    const typename Pair::first_type& operator()(const Pair& x) const {
        return x.first;
    }
};

/**
 * @brief  Extracts the second element of a pair.
 *
 * @tparam Pair  The pair type.
 */
template <typename Pair>
struct select2nd {
    /**
     * @brief  Returns the second element.
     * @param  x  The pair.
     * @return    x.second
     */
    const typename Pair::second_type& operator()(const Pair& x) const {
        return x.second;
    }
};

// =========================================================================
// Hash functors
// =========================================================================

/**
 * @brief  Primary hash template — delegates to std::hash.
 *
 * @tparam T  The type to hash.
 */
template <typename T>
struct hash {
    /**
     * @brief  Computes a hash value.
     * @param  x  The value to hash.
     * @return    A size_t hash code.
     */
    size_t operator()(const T& x) const {
        return std::hash<T>()(x);
    }
};

/**
 * @brief  Hash specialization for raw pointers.
 *
 * Hashes the pointer address directly (identity hash).
 *
 * @tparam T  The pointed-to type.
 */
template <typename T>
struct hash<T*> {
    /**
     * @brief  Hashes a pointer.
     * @param  p  The pointer.
     * @return    reinterpret_cast<size_t>(p)
     */
    size_t operator()(T* p) const {
        return reinterpret_cast<size_t>(p);
    }
};

/**
 * @brief  Hash specialization for const pointers.
 * @tparam T  The pointed-to type.
 */
template <typename T>
struct hash<const T*> {
    /**
     * @brief  Hashes a const pointer.
     * @param  p  The pointer.
     * @return    reinterpret_cast<size_t>(p)
     */
    size_t operator()(const T* p) const {
        return reinterpret_cast<size_t>(p);
    }
};

/**
 * @brief  Hash specialization for int.
 *
 * Uses the integer value directly as the hash (identity).
 * For production, consider mixing to improve avalanche for
 * hash table bucket selection.
 */
template <>
struct hash<int> {
    size_t operator()(int x) const { return static_cast<size_t>(x); }
};

/// @brief Hash for unsigned int (identity hash).
template <>
struct hash<unsigned int> {
    size_t operator()(unsigned int x) const { return static_cast<size_t>(x); }
};

/// @brief Hash for long (identity hash).
template <>
struct hash<long> {
    size_t operator()(long x) const { return static_cast<size_t>(x); }
};

/// @brief Hash for unsigned long (identity hash).
template <>
struct hash<unsigned long> {
    size_t operator()(unsigned long x) const { return static_cast<size_t>(x); }
};

/**
 * @brief  Hash for long long — XOR-folds high bits into low bits.
 *
 * For 64-bit integers on a 64-bit size_t platform, this is equivalent
 * to identity. On 32-bit platforms, it mixes the high 32 bits with
 * the low 32 bits for better distribution.
 */
template <>
struct hash<long long> {
    size_t operator()(long long x) const {
        return static_cast<size_t>(x ^ (static_cast<size_t>(x) >> 32));
    }
};

/// @brief  Hash for unsigned long long (same XOR-fold strategy).
template <>
struct hash<unsigned long long> {
    size_t operator()(unsigned long long x) const {
        return static_cast<size_t>(x ^ (x >> 32));
    }
};

// =========================================================================
// FNV-1a string hashing
// =========================================================================

/**
 * @brief  FNV-1a hash for a character buffer.
 *
 * Fowler–Noll–Vo hash algorithm — provides excellent distribution
 * and is simple to implement. Uses the 64-bit FNV parameters:
 * - Offset basis: 14695981039346656037
 * - Prime:        1099511628211
 *
 * @param  s    Pointer to the start of the buffer.
 * @param  len  Number of bytes to hash.
 * @return      64-bit FNV-1a hash (truncated to size_t on 32-bit platforms).
 */
inline size_t hash_string(const char* s, size_t len) {
    size_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<size_t>(static_cast<unsigned char>(s[i]));
        h *= 1099511628211ULL;
    }
    return h;
}

/**
 * @brief  Hash functor for null-terminated C strings (FNV-1a).
 *
 * Usage:
 * @code
 * lstl::unordered_map<const char*, int, lstl::cstr_hash, lstl::cstr_equal> m;
 * @endcode
 */
struct cstr_hash {
    /**
     * @brief  Hashes a C string.
     * @param  s  Null-terminated string (must not be nullptr).
     * @return    FNV-1a hash of the string.
     */
    size_t operator()(const char* s) const {
        size_t h = 14695981039346656037ULL;
        while (*s) {
            h ^= static_cast<size_t>(static_cast<unsigned char>(*s++));
            h *= 1099511628211ULL;
        }
        return h;
    }
};

/**
 * @brief  Equality comparison for C strings.
 *
 * Uses pointer equality as a fast path before falling back to strcmp.
 */
struct cstr_equal {
    /**
     * @brief  Compares two C strings for equality.
     * @param  a  First string (may be nullptr).
     * @param  b  Second string (may be nullptr).
     * @return    true if both are null, or point to the same string,
     *            or contain the same characters.
     */
    bool operator()(const char* a, const char* b) const {
        return a == b || (a && b && std::strcmp(a, b) == 0);
    }
};

// =========================================================================
// Comparison functors
// =========================================================================

/**
 * @brief  Less-than comparison functor.
 *
 * Wraps operator< for use with containers that require
 * a comparison function object (e.g., map, sort).
 *
 * @tparam T  Type to compare.
 */
template <typename T>
struct less {
    /**
     * @brief  Compares two values.
     * @param  a  Left operand.
     * @param  b  Right operand.
     * @return    a < b
     */
    bool operator()(const T& a, const T& b) const { return a < b; }
};

/**
 * @brief  Greater-than comparison functor.
 * @tparam T  Type to compare.
 */
template <typename T>
struct greater {
    /**
     * @brief  Compares two values.
     * @return   a > b
     */
    bool operator()(const T& a, const T& b) const { return a > b; }
};

/**
 * @brief  Equality comparison functor.
 * @tparam T  Type to compare.
 */
template <typename T>
struct equal_to {
    /**
     * @brief  Compares two values for equality.
     * @return   a == b
     */
    bool operator()(const T& a, const T& b) const { return a == b; }
};

/**
 * @brief  Not-equal comparison functor.
 * @tparam T  Type to compare.
 */
template <typename T>
struct not_equal_to {
    /**
     * @brief  Compares two values for inequality.
     * @return   a != b
     */
    bool operator()(const T& a, const T& b) const { return a != b; }
};

} // namespace lstl
