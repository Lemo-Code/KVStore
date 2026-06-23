// zero lexical_cast — fast, allocation-light type conversion
// Uses std::to_chars / std::from_chars for integer<->string conversion,
// and specialization for common numeric/string types.
// All conversion functions are noexcept where possible.
#pragma once

#include <string>
#include <string_view>
#include <charconv>
#include <cstdint>
#include <type_traits>
#include <system_error>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace zero {

// ============================================================
// Integer/Float to String (stack buffer, no heap allocation)
// ============================================================

template <typename T>
std::string lexical_cast_to_string(T val) {
    // Stack buffer: enough for 128-bit int in decimal (39 digits) + sign + null
    char buf[64];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), val);
    if (ec != std::errc()) {
        return {};
    }
    return std::string(buf, ptr - buf);
}

// String to integer using std::from_chars (no heap allocation)
template <typename T>
bool lexical_cast_from_string(std::string_view sv, T& out) noexcept {
    const char* first = sv.data();
    const char* last = first + sv.size();
    auto [ptr, ec] = std::from_chars(first, last, out);
    return ec == std::errc() && ptr == last;
}

// ============================================================
// Generic lexical_cast<Target>(Source)
// ============================================================

namespace detail {

// Detect if type is string-like (decays to handle const char[N] arrays)
template <typename T>
struct is_string_like
    : std::disjunction<std::is_same<std::decay_t<T>, std::string>,
                       std::is_same<std::decay_t<T>, std::string_view>,
                       std::is_same<std::decay_t<T>, const char*>> {};

template <typename T>
inline constexpr bool is_string_like_v = is_string_like<T>::value;

} // namespace detail

// Primary template — uses if/else if chain so GCC discards non-taken branches
template <typename To, typename From>
To lexical_cast(const From& from) {
    // String -> Numeric
    if constexpr (detail::is_string_like_v<From> && std::is_arithmetic_v<To>) {
        To val{};
        std::string_view sv;
        if constexpr (std::is_same_v<From, const char*>) {
            if (!from) {
                throw std::runtime_error("lexical_cast: null const char*");
            }
            sv = std::string_view(from);
        } else {
            sv = std::string_view(from);
        }
        if (!lexical_cast_from_string(sv, val)) {
            throw std::runtime_error(
                "lexical_cast: cannot convert string to numeric: " +
                std::string(sv));
        }
        return val;
    }
    // Numeric -> String
    else if constexpr (std::is_arithmetic_v<From> && detail::is_string_like_v<To>) {
        if constexpr (std::is_same_v<To, const char*>) {
            throw std::runtime_error(
                "lexical_cast: cannot return const char* (dangling)");
        } else {
            return lexical_cast_to_string(from);
        }
    }
    // Bool -> String
    else if constexpr (std::is_same_v<From, bool> && detail::is_string_like_v<To>) {
        if constexpr (std::is_same_v<To, const char*>) {
            throw std::runtime_error(
                "lexical_cast: cannot return const char* (dangling)");
        } else {
            return from ? std::string("true") : std::string("false");
        }
    }
    // String -> Bool
    else if constexpr (detail::is_string_like_v<From> && std::is_same_v<To, bool>) {
        std::string_view sv;
        if constexpr (std::is_same_v<From, const char*>) {
            if (!from) throw std::runtime_error("lexical_cast: null const char*");
            sv = std::string_view(from);
        } else {
            sv = std::string_view(from);
        }
        if (sv == "true" || sv == "1" || sv == "yes" || sv == "on") {
            return true;
        }
        if (sv == "false" || sv == "0" || sv == "no" || sv == "off") {
            return false;
        }
        throw std::runtime_error(
            "lexical_cast: cannot convert string to bool: " + std::string(sv));
    }
    // Numeric -> Numeric
    else if constexpr (std::is_arithmetic_v<From> && std::is_arithmetic_v<To>) {
        return static_cast<To>(from);
    }
    // Default: static_cast
    else {
        return static_cast<To>(from);
    }
}

// ============================================================
// Convenience safe-cast: returns bool, sets out parameter
// ============================================================

template <typename To, typename From>
bool lexical_cast_safe(const From& from, To& out) noexcept {
    try {
        out = lexical_cast<To, From>(from);
        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================
// Specialized charconv-based helpers for int types
// ============================================================

// Integer-to-string, returns false on failure (unlikely)
template <typename T>
bool to_chars_string(T val, std::string& out) noexcept {
    char buf[64];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), val);
    if (ec != std::errc()) return false;
    out.assign(buf, ptr - buf);
    return true;
}

// String-to-integer, returns false on failure
template <typename T>
bool from_chars_string(std::string_view sv, T& out) noexcept {
    return lexical_cast_from_string(sv, out);
}

// ============================================================
// Floating-point to/from string
// ============================================================

inline std::string float_to_string(float val) {
    char buf[64];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), val,
                                    std::chars_format::general, 9);
    if (ec != std::errc()) return {};
    return std::string(buf, ptr - buf);
}

inline std::string double_to_string(double val) {
    char buf[64];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), val,
                                    std::chars_format::general, 17);
    if (ec != std::errc()) return {};
    return std::string(buf, ptr - buf);
}

inline bool string_to_float(std::string_view sv, float& out) noexcept {
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), out);
    return ec == std::errc() && ptr == sv.data() + sv.size();
}

inline bool string_to_double(std::string_view sv, double& out) noexcept {
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), out);
    return ec == std::errc() && ptr == sv.data() + sv.size();
}

} // namespace zero