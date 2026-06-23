// zstl string_view — non-owning view over a character sequence
#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <ostream>
#include <functional>
#include <algorithm>
#include <limits>

namespace zstl {

// ============================================================
// string_view_traits<CharT> — default character traits adapter
//
// Wraps std::char_traits<CharT> to provide the full set of
// operations needed by basic_string_view.  Users can supply
// a custom Traits class with the same interface.
// ============================================================
template<typename CharT>
struct string_view_traits {
    using char_type   = CharT;
    using traits_type = std::char_traits<CharT>;
    using size_type   = size_t;

    static constexpr int compare(const CharT* a, const CharT* b, size_type n) noexcept {
        return traits_type::compare(a, b, n);
    }

    static constexpr size_type length(const CharT* s) noexcept {
        return traits_type::length(s);
    }

    static constexpr const CharT* find(const CharT* s, size_type n, const CharT& c) noexcept {
        for (size_type i = 0; i < n; ++i) {
            if (traits_type::eq(s[i], c)) return s + i;
        }
        return nullptr;
    }

    static constexpr bool eq(const CharT& a, const CharT& b) noexcept {
        return traits_type::eq(a, b);
    }

    static constexpr bool lt(const CharT& a, const CharT& b) noexcept {
        return traits_type::lt(a, b);
    }

    static constexpr CharT to_char_type(int c) noexcept {
        return traits_type::to_char_type(c);
    }

    static constexpr int to_int_type(const CharT& c) noexcept {
        return traits_type::to_int_type(c);
    }

    static constexpr bool eq_int_type(int a, int b) noexcept {
        return traits_type::eq_int_type(a, b);
    }

    static constexpr int eof() noexcept {
        return traits_type::eof();
    }
};

// ============================================================
// basic_string_view<CharT, Traits>
//
// A lightweight, non-owning reference to a contiguous sequence
// of characters.  Does not allocate, copy, or free memory.
//
// The view becomes invalid if the referenced string is destroyed
// or mutated.
// ============================================================
template<typename CharT, typename Traits = string_view_traits<CharT>>
class basic_string_view {
public:
    // ---- types ----
    using traits_type     = Traits;
    using value_type      = CharT;
    using pointer         = CharT*;
    using const_pointer   = const CharT*;
    using reference       = CharT&;
    using const_reference = const CharT&;
    using const_iterator  = const CharT*;
    using iterator        = const CharT*;  // const-only iterators
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using size_type              = size_t;
    using difference_type        = ptrdiff_t;

    // Sentinel constant (returned by find operations on failure)
    static constexpr size_type npos = static_cast<size_type>(-1);

private:
    const_pointer data_ = nullptr;
    size_type     size_ = 0;

public:
    // ============================================================
    // Constructors
    // ============================================================

    // Default constructor — empty view
    constexpr basic_string_view() noexcept = default;

    // Construct from null-terminated string
    constexpr basic_string_view(const CharT* s) noexcept
        : data_(s), size_(Traits::length(s)) {}

    // Construct from pointer + length
    constexpr basic_string_view(const CharT* s, size_type count)
        : data_(s), size_(count) {}

    // Construct from std::basic_string (implicit)
    basic_string_view(const std::basic_string<CharT>& str) noexcept
        : data_(str.data()), size_(str.size()) {}

    // Copy constructor
    constexpr basic_string_view(const basic_string_view&) noexcept = default;

    // Assignment
    constexpr basic_string_view& operator=(const basic_string_view&) noexcept = default;

    // Destructor (trivial)
    ~basic_string_view() = default;

    // ============================================================
    // Iterators
    // ============================================================

    constexpr const_iterator begin() const noexcept { return data_; }
    constexpr const_iterator cbegin() const noexcept { return data_; }

    constexpr const_iterator end() const noexcept { return data_ + size_; }
    constexpr const_iterator cend() const noexcept { return data_ + size_; }

    constexpr const_reverse_iterator rbegin() const noexcept {
        return const_reverse_iterator(end());
    }

    constexpr const_reverse_iterator crbegin() const noexcept {
        return const_reverse_iterator(end());
    }

    constexpr const_reverse_iterator rend() const noexcept {
        return const_reverse_iterator(begin());
    }

    constexpr const_reverse_iterator crend() const noexcept {
        return const_reverse_iterator(begin());
    }

    // ============================================================
    // Element access
    // ============================================================

    constexpr const_reference operator[](size_type pos) const noexcept {
        return data_[pos];
    }

    // Checked element access — throws std::out_of_range
    const_reference at(size_type pos) const {
        if (pos >= size_) {
            throw std::out_of_range("zstl::basic_string_view::at: position out of range");
        }
        return data_[pos];
    }

    constexpr const_reference front() const noexcept { return data_[0]; }
    constexpr const_reference back() const noexcept { return data_[size_ - 1]; }
    constexpr const_pointer data() const noexcept { return data_; }

    // ============================================================
    // Capacity
    // ============================================================

    constexpr size_type size() const noexcept { return size_; }
    constexpr size_type length() const noexcept { return size_; }
    constexpr size_type max_size() const noexcept {
        return std::numeric_limits<size_type>::max() / sizeof(value_type);
    }
    constexpr bool empty() const noexcept { return size_ == 0; }

    // ============================================================
    // Modifiers
    // ============================================================

    // Shrink the view by removing count characters from the front
    constexpr void remove_prefix(size_type n) noexcept {
        data_ += n;
        size_ -= n;
    }

    // Shrink the view by removing count characters from the back
    constexpr void remove_suffix(size_type n) noexcept {
        size_ -= n;
    }

    // Swap two views
    constexpr void swap(basic_string_view& other) noexcept {
        const_pointer tmp_data = data_;
        size_type     tmp_size = size_;
        data_ = other.data_;
        size_ = other.size_;
        other.data_ = tmp_data;
        other.size_ = tmp_size;
    }

    // ============================================================
    // Operations
    // ============================================================

    // Copy size() characters to dest (not null-terminated)
    size_type copy(CharT* dest, size_type count, size_type pos = 0) const {
        if (pos > size_) {
            throw std::out_of_range("zstl::basic_string_view::copy: position out of range");
        }
        size_type rlen = (std::min)(count, size_ - pos);
        Traits::copy(dest, data_ + pos, rlen);
        return rlen;
    }

    // Return a substring [pos, pos + count)
    constexpr basic_string_view substr(size_type pos = 0, size_type count = npos) const {
        if (pos > size_) {
            throw std::out_of_range("zstl::basic_string_view::substr: position out of range");
        }
        size_type rlen = (std::min)(count, size_ - pos);
        return basic_string_view(data_ + pos, rlen);
    }

    // ============================================================
    // compare — lexicographical comparison
    // ============================================================

    constexpr int compare(basic_string_view other) const noexcept {
        size_type rlen = (std::min)(size_, other.size_);
        int result = Traits::compare(data_, other.data_, rlen);
        if (result != 0) return result;
        if (size_ < other.size_) return -1;
        if (size_ > other.size_) return 1;
        return 0;
    }

    constexpr int compare(size_type pos1, size_type count1,
                          basic_string_view v) const {
        return substr(pos1, count1).compare(v);
    }

    constexpr int compare(size_type pos1, size_type count1,
                          basic_string_view v,
                          size_type pos2, size_type count2) const {
        return substr(pos1, count1).compare(v.substr(pos2, count2));
    }

    constexpr int compare(const CharT* s) const {
        return compare(basic_string_view(s));
    }

    constexpr int compare(size_type pos1, size_type count1,
                          const CharT* s) const {
        return substr(pos1, count1).compare(basic_string_view(s));
    }

    constexpr int compare(size_type pos1, size_type count1,
                          const CharT* s, size_type count2) const {
        return substr(pos1, count1).compare(basic_string_view(s, count2));
    }

    // ============================================================
    // starts_with / ends_with (C++20 style)
    // ============================================================

    constexpr bool starts_with(basic_string_view x) const noexcept {
        return size_ >= x.size_ &&
               Traits::compare(data_, x.data_, x.size_) == 0;
    }

    constexpr bool starts_with(CharT x) const noexcept {
        return !empty() && Traits::eq(front(), x);
    }

    constexpr bool starts_with(const CharT* x) const {
        return starts_with(basic_string_view(x));
    }

    constexpr bool ends_with(basic_string_view x) const noexcept {
        return size_ >= x.size_ &&
               Traits::compare(data_ + size_ - x.size_, x.data_, x.size_) == 0;
    }

    constexpr bool ends_with(CharT x) const noexcept {
        return !empty() && Traits::eq(back(), x);
    }

    constexpr bool ends_with(const CharT* x) const {
        return ends_with(basic_string_view(x));
    }

    // ============================================================
    // contains (C++23 style)
    // ============================================================

    constexpr bool contains(basic_string_view x) const noexcept {
        return find(x) != npos;
    }

    constexpr bool contains(CharT x) const noexcept {
        return find(x) != npos;
    }

    constexpr bool contains(const CharT* x) const noexcept {
        return find(x) != npos;
    }

    // ============================================================
    // find — search for the first occurrence
    // ============================================================

    constexpr size_type find(basic_string_view v, size_type pos = 0) const noexcept {
        if (pos > size_ || v.size_ > size_ - pos) return npos;
        if (v.empty()) return pos;
        const_pointer result = find_sub(data_ + pos, size_ - pos, v.data_, v.size_);
        return result ? static_cast<size_type>(result - data_) : npos;
    }

    constexpr size_type find(CharT c, size_type pos = 0) const noexcept {
        if (pos >= size_) return npos;
        const_pointer result = Traits::find(data_ + pos, size_ - pos, c);
        return result ? static_cast<size_type>(result - data_) : npos;
    }

    constexpr size_type find(const CharT* s, size_type pos, size_type count) const {
        return find(basic_string_view(s, count), pos);
    }

    constexpr size_type find(const CharT* s, size_type pos = 0) const {
        return find(basic_string_view(s), pos);
    }

    // ============================================================
    // rfind — search for the last occurrence
    // ============================================================

    constexpr size_type rfind(basic_string_view v, size_type pos = npos) const noexcept {
        if (size_ < v.size_) return npos;
        size_type start = (std::min)(size_ - v.size_, pos);
        if (v.empty()) return start;
        for (size_type i = start; ; --i) {
            if (Traits::compare(data_ + i, v.data_, v.size_) == 0)
                return i;
            if (i == 0) break;
        }
        return npos;
    }

    constexpr size_type rfind(CharT c, size_type pos = npos) const noexcept {
        if (empty()) return npos;
        size_type start = (std::min)(size_ - 1, pos);
        for (size_type i = start; ; --i) {
            if (Traits::eq(data_[i], c)) return i;
            if (i == 0) break;
        }
        return npos;
    }

    constexpr size_type rfind(const CharT* s, size_type pos, size_type count) const {
        return rfind(basic_string_view(s, count), pos);
    }

    constexpr size_type rfind(const CharT* s, size_type pos = npos) const {
        return rfind(basic_string_view(s), pos);
    }

    // ============================================================
    // find_first_of — first occurrence of any character in v
    // ============================================================

    constexpr size_type find_first_of(basic_string_view v, size_type pos = 0) const noexcept {
        for (size_type i = pos; i < size_; ++i) {
            if (Traits::find(v.data_, v.size_, data_[i]))
                return i;
        }
        return npos;
    }

    constexpr size_type find_first_of(CharT c, size_type pos = 0) const noexcept {
        return find(c, pos);
    }

    constexpr size_type find_first_of(const CharT* s, size_type pos, size_type count) const {
        return find_first_of(basic_string_view(s, count), pos);
    }

    constexpr size_type find_first_of(const CharT* s, size_type pos = 0) const {
        return find_first_of(basic_string_view(s), pos);
    }

    // ============================================================
    // find_last_of — last occurrence of any character in v
    // ============================================================

    constexpr size_type find_last_of(basic_string_view v, size_type pos = npos) const noexcept {
        if (empty()) return npos;
        size_type start = (std::min)(size_ - 1, pos);
        for (size_type i = start; ; --i) {
            if (Traits::find(v.data_, v.size_, data_[i]))
                return i;
            if (i == 0) break;
        }
        return npos;
    }

    constexpr size_type find_last_of(CharT c, size_type pos = npos) const noexcept {
        return rfind(c, pos);
    }

    constexpr size_type find_last_of(const CharT* s, size_type pos, size_type count) const {
        return find_last_of(basic_string_view(s, count), pos);
    }

    constexpr size_type find_last_of(const CharT* s, size_type pos = npos) const {
        return find_last_of(basic_string_view(s), pos);
    }

    // ============================================================
    // find_first_not_of — first occurrence of any character NOT in v
    // ============================================================

    constexpr size_type find_first_not_of(basic_string_view v, size_type pos = 0) const noexcept {
        for (size_type i = pos; i < size_; ++i) {
            if (!Traits::find(v.data_, v.size_, data_[i]))
                return i;
        }
        return npos;
    }

    constexpr size_type find_first_not_of(CharT c, size_type pos = 0) const noexcept {
        for (size_type i = pos; i < size_; ++i) {
            if (!Traits::eq(data_[i], c)) return i;
        }
        return npos;
    }

    constexpr size_type find_first_not_of(const CharT* s, size_type pos, size_type count) const {
        return find_first_not_of(basic_string_view(s, count), pos);
    }

    constexpr size_type find_first_not_of(const CharT* s, size_type pos = 0) const {
        return find_first_not_of(basic_string_view(s), pos);
    }

    // ============================================================
    // find_last_not_of — last occurrence of any character NOT in v
    // ============================================================

    constexpr size_type find_last_not_of(basic_string_view v, size_type pos = npos) const noexcept {
        if (empty()) return npos;
        size_type start = (std::min)(size_ - 1, pos);
        for (size_type i = start; ; --i) {
            if (!Traits::find(v.data_, v.size_, data_[i]))
                return i;
            if (i == 0) break;
        }
        return npos;
    }

    constexpr size_type find_last_not_of(CharT c, size_type pos = npos) const noexcept {
        if (empty()) return npos;
        size_type start = (std::min)(size_ - 1, pos);
        for (size_type i = start; ; --i) {
            if (!Traits::eq(data_[i], c)) return i;
            if (i == 0) break;
        }
        return npos;
    }

    constexpr size_type find_last_not_of(const CharT* s, size_type pos, size_type count) const {
        return find_last_not_of(basic_string_view(s, count), pos);
    }

    constexpr size_type find_last_not_of(const CharT* s, size_type pos = npos) const {
        return find_last_not_of(basic_string_view(s), pos);
    }

private:
    // Boyer-Moore-Horspool-like search or naive for small patterns:
    // Use naive search with early exit for clarity and reasonable perf.
    static constexpr const_pointer find_sub(const_pointer haystack, size_type hlen,
                                             const_pointer needle, size_type nlen) noexcept {
        if (nlen == 0) return haystack;
        if (nlen > hlen) return nullptr;
        const_pointer end = haystack + (hlen - nlen) + 1;
        for (const_pointer p = haystack; p < end; ++p) {
            if (Traits::compare(p, needle, nlen) == 0)
                return p;
        }
        return nullptr;
    }
};

// ============================================================
// Comparison operators (non-member)
// ============================================================

template<typename CharT, typename Traits>
constexpr bool operator==(basic_string_view<CharT, Traits> a,
                           basic_string_view<CharT, Traits> b) noexcept {
    return a.compare(b) == 0;
}

template<typename CharT, typename Traits>
constexpr bool operator!=(basic_string_view<CharT, Traits> a,
                           basic_string_view<CharT, Traits> b) noexcept {
    return a.compare(b) != 0;
}

template<typename CharT, typename Traits>
constexpr bool operator<(basic_string_view<CharT, Traits> a,
                          basic_string_view<CharT, Traits> b) noexcept {
    return a.compare(b) < 0;
}

template<typename CharT, typename Traits>
constexpr bool operator>(basic_string_view<CharT, Traits> a,
                          basic_string_view<CharT, Traits> b) noexcept {
    return a.compare(b) > 0;
}

template<typename CharT, typename Traits>
constexpr bool operator<=(basic_string_view<CharT, Traits> a,
                           basic_string_view<CharT, Traits> b) noexcept {
    return a.compare(b) <= 0;
}

template<typename CharT, typename Traits>
constexpr bool operator>=(basic_string_view<CharT, Traits> a,
                           basic_string_view<CharT, Traits> b) noexcept {
    return a.compare(b) >= 0;
}

// ============================================================
// ostream operator<<
// ============================================================

template<typename CharT, typename Traits>
std::basic_ostream<CharT>& operator<<(std::basic_ostream<CharT>& os,
                                       basic_string_view<CharT, Traits> sv) {
    // Write characters directly; avoid forming a temporary string
    for (auto c : sv) {
        os.put(c);
    }
    return os;
}

// ============================================================
// Swap
// ============================================================

template<typename CharT, typename Traits>
constexpr void swap(basic_string_view<CharT, Traits>& a,
                    basic_string_view<CharT, Traits>& b) noexcept {
    a.swap(b);
}

// ============================================================
// Convenience typedefs
// ============================================================

using string_view    = basic_string_view<char>;
using wstring_view   = basic_string_view<wchar_t>;
#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L
using u8string_view  = basic_string_view<char8_t>;
#endif
using u16string_view = basic_string_view<char16_t>;
using u32string_view = basic_string_view<char32_t>;

// ============================================================
// Literal operator ""_sv (C++17)
// ============================================================

inline namespace literals {
inline namespace string_view_literals {

constexpr string_view operator""_sv(const char* str, size_t len) noexcept {
    return string_view(str, len);
}

constexpr wstring_view operator""_sv(const wchar_t* str, size_t len) noexcept {
    return wstring_view(str, len);
}

#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L
constexpr u8string_view operator""_sv(const char8_t* str, size_t len) noexcept {
    return u8string_view(str, len);
}
#endif

constexpr u16string_view operator""_sv(const char16_t* str, size_t len) noexcept {
    return u16string_view(str, len);
}

constexpr u32string_view operator""_sv(const char32_t* str, size_t len) noexcept {
    return u32string_view(str, len);
}

} // namespace string_view_literals
} // namespace literals

} // namespace zstl

// ============================================================
// std::hash specialization for zstl::basic_string_view
// ============================================================

namespace std {

template<typename CharT, typename Traits>
struct hash<zstl::basic_string_view<CharT, Traits>> {
    size_t operator()(const zstl::basic_string_view<CharT, Traits>& sv) const noexcept {
        // FNV-1a hash for good distribution
        size_t h = 14695981039346656037ULL;
        for (size_t i = 0; i < sv.size(); ++i) {
            h ^= static_cast<size_t>(sv[i]);
            h *= 1099511628211ULL;
        }
        return h;
    }
};

} // namespace std
