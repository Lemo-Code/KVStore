// zstl basic_string — custom string with Small String Optimization (SSO)
//
// SSO: strings up to 15 chars (+ null terminator = 16 bytes) are stored
// directly on the stack (no heap allocation). Larger strings use the pool
// allocator (zstl::default_alloc).
//
// Layout (64-bit):
//   - Union: { char local[16] } / { char* ptr; size_t cap; size_t len; }
//   - 16 bytes on stack = 15 chars + '\0'
//
// Complexity:
//   - c_str(), data(), size(), length(), empty(): O(1)
//   - find, rfind, compare, starts_with, ends_with: O(n)
//   - append, insert, erase, replace: O(n)
//   - substr: O(n)
//   - reserve: O(n) if reallocation needed
//
// Typedefs:
//   - zstl::string  = basic_string<char>
//   - zstl::wstring = basic_string<wchar_t>
#pragma once

#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <stdexcept>
#include <initializer_list>
#include "zstl/memory/allocator.h"
#include "zstl/memory/utility.h"

namespace zstl {

template<typename CharT, typename Alloc = default_alloc<CharT>>
class basic_string {
public:
    // ---- types ----
    using value_type      = CharT;
    using allocator_type  = Alloc;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;
    using reference       = CharT&;
    using const_reference = const CharT&;
    using pointer         = CharT*;
    using const_pointer   = const CharT*;
    using iterator        = CharT*;
    using const_iterator  = const CharT*;
    using reverse_iterator       = CharT*;  // simplified
    using const_reverse_iterator = const CharT*;

    static constexpr size_type kSSOCapacity = 15;  // chars before heap
    static constexpr size_type npos = static_cast<size_type>(-1);

private:
    // SSO layout: 16 bytes on stack
    // local_[0..14] = characters, local_[15] = size (0-15 for SSO mode)
    // When size > 15, we use heap allocation.
    union {
        struct {
            pointer data_;
            size_type cap_;
            size_type size_;
        } heap_;
        CharT local_[16];
    };

    bool is_local() const noexcept {
        // In SSO mode, local_[15] stores the size (0-15)
        // In heap mode, grow_to() sets local_[15] = kSSOCapacity + 1
        return static_cast<unsigned char>(local_[15]) <= kSSOCapacity;
    }

    [[no_unique_address]] allocator_type alloc_;

    // ---- helpers ----
    pointer data_ptr() noexcept {
        return is_local() ? local_ : heap_.data_;
    }

    const_pointer data_ptr() const noexcept {
        return const_cast<basic_string*>(this)->data_ptr();
    }

    size_type heap_cap() const noexcept {
        return is_local() ? 0 : heap_.cap_;
    }

    size_type local_size() const noexcept {
        return static_cast<size_type>(static_cast<unsigned char>(local_[15]));
    }

    void set_local_size(size_type n) noexcept {
        local_[15] = static_cast<CharT>(n & 0xFF);
    }

    size_type get_size() const noexcept {
        return is_local() ? local_size() : heap_.size_;
    }

    void set_size(size_type n) noexcept {
        if (is_local()) {
            set_local_size(n);
        } else {
            heap_.size_ = n;
        }
    }

    // Allocate heap storage
    void heap_allocate(size_type cap) {
        pointer p = alloc_.allocate(cap + 1);  // +1 for null terminator
        heap_.data_ = p;
        heap_.cap_ = cap;
        heap_.size_ = 0;
        p[0] = CharT(0);
    }

    void heap_deallocate() noexcept {
        if (heap_.data_) {
            alloc_.deallocate(heap_.data_, heap_.cap_ + 1);
            heap_.data_ = nullptr;
            heap_.cap_ = 0;
            heap_.size_ = 0;
        }
    }

    void grow_to(size_type new_cap) {
        size_type old_size = get_size();
        pointer old_data = data_ptr();
        bool was_local = is_local();

        size_type grow = old_size ? old_size : 4;
        while (grow < new_cap) {
            grow = grow + grow / 2;  // 1.5x growth
        }

        pointer new_data = alloc_.allocate(grow + 1);
        // Copy old data
        if (old_size > 0) {
            __builtin_memmove(new_data, old_data, old_size * sizeof(CharT));
        }
        new_data[old_size] = CharT(0);

        if (!was_local) {
            alloc_.deallocate(old_data, heap_.cap_ + 1);
        }

        heap_.data_ = new_data;
        heap_.cap_ = grow;
        heap_.size_ = old_size;
        // Mark as heap mode
        local_[15] = CharT(kSSOCapacity + 1);  // > kSSOCapacity signals heap
    }

    void ensure_capacity(size_type n) {
        if (is_local()) {
            if (n > kSSOCapacity) {
                grow_to(n);
            }
        } else {
            if (n > heap_.cap_) {
                grow_to(n);
            }
        }
    }

public:
    // ============================================================
    // Constructors
    // ============================================================
    basic_string() noexcept {
        local_[0] = CharT(0);
        set_local_size(0);
    }

    basic_string(size_type count, CharT ch) {
        ensure_capacity(count);
        pointer p = data_ptr();
        for (size_type i = 0; i < count; ++i) {
            p[i] = ch;
        }
        p[count] = CharT(0);
        set_size(count);
    }

    basic_string(const CharT* s) {
        size_type len = 0;
        while (s[len]) ++len;
        ensure_capacity(len);
        pointer p = data_ptr();
        __builtin_memmove(p, s, len * sizeof(CharT));
        p[len] = CharT(0);
        set_size(len);
    }

    basic_string(const CharT* s, size_type count) {
        ensure_capacity(count);
        pointer p = data_ptr();
        __builtin_memmove(p, s, count * sizeof(CharT));
        p[count] = CharT(0);
        set_size(count);
    }

    basic_string(const basic_string& other) {
        size_type len = other.get_size();
        ensure_capacity(len);
        pointer p = data_ptr();
        if (len > 0) {
            __builtin_memmove(p, other.data_ptr(), len * sizeof(CharT));
        }
        p[len] = CharT(0);
        set_size(len);
    }

    basic_string(basic_string&& other) noexcept {
        if (other.is_local()) {
            __builtin_memmove(local_, other.local_, sizeof(local_));
            other.local_[0] = CharT(0);
            other.set_local_size(0);
        } else {
            heap_.data_ = other.heap_.data_;
            heap_.cap_ = other.heap_.cap_;
            heap_.size_ = other.heap_.size_;
            local_[15] = CharT(kSSOCapacity + 1);
            other.local_[0] = CharT(0);
            other.set_local_size(0);
        }
    }

    basic_string(std::initializer_list<CharT> il) {
        size_type len = il.size();
        ensure_capacity(len);
        pointer p = data_ptr();
        size_type i = 0;
        for (auto ch : il) {
            p[i++] = ch;
        }
        p[len] = CharT(0);
        set_size(len);
    }

    template<typename InputIterator,
             typename = decltype(*std::declval<InputIterator&>())>
    basic_string(InputIterator first, InputIterator last) {
        for (; first != last; ++first) {
            push_back(*first);
        }
    }

    ~basic_string() {
        if (!is_local()) {
            heap_deallocate();
        }
    }

    // ============================================================
    // Assignment
    // ============================================================
    basic_string& operator=(const basic_string& other) {
        if (this != &other) {
            basic_string tmp(other);
            swap(tmp);
        }
        return *this;
    }

    basic_string& operator=(basic_string&& other) noexcept {
        if (this != &other) {
            if (!is_local()) heap_deallocate();
            if (other.is_local()) {
                __builtin_memmove(local_, other.local_, sizeof(local_));
                other.local_[0] = CharT(0);
                other.set_local_size(0);
            } else {
                heap_.data_ = other.heap_.data_;
                heap_.cap_ = other.heap_.cap_;
                heap_.size_ = other.heap_.size_;
                local_[15] = CharT(kSSOCapacity + 1);
                other.local_[0] = CharT(0);
                other.set_local_size(0);
            }
        }
        return *this;
    }

    basic_string& operator=(const CharT* s) {
        assign(s);
        return *this;
    }

    basic_string& operator=(CharT ch) {
        assign(1, ch);
        return *this;
    }

    basic_string& operator=(std::initializer_list<CharT> il) {
        assign(il);
        return *this;
    }

    // ============================================================
    // assign
    // ============================================================
    basic_string& assign(size_type count, CharT ch) {
        clear();
        ensure_capacity(count);
        pointer p = data_ptr();
        for (size_type i = 0; i < count; ++i) p[i] = ch;
        p[count] = CharT(0);
        set_size(count);
        return *this;
    }

    basic_string& assign(const basic_string& str) {
        return operator=(str);
    }

    basic_string& assign(const basic_string& str, size_type pos, size_type count = npos) {
        size_type len = str.get_size();
        if (pos > len) throw std::out_of_range("basic_string::assign: pos out of range");
        size_type rlen = count < len - pos ? count : len - pos;
        return assign(str.data_ptr() + pos, rlen);
    }

    basic_string& assign(const CharT* s, size_type count) {
        clear();
        ensure_capacity(count);
        pointer p = data_ptr();
        __builtin_memmove(p, s, count * sizeof(CharT));
        p[count] = CharT(0);
        set_size(count);
        return *this;
    }

    basic_string& assign(const CharT* s) {
        return assign(s, zstl_strlen(s));
    }

    template<typename InputIterator,
             typename = decltype(*std::declval<InputIterator&>())>
    basic_string& assign(InputIterator first, InputIterator last) {
        clear();
        for (; first != last; ++first) {
            push_back(*first);
        }
        return *this;
    }

    basic_string& assign(std::initializer_list<CharT> il) {
        return assign(il.begin(), il.size());
    }

    // ============================================================
    // Element access
    // ============================================================
    reference operator[](size_type i) noexcept {
        return data_ptr()[i];
    }

    const_reference operator[](size_type i) const noexcept {
        return data_ptr()[i];
    }

    reference at(size_type i) {
        if (i >= get_size()) throw std::out_of_range("basic_string::at: index out of range");
        return data_ptr()[i];
    }

    const_reference at(size_type i) const {
        if (i >= get_size()) throw std::out_of_range("basic_string::at: index out of range");
        return data_ptr()[i];
    }

    reference front() noexcept { return data_ptr()[0]; }
    const_reference front() const noexcept { return data_ptr()[0]; }
    reference back() noexcept { return data_ptr()[get_size() - 1]; }
    const_reference back() const noexcept { return data_ptr()[get_size() - 1]; }

    // ============================================================
    // C string access
    // ============================================================
    const CharT* c_str() const noexcept { return data_ptr(); }
    const CharT* data() const noexcept { return data_ptr(); }
    CharT* data() noexcept { return data_ptr(); }

    // ============================================================
    // Iterators
    // ============================================================
    iterator begin() noexcept { return data_ptr(); }
    const_iterator begin() const noexcept { return data_ptr(); }
    const_iterator cbegin() const noexcept { return data_ptr(); }
    iterator end() noexcept { return data_ptr() + get_size(); }
    const_iterator end() const noexcept { return data_ptr() + get_size(); }
    const_iterator cend() const noexcept { return data_ptr() + get_size(); }

    // ============================================================
    // Capacity
    // ============================================================
    size_type size() const noexcept { return get_size(); }
    size_type length() const noexcept { return get_size(); }
    size_type max_size() const noexcept { return static_cast<size_type>(-1) / 2; }
    size_type capacity() const noexcept {
        return is_local() ? kSSOCapacity : heap_.cap_;
    }
    bool empty() const noexcept { return get_size() == 0; }

    void reserve(size_type new_cap) {
        if (new_cap > capacity()) {
            ensure_capacity(new_cap);
        }
    }

    void shrink_to_fit() {
        if (is_local()) return;
        size_type len = get_size();
        if (len <= kSSOCapacity) {
            // Move to SSO
            pointer old_data = heap_.data_;
            __builtin_memmove(local_, old_data, len * sizeof(CharT));
            local_[len] = CharT(0);
            set_local_size(len);
            alloc_.deallocate(old_data, heap_.cap_ + 1);
        } else if (len < heap_.cap_) {
            // Shrink heap
            pointer new_data = alloc_.allocate(len + 1);
            __builtin_memmove(new_data, heap_.data_, len * sizeof(CharT));
            new_data[len] = CharT(0);
            alloc_.deallocate(heap_.data_, heap_.cap_ + 1);
            heap_.data_ = new_data;
            heap_.cap_ = len;
            // size_ unchanged
        }
    }

    void clear() noexcept {
        set_size(0);
        data_ptr()[0] = CharT(0);
    }

    // ============================================================
    // Modifiers
    // ============================================================
    basic_string& operator+=(const basic_string& str) {
        return append(str);
    }

    basic_string& operator+=(const CharT* s) {
        return append(s);
    }

    basic_string& operator+=(CharT ch) {
        push_back(ch);
        return *this;
    }

    basic_string& operator+=(std::initializer_list<CharT> il) {
        return append(il);
    }

    basic_string& append(const basic_string& str) {
        return append(str.data_ptr(), str.get_size());
    }

    basic_string& append(const basic_string& str, size_type pos, size_type count = npos) {
        size_type len = str.get_size();
        if (pos > len) throw std::out_of_range("basic_string::append: pos out of range");
        size_type rlen = count < len - pos ? count : len - pos;
        return append(str.data_ptr() + pos, rlen);
    }

    basic_string& append(const CharT* s, size_type count) {
        size_type old_size = get_size();
        ensure_capacity(old_size + count);
        pointer p = data_ptr();
        __builtin_memmove(p + old_size, s, count * sizeof(CharT));
        p[old_size + count] = CharT(0);
        set_size(old_size + count);
        return *this;
    }

    basic_string& append(const CharT* s) {
        return append(s, zstl_strlen(s));
    }

    basic_string& append(size_type count, CharT ch) {
        size_type old_size = get_size();
        ensure_capacity(old_size + count);
        pointer p = data_ptr();
        for (size_type i = 0; i < count; ++i) {
            p[old_size + i] = ch;
        }
        p[old_size + count] = CharT(0);
        set_size(old_size + count);
        return *this;
    }

    template<typename InputIterator,
             typename = decltype(*std::declval<InputIterator&>())>
    basic_string& append(InputIterator first, InputIterator last) {
        for (; first != last; ++first) {
            push_back(*first);
        }
        return *this;
    }

    basic_string& append(std::initializer_list<CharT> il) {
        return append(il.begin(), il.size());
    }

    void push_back(CharT ch) {
        size_type old_size = get_size();
        ensure_capacity(old_size + 1);
        pointer p = data_ptr();
        p[old_size] = ch;
        p[old_size + 1] = CharT(0);
        set_size(old_size + 1);
    }

    void pop_back() {
        size_type sz = get_size();
        if (sz > 0) {
            set_size(sz - 1);
            data_ptr()[sz - 1] = CharT(0);
        }
    }

    // ---- insert ----
    basic_string& insert(size_type index, size_type count, CharT ch) {
        size_type old_size = get_size();
        if (index > old_size) throw std::out_of_range("basic_string::insert");
        ensure_capacity(old_size + count);
        pointer p = data_ptr();
        __builtin_memmove(p + index + count, p + index, (old_size - index) * sizeof(CharT));
        for (size_type i = 0; i < count; ++i) {
            p[index + i] = ch;
        }
        set_size(old_size + count);
        p[old_size + count] = CharT(0);
        return *this;
    }

    basic_string& insert(size_type index, const CharT* s) {
        return insert(index, s, zstl_strlen(s));
    }

    basic_string& insert(size_type index, const CharT* s, size_type count) {
        size_type old_size = get_size();
        if (index > old_size) throw std::out_of_range("basic_string::insert");
        ensure_capacity(old_size + count);
        pointer p = data_ptr();
        __builtin_memmove(p + index + count, p + index, (old_size - index) * sizeof(CharT));
        __builtin_memmove(p + index, s, count * sizeof(CharT));
        set_size(old_size + count);
        p[old_size + count] = CharT(0);
        return *this;
    }

    basic_string& insert(size_type index, const basic_string& str) {
        return insert(index, str.data_ptr(), str.get_size());
    }

    // ---- erase ----
    basic_string& erase(size_type index = 0, size_type count = npos) {
        size_type old_size = get_size();
        if (index > old_size) throw std::out_of_range("basic_string::erase");
        size_type rlen = count < old_size - index ? count : old_size - index;
        pointer p = data_ptr();
        __builtin_memmove(p + index, p + index + rlen, (old_size - index - rlen) * sizeof(CharT));
        set_size(old_size - rlen);
        p[old_size - rlen] = CharT(0);
        return *this;
    }

    iterator erase(const_iterator pos) {
        size_type index = static_cast<size_type>(pos - data_ptr());
        erase(index, 1);
        return data_ptr() + index;
    }

    iterator erase(const_iterator first, const_iterator last) {
        size_type index = static_cast<size_type>(first - data_ptr());
        size_type count = static_cast<size_type>(last - first);
        erase(index, count);
        return data_ptr() + index;
    }

    // ---- replace ----
    basic_string& replace(size_type pos, size_type count, const basic_string& str) {
        return replace(pos, count, str.data_ptr(), str.get_size());
    }

    basic_string& replace(size_type pos, size_type count, const CharT* s, size_type count2) {
        size_type old_size = get_size();
        if (pos > old_size) throw std::out_of_range("basic_string::replace");
        size_type rlen = count < old_size - pos ? count : old_size - pos;
        difference_type diff = static_cast<difference_type>(count2) - rlen;
        if (diff > 0) {
            ensure_capacity(old_size + diff);
        }
        pointer p = data_ptr();
        if (count2 != rlen) {
            __builtin_memmove(p + pos + count2, p + pos + rlen,
                              (old_size - pos - rlen) * sizeof(CharT));
        }
        __builtin_memmove(p + pos, s, count2 * sizeof(CharT));
        set_size(old_size + diff);
        p[old_size + diff] = CharT(0);
        return *this;
    }

    basic_string& replace(size_type pos, size_type count, const CharT* s) {
        return replace(pos, count, s, zstl_strlen(s));
    }

    basic_string& replace(size_type pos, size_type count, size_type count2, CharT ch) {
        size_type old_size = get_size();
        if (pos > old_size) throw std::out_of_range("basic_string::replace");
        size_type rlen = count < old_size - pos ? count : old_size - pos;
        difference_type diff = static_cast<difference_type>(count2) - rlen;
        if (diff > 0) {
            ensure_capacity(old_size + diff);
        }
        pointer p = data_ptr();
        if (static_cast<size_type>(diff) != 0) {
            __builtin_memmove(p + pos + count2, p + pos + rlen,
                            (old_size - pos - rlen) * sizeof(CharT));
        }
        for (size_type i = 0; i < count2; ++i) {
            p[pos + i] = ch;
        }
        set_size(old_size + diff);
        p[old_size + diff] = CharT(0);
        return *this;
    }

    // ---- swap ----
    void swap(basic_string& other) noexcept {
        CharT tmp[16];
        __builtin_memmove(tmp, local_, sizeof(local_));
        __builtin_memmove(local_, other.local_, sizeof(local_));
        __builtin_memmove(other.local_, tmp, sizeof(local_));
    }

    // ============================================================
    // Search operations
    // ============================================================

    // ---- find (forward) ----
    size_type find(const basic_string& str, size_type pos = 0) const noexcept {
        return find(str.data_ptr(), pos, str.get_size());
    }

    size_type find(const CharT* s, size_type pos, size_type count) const noexcept {
        if (count == 0) return pos <= get_size() ? pos : npos;
        size_type n = get_size();
        if (pos + count > n) return npos;
        const CharT* p = data_ptr();
        for (size_type i = pos; i <= n - count; ++i) {
            bool match = true;
            for (size_type j = 0; j < count; ++j) {
                if (p[i + j] != s[j]) {
                    match = false;
                    break;
                }
            }
            if (match) return i;
        }
        return npos;
    }

    size_type find(const CharT* s, size_type pos = 0) const noexcept {
        return find(s, pos, zstl_strlen(s));
    }

    size_type find(CharT ch, size_type pos = 0) const noexcept {
        size_type n = get_size();
        const CharT* p = data_ptr();
        for (size_type i = pos; i < n; ++i) {
            if (p[i] == ch) return i;
        }
        return npos;
    }

    // ---- rfind (backward) ----
    size_type rfind(const basic_string& str, size_type pos = npos) const noexcept {
        return rfind(str.data_ptr(), pos, str.get_size());
    }

    size_type rfind(const CharT* s, size_type pos, size_type count) const noexcept {
        if (count == 0) {
            size_type n = get_size();
            return pos < n ? pos : n;
        }
        size_type n = get_size();
        if (pos >= n) pos = n - 1;
        if (count > n) return npos;
        const CharT* p = data_ptr();
        for (size_type i = pos + 1 - count; ; --i) {
            bool match = true;
            for (size_type j = 0; j < count; ++j) {
                if (p[i + j] != s[j]) {
                    match = false;
                    break;
                }
            }
            if (match) return i;
            if (i == 0) break;
        }
        return npos;
    }

    size_type rfind(const CharT* s, size_type pos = npos) const noexcept {
        return rfind(s, pos, zstl_strlen(s));
    }

    size_type rfind(CharT ch, size_type pos = npos) const noexcept {
        size_type n = get_size();
        if (pos >= n) pos = n - 1;
        const CharT* p = data_ptr();
        for (size_type i = pos + 1; i > 0; --i) {
            if (p[i - 1] == ch) return i - 1;
        }
        return npos;
    }

    // ---- find_first_of ----
    size_type find_first_of(const basic_string& str, size_type pos = 0) const noexcept {
        return find_first_of(str.data_ptr(), pos, str.get_size());
    }

    size_type find_first_of(const CharT* s, size_type pos, size_type count) const noexcept {
        size_type n = get_size();
        const CharT* p = data_ptr();
        for (size_type i = pos; i < n; ++i) {
            for (size_type j = 0; j < count; ++j) {
                if (p[i] == s[j]) return i;
            }
        }
        return npos;
    }

    size_type find_first_of(CharT ch, size_type pos = 0) const noexcept {
        return find(ch, pos);
    }

    // ---- find_last_of ----
    size_type find_last_of(const basic_string& str, size_type pos = npos) const noexcept {
        return find_last_of(str.data_ptr(), pos, str.get_size());
    }

    size_type find_last_of(const CharT* s, size_type pos, size_type count) const noexcept {
        size_type n = get_size();
        if (pos >= n) pos = n - 1;
        const CharT* p = data_ptr();
        for (size_type i = pos + 1; i > 0; --i) {
            for (size_type j = 0; j < count; ++j) {
                if (p[i - 1] == s[j]) return i - 1;
            }
        }
        return npos;
    }

    size_type find_last_of(CharT ch, size_type pos = npos) const noexcept {
        return rfind(ch, pos);
    }

    // ---- find_first_not_of ----
    size_type find_first_not_of(const basic_string& str, size_type pos = 0) const noexcept {
        return find_first_not_of(str.data_ptr(), pos, str.get_size());
    }

    size_type find_first_not_of(const CharT* s, size_type pos, size_type count) const noexcept {
        size_type n = get_size();
        const CharT* p = data_ptr();
        for (size_type i = pos; i < n; ++i) {
            bool found = false;
            for (size_type j = 0; j < count; ++j) {
                if (p[i] == s[j]) { found = true; break; }
            }
            if (!found) return i;
        }
        return npos;
    }

    size_type find_first_not_of(CharT ch, size_type pos = 0) const noexcept {
        size_type n = get_size();
        const CharT* p = data_ptr();
        for (size_type i = pos; i < n; ++i) {
            if (p[i] != ch) return i;
        }
        return npos;
    }

    // ---- find_last_not_of ----
    size_type find_last_not_of(const basic_string& str, size_type pos = npos) const noexcept {
        return find_last_not_of(str.data_ptr(), pos, str.get_size());
    }

    size_type find_last_not_of(const CharT* s, size_type pos, size_type count) const noexcept {
        size_type n = get_size();
        if (pos >= n) pos = n - 1;
        const CharT* p = data_ptr();
        for (size_type i = pos + 1; i > 0; --i) {
            bool found = false;
            for (size_type j = 0; j < count; ++j) {
                if (p[i - 1] == s[j]) { found = true; break; }
            }
            if (!found) return i - 1;
        }
        return npos;
    }

    size_type find_last_not_of(CharT ch, size_type pos = npos) const noexcept {
        size_type n = get_size();
        if (pos >= n) pos = n - 1;
        const CharT* p = data_ptr();
        for (size_type i = pos + 1; i > 0; --i) {
            if (p[i - 1] != ch) return i - 1;
        }
        return npos;
    }

    // ============================================================
    // Compare
    // ============================================================
    int compare(const basic_string& str) const noexcept {
        size_type n1 = get_size();
        size_type n2 = str.get_size();
        size_type rlen = n1 < n2 ? n1 : n2;
        int result = __builtin_memcmp(data_ptr(), str.data_ptr(), rlen * sizeof(CharT));
        if (result != 0) return result;
        if (n1 < n2) return -1;
        if (n1 > n2) return 1;
        return 0;
    }

    int compare(size_type pos, size_type count, const basic_string& str) const {
        return substr(pos, count).compare(str);
    }

    int compare(const CharT* s) const noexcept {
        return compare(basic_string(s));
    }

    // ============================================================
    // starts_with / ends_with (C++20 style)
    // ============================================================
    bool starts_with(const basic_string& str) const noexcept {
        size_type n1 = get_size();
        size_type n2 = str.get_size();
        if (n2 > n1) return false;
        return __builtin_memcmp(data_ptr(), str.data_ptr(), n2 * sizeof(CharT)) == 0;
    }

    bool starts_with(CharT ch) const noexcept {
        return get_size() > 0 && data_ptr()[0] == ch;
    }

    bool starts_with(const CharT* s) const noexcept {
        return starts_with(basic_string(s));
    }

    bool ends_with(const basic_string& str) const noexcept {
        size_type n1 = get_size();
        size_type n2 = str.get_size();
        if (n2 > n1) return false;
        return __builtin_memcmp(data_ptr() + n1 - n2, str.data_ptr(), n2 * sizeof(CharT)) == 0;
    }

    bool ends_with(CharT ch) const noexcept {
        size_type n = get_size();
        return n > 0 && data_ptr()[n - 1] == ch;
    }

    bool ends_with(const CharT* s) const noexcept {
        return ends_with(basic_string(s));
    }

    // ============================================================
    // substr
    // ============================================================
    basic_string substr(size_type pos = 0, size_type count = npos) const {
        size_type n = get_size();
        if (pos > n) throw std::out_of_range("basic_string::substr");
        size_type rlen = count < n - pos ? count : n - pos;
        return basic_string(data_ptr() + pos, rlen);
    }

    // ============================================================
    // Conversion (to integer types)
    // ============================================================
    int to_int() const { return std::atoi(c_str()); }
    long to_long() const { return std::atol(c_str()); }
    long long to_llong() const { return std::atoll(c_str()); }
    unsigned long to_ulong() const { return std::strtoul(c_str(), nullptr, 10); }
    float to_float() const { return std::atof(c_str()); }
    double to_double() const { return std::atof(c_str()); }

private:
    static size_type zstl_strlen(const CharT* s) noexcept {
        size_type len = 0;
        while (s[len]) ++len;
        return len;
    }
};

// ============================================================
// Typedefs
// ============================================================
using string  = basic_string<char>;
using wstring = basic_string<wchar_t>;

// ============================================================
// to_string — convert integer/float to string
// ============================================================
inline string to_string(int val) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%d", val);
    return string(buf, static_cast<size_t>(len));
}

inline string to_string(long val) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%ld", val);
    return string(buf, static_cast<size_t>(len));
}

inline string to_string(long long val) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%lld", val);
    return string(buf, static_cast<size_t>(len));
}

inline string to_string(unsigned int val) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%u", val);
    return string(buf, static_cast<size_t>(len));
}

inline string to_string(unsigned long val) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%lu", val);
    return string(buf, static_cast<size_t>(len));
}

inline string to_string(unsigned long long val) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%llu", val);
    return string(buf, static_cast<size_t>(len));
}

inline string to_string(float val) {
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%g", val);
    return string(buf, static_cast<size_t>(len));
}

inline string to_string(double val) {
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%g", val);
    return string(buf, static_cast<size_t>(len));
}

// ============================================================
// stoi / stol / stoul / stof / stod
// ============================================================
inline int stoi(const string& str, size_t* idx = nullptr, int base = 10) {
    const char* s = str.c_str();
    char* end;
    long result = std::strtol(s, &end, base);
    if (idx) *idx = static_cast<size_t>(end - s);
    return static_cast<int>(result);
}

inline long stol(const string& str, size_t* idx = nullptr, int base = 10) {
    const char* s = str.c_str();
    char* end;
    long result = std::strtol(s, &end, base);
    if (idx) *idx = static_cast<size_t>(end - s);
    return result;
}

inline unsigned long stoul(const string& str, size_t* idx = nullptr, int base = 10) {
    const char* s = str.c_str();
    char* end;
    unsigned long result = std::strtoul(s, &end, base);
    if (idx) *idx = static_cast<size_t>(end - s);
    return result;
}

inline float stof(const string& str, size_t* idx = nullptr) {
    const char* s = str.c_str();
    char* end;
    float result = std::strtof(s, &end);
    if (idx) *idx = static_cast<size_t>(end - s);
    return result;
}

inline double stod(const string& str, size_t* idx = nullptr) {
    const char* s = str.c_str();
    char* end;
    double result = std::strtod(s, &end);
    if (idx) *idx = static_cast<size_t>(end - s);
    return result;
}

// ============================================================
// Concatenation operators
// ============================================================
inline string operator+(const string& lhs, const string& rhs) {
    string result(lhs);
    result += rhs;
    return result;
}

inline string operator+(const string& lhs, const char* rhs) {
    string result(lhs);
    result += rhs;
    return result;
}

inline string operator+(const char* lhs, const string& rhs) {
    string result(lhs);
    result += rhs;
    return result;
}

inline string operator+(const string& lhs, char rhs) {
    string result(lhs);
    result += rhs;
    return result;
}

inline string operator+(char lhs, const string& rhs) {
    string result(1, lhs);
    result += rhs;
    return result;
}

inline string operator+(string&& lhs, const string& rhs) {
    lhs += rhs;
    return zstl::move(lhs);
}

inline string operator+(string&& lhs, const char* rhs) {
    lhs += rhs;
    return zstl::move(lhs);
}

inline string operator+(string&& lhs, char rhs) {
    lhs += rhs;
    return zstl::move(lhs);
}

// ============================================================
// Comparison operators
// ============================================================
inline bool operator==(const string& a, const string& b) noexcept { return a.compare(b) == 0; }
inline bool operator!=(const string& a, const string& b) noexcept { return a.compare(b) != 0; }
inline bool operator<(const string& a, const string& b) noexcept { return a.compare(b) < 0; }
inline bool operator>(const string& a, const string& b) noexcept { return a.compare(b) > 0; }
inline bool operator<=(const string& a, const string& b) noexcept { return a.compare(b) <= 0; }
inline bool operator>=(const string& a, const string& b) noexcept { return a.compare(b) >= 0; }

inline bool operator==(const string& a, const char* b) noexcept { return a.compare(b) == 0; }
inline bool operator==(const char* a, const string& b) noexcept { return b.compare(a) == 0; }
inline bool operator!=(const string& a, const char* b) noexcept { return a.compare(b) != 0; }
inline bool operator!=(const char* a, const string& b) noexcept { return b.compare(a) != 0; }

// ============================================================
// Non-member swap
// ============================================================
template<typename CharT, typename Alloc>
void swap(basic_string<CharT, Alloc>& a, basic_string<CharT, Alloc>& b) noexcept {
    a.swap(b);
}

// ============================================================
// Hash support
// ============================================================
template<typename CharT>
struct hash;

template<>
struct hash<string> {
    size_t operator()(const string& s) const noexcept {
        // FNV-1a hash
        size_t h = 14695981039346656037ULL;
        for (char c : s) {
            h ^= static_cast<size_t>(static_cast<unsigned char>(c));
            h *= 1099511628211ULL;
        }
        return h;
    }
};

} // namespace zstl
