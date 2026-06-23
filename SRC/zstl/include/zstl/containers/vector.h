// zstl vector — dynamic array with growth strategy and POD memmove optimization
//
// Growth strategy: 2x for large vectors (capacity >= 4096/sizeof(T)),
//                  1.5x for small vectors, minimum initial capacity 4.
// POD optimization: uses __builtin_memmove for trivially relocatable types.
// Exception safety: strong guarantee for insert when copy may throw.
//
// Iterator invalidation:
//   - insert/push_back/emplace_back: all iterators if reallocation occurs,
//                                    only iterators after insertion point otherwise.
//   - erase/pop_back: iterators at and after the erased element.
//   - reserve/shrink_to_fit: all iterators.
#pragma once

#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <algorithm>
#include "zstl/memory/allocator.h"
#include "zstl/memory/construct.h"
#include "zstl/memory/utility.h"
#include "zstl/memory/type_traits.h"
#include "zstl/iterators/iterator_traits.h"
#include "zstl/iterators/reverse_iterator.h"

namespace zstl {

template<typename T, typename Alloc = default_alloc<T>>
class vector {
public:
    // ---- types ----
    using value_type      = T;
    using allocator_type  = Alloc;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;
    using reference       = T&;
    using const_reference = const T&;
    using pointer         = T*;
    using const_pointer   = const T*;
    using iterator        = T*;
    using const_iterator  = const T*;
    using reverse_iterator       = zstl::reverse_iterator<iterator>;
    using const_reverse_iterator = zstl::reverse_iterator<const_iterator>;

    // ---- growth policy constants ----
    static constexpr size_t kMinCapacity = 4;
    static constexpr size_t kLargeThreshold = 4096;  // bytes

private:
    pointer begin_   = nullptr;   // first element
    pointer end_     = nullptr;   // one past last element
    pointer cap_end_ = nullptr;   // one past end of allocated memory
    [[no_unique_address]] allocator_type alloc_;

    // ---- helpers ----
    size_type growth_capacity(size_type current_cap) const noexcept {
        if (current_cap == 0) return kMinCapacity;
        // Use 2x for large vectors (>512 elements typical), 1.5x for small
        size_t current_bytes = current_cap * sizeof(T);
        if (current_bytes >= kLargeThreshold) {
            return current_cap * 2;
        }
        return current_cap + current_cap / 2;  // 1.5x
    }

    void reallocate(size_type new_cap) {
        pointer new_begin = alloc_.allocate(new_cap);
        size_type old_size = size();

        // Move existing elements
        if constexpr (is_trivially_relocatable_v<T>) {
            if (old_size > 0) {
                __builtin_memmove(new_begin, begin_, old_size * sizeof(T));
            }
        } else {
            uninitialized_move(begin_, end_, new_begin);
            destroy_range(begin_, end_);
        }

        deallocate();
        begin_ = new_begin;
        end_ = new_begin + old_size;
        cap_end_ = new_begin + new_cap;
    }

    void deallocate() noexcept {
        if (begin_) {
            alloc_.deallocate(begin_, capacity());
        }
    }

    // Grow enough to fit at least one more element
    void ensure_capacity() {
        if (end_ == cap_end_) {
            reallocate(growth_capacity(capacity()));
        }
    }

    // Grow enough to fit n more elements (used by range insert)
    void ensure_capacity(size_type extra) {
        if (static_cast<size_type>(cap_end_ - end_) < extra) {
            size_type new_cap = capacity() ? capacity() : kMinCapacity;
            while (new_cap < size() + extra) {
                new_cap = growth_capacity(new_cap);
            }
            reallocate(new_cap);
        }
    }

    // Shift elements right by n positions starting at pos.
    // Precondition: there is room (caller ensured capacity).
    void shift_right(iterator pos, size_type n) {
        iterator old_end = end_;
        end_ += n;
        if constexpr (is_trivially_relocatable_v<T>) {
            __builtin_memmove(pos + n, pos, (old_end - pos) * sizeof(T));
        } else {
            // Move from back to front to avoid overwriting
            for (iterator it = old_end - 1; it >= pos; --it) {
                construct(it + n, zstl::move(*it));
                destroy(it);
            }
        }
    }

    // Shift elements left by n positions starting at pos, destroying freed slots
    void shift_left(iterator pos, size_type n) {
        if constexpr (is_trivially_relocatable_v<T>) {
            __builtin_memmove(pos, pos + n, (end_ - pos - n) * sizeof(T));
        } else {
            for (iterator it = pos; it < end_ - n; ++it) {
                *it = zstl::move(*(it + n));
            }
        }
        destroy_range(end_ - n, end_);
        end_ -= n;
    }

public:
    // ============================================================
    // Constructors
    // ============================================================
    vector() noexcept = default;

    explicit vector(size_type n) {
        resize(n);
    }

    vector(size_type n, const T& value) {
        reserve(n);
        end_ = uninitialized_fill_n(begin_, n, value);
    }

    template<typename InputIterator,
             typename = enable_if_t<detail::is_input_iterator_v<InputIterator>>>
    vector(InputIterator first, InputIterator last) {
        for (; first != last; ++first) {
            push_back(*first);
        }
    }

    vector(const vector& other) {
        reserve(other.size());
        end_ = uninitialized_copy(other.begin_, other.end_, begin_);
    }

    vector(vector&& other) noexcept
        : begin_(other.begin_)
        , end_(other.end_)
        , cap_end_(other.cap_end_) {
        other.begin_ = nullptr;
        other.end_ = nullptr;
        other.cap_end_ = nullptr;
    }

    vector(std::initializer_list<T> il) {
        reserve(il.size());
        for (const auto& v : il) {
            construct(end_, v);
            ++end_;
        }
    }

    ~vector() {
        destroy_range(begin_, end_);
        deallocate();
    }

    // ============================================================
    // Assignment
    // ============================================================
    vector& operator=(const vector& other) {
        if (this != &other) {
            // Strong exception guarantee: copy to temp, then swap
            vector tmp(other);
            swap(tmp);
        }
        return *this;
    }

    vector& operator=(vector&& other) noexcept {
        if (this != &other) {
            destroy_range(begin_, end_);
            deallocate();
            begin_ = other.begin_;
            end_ = other.end_;
            cap_end_ = other.cap_end_;
            other.begin_ = nullptr;
            other.end_ = nullptr;
            other.cap_end_ = nullptr;
        }
        return *this;
    }

    vector& operator=(std::initializer_list<T> il) {
        assign(il);
        return *this;
    }

    // ============================================================
    // assign — replace all contents
    // ============================================================
    void assign(size_type count, const T& value) {
        clear();
        if (count > capacity()) {
            reallocate(count);
        }
        end_ = uninitialized_fill_n(begin_, count, value);
    }

    template<typename InputIterator,
             typename = enable_if_t<detail::is_input_iterator_v<InputIterator>>>
    void assign(InputIterator first, InputIterator last) {
        clear();
        for (; first != last; ++first) {
            push_back(*first);
        }
    }

    void assign(std::initializer_list<T> il) {
        clear();
        reserve(il.size());
        for (const auto& v : il) {
            construct(end_, v);
            ++end_;
        }
    }

    // ============================================================
    // Element access
    // ============================================================
    reference operator[](size_type i) noexcept { return begin_[i]; }
    const_reference operator[](size_type i) const noexcept { return begin_[i]; }

    reference at(size_type i) {
        if (i >= size()) throw std::out_of_range("vector::at: index out of range");
        return begin_[i];
    }
    const_reference at(size_type i) const {
        if (i >= size()) throw std::out_of_range("vector::at: index out of range");
        return begin_[i];
    }

    reference front() noexcept { return *begin_; }
    const_reference front() const noexcept { return *begin_; }
    reference back() noexcept { return *(end_ - 1); }
    const_reference back() const noexcept { return *(end_ - 1); }

    pointer data() noexcept { return begin_; }
    const_pointer data() const noexcept { return begin_; }

    // ============================================================
    // Iterators
    // ============================================================
    iterator begin() noexcept { return begin_; }
    const_iterator begin() const noexcept { return begin_; }
    const_iterator cbegin() const noexcept { return begin_; }
    iterator end() noexcept { return end_; }
    const_iterator end() const noexcept { return end_; }
    const_iterator cend() const noexcept { return end_; }

    reverse_iterator rbegin() noexcept { return reverse_iterator(end_); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end_); }
    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end_); }
    reverse_iterator rend() noexcept { return reverse_iterator(begin_); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin_); }
    const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin_); }

    // ============================================================
    // Capacity
    // ============================================================
    bool empty() const noexcept { return begin_ == end_; }
    size_type size() const noexcept { return static_cast<size_type>(end_ - begin_); }
    size_type capacity() const noexcept { return static_cast<size_type>(cap_end_ - begin_); }
    size_type max_size() const noexcept { return static_cast<size_type>(-1) / sizeof(T); }

    void reserve(size_type n) {
        if (n > capacity()) {
            reallocate(n);
        }
    }

    void shrink_to_fit() {
        if (capacity() > size()) {
            if (size() == 0) {
                deallocate();
                begin_ = nullptr;
                end_ = nullptr;
                cap_end_ = nullptr;
            } else {
                reallocate(size());
            }
        }
    }

    // ============================================================
    // Modifiers
    // ============================================================
    void clear() noexcept {
        destroy_range(begin_, end_);
        end_ = begin_;
    }

    // ---- insert (single element) ----
    iterator insert(const_iterator pos, const T& value) {
        return emplace(pos, value);
    }

    iterator insert(const_iterator pos, T&& value) {
        return emplace(pos, zstl::move(value));
    }

    // ---- insert (fill) ----
    iterator insert(const_iterator pos, size_type count, const T& value) {
        if (count == 0) return begin_ + (pos - begin_);
        difference_type offset = pos - begin_;
        ensure_capacity(count);
        iterator p = begin_ + offset;
        shift_right(p, count);
        // Fill with copies
        for (size_type i = 0; i < count; ++i) {
            construct(p + i, value);
        }
        return p;
    }

    // ---- insert (range) ----
    template<typename InputIterator,
             typename = enable_if_t<detail::is_input_iterator_v<InputIterator>>>
    iterator insert(const_iterator pos, InputIterator first, InputIterator last) {
        difference_type offset = pos - begin_;
        if (first == last) return begin_ + offset;

        // For input iterators, we must push_back one at a time and then rotate
        // to preserve strong exception guarantee
        size_type old_size = size();
        iterator insert_point = begin_ + offset;

        // If it's a forward iterator, we can compute distance and reserve
        if constexpr (detail::is_forward_iterator_v<InputIterator>) {
            size_type count = static_cast<size_type>(zstl::distance(first, last));
            ensure_capacity(count);
            iterator p = begin_ + offset;
            shift_right(p, count);
            try {
                for (size_type i = 0; first != last; ++first, ++i) {
                    construct(p + i, *first);
                }
            } catch (...) {
                // Restore: destroy inserted elements and shift back
                destroy_range(p, p + (first - (begin_ + offset)));
                shift_left(p, count);
                throw;
            }
            return p;
        } else {
            // Input iterator: append then rotate
            // Use tag dispatch to avoid copying elements
            size_type count = 0;
            size_type remaining_cap = static_cast<size_type>(cap_end_ - end_);
            try {
                for (; first != last; ++first) {
                    if (end_ == cap_end_) {
                        // Compute offset before reallocation
                        difference_type new_offset = insert_point - begin_;
                        reallocate(growth_capacity(capacity()));
                        insert_point = begin_ + new_offset;
                    }
                    construct(end_, *first);
                    ++end_;
                    ++count;
                }
            } catch (...) {
                // Destroy the elements we appended
                destroy_range(end_ - count, end_);
                end_ -= count;
                throw;
            }
            // Rotate the inserted elements into position
            std::rotate(insert_point, end_ - count, end_);
            return insert_point;
        }
    }

    // ---- insert (initializer_list) ----
    iterator insert(const_iterator pos, std::initializer_list<T> il) {
        return insert(pos, il.begin(), il.end());
    }

    // ---- emplace ----
    template<typename... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        difference_type offset = pos - begin_;
        ensure_capacity();
        iterator p = begin_ + offset;
        if (p < end_) {
            // Make room: construct last element as a copy of the previous last,
            // then shift all elements right by one
            if (end_ < cap_end_) {
                // Room exists, move elements right
                if constexpr (is_trivially_relocatable_v<T>) {
                    __builtin_memmove(p + 1, p, (end_ - p) * sizeof(T));
                } else {
                    construct(end_, zstl::move(*(end_ - 1)));
                    for (iterator it = end_ - 1; it > p; --it) {
                        *it = zstl::move(*(it - 1));
                    }
                    destroy(p);  // p now contains moved-from value
                }
                ++end_;
            }
        } else {
            ++end_;
        }
        // Construct in place
        construct(p, zstl::forward<Args>(args)...);
        return p;
    }

    // ---- emplace_back ----
    template<typename... Args>
    reference emplace_back(Args&&... args) {
        if (end_ == cap_end_) {
            reallocate(growth_capacity(capacity()));
        }
        construct(end_, zstl::forward<Args>(args)...);
        ++end_;
        return back();
    }

    // ---- push_back ----
    void push_back(const T& value) {
        emplace_back(value);
    }

    void push_back(T&& value) {
        emplace_back(zstl::move(value));
    }

    // ---- pop_back ----
    void pop_back() {
        --end_;
        destroy(end_);
    }

    // ---- erase (single element) ----
    // Returns iterator following the erased element.
    // O(n) — shifts remaining elements.
    iterator erase(const_iterator pos) {
        iterator p = begin_ + (pos - begin_);
        if constexpr (is_trivially_relocatable_v<T>) {
            if (p + 1 < end_) {
                __builtin_memmove(p, p + 1, (end_ - p - 1) * sizeof(T));
            }
        } else {
            for (iterator it = p; it < end_ - 1; ++it) {
                *it = zstl::move(*(it + 1));
            }
        }
        --end_;
        destroy(end_);
        return p;
    }

    // ---- erase (range) ----
    // Returns iterator following the last erased element.
    // O(n) — shifts remaining elements.
    iterator erase(const_iterator first, const_iterator last) {
        if (first == last) return begin_ + (first - begin_);
        iterator p = begin_ + (first - begin_);
        size_type count = static_cast<size_type>(last - first);
        shift_left(p, count);
        return p;
    }

    // ---- resize ----
    void resize(size_type n) {
        if (n < size()) {
            destroy_range(begin_ + n, end_);
            end_ = begin_ + n;
        } else if (n > size()) {
            reserve(n);
            end_ = uninitialized_fill_n(end_, n - size(), T());
        }
    }

    void resize(size_type n, const T& value) {
        if (n < size()) {
            destroy_range(begin_ + n, end_);
            end_ = begin_ + n;
        } else if (n > size()) {
            reserve(n);
            end_ = uninitialized_fill_n(end_, n - size(), value);
        }
    }

    // ---- swap ----
    void swap(vector& other) noexcept {
        zstl::swap(begin_, other.begin_);
        zstl::swap(end_, other.end_);
        zstl::swap(cap_end_, other.cap_end_);
    }

    // ============================================================
    // Non-member functions (friends)
    // ============================================================
    friend bool operator==(const vector& a, const vector& b) {
        if (a.size() != b.size()) return false;
        for (size_type i = 0; i < a.size(); ++i) {
            if (!(a[i] == b[i])) return false;
        }
        return true;
    }

    friend bool operator!=(const vector& a, const vector& b) {
        return !(a == b);
    }

    friend bool operator<(const vector& a, const vector& b) {
        size_type n = a.size() < b.size() ? a.size() : b.size();
        for (size_type i = 0; i < n; ++i) {
            if (a[i] < b[i]) return true;
            if (b[i] < a[i]) return false;
        }
        return a.size() < b.size();
    }

    friend bool operator>(const vector& a, const vector& b) {
        return b < a;
    }

    friend bool operator<=(const vector& a, const vector& b) {
        return !(b < a);
    }

    friend bool operator>=(const vector& a, const vector& b) {
        return !(a < b);
    }
};

// ============================================================
// Non-member swap
// ============================================================
template<typename T, typename Alloc>
void swap(vector<T, Alloc>& a, vector<T, Alloc>& b) noexcept {
    a.swap(b);
}

} // namespace zstl
