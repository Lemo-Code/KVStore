/**
 * @file    deque.h
 * @brief   Double-ended queue (segmented array) with O(1) front/back ops.
 * @author  lstl team
 * @date    2025
 *
 * lstl::deque is implemented as a segmented array: an array ("map") of
 * pointers to fixed-size buffers. This provides:
 * - O(1) random access (with small constant factor).
 * - O(1) push/pop at both ends.
 * - Stable references to elements (unless erased from the middle).
 * - Good cache locality within each buffer.
 *
 * @tparam T        Element type.
 * @tparam BufSize  Number of elements per buffer (default: 64).
 * @tparam Alloc    Allocator type.
 *
 * @ingroup container
 */

#pragma once

#include <cstddef>
#include <iterator>
#include <algorithm>
#include <stdexcept>

#include "../memory/utility.h"
#include "../memory/construct.h"
#include "../memory/uninitialized.h"
#include "../memory/allocator.h"
#include "../memory/alloc.h"
#include "../memory/pool.h"
#include "detail/deque_iterator.h"
#include "detail/reverse_iterator.h"

namespace lstl {

/**
 * @brief  Double-ended queue using segmented storage.
 *
 * @tparam T     Element type.
 * @tparam Alloc Allocator type.
 */
template <typename T, typename Alloc = allocator<T>>
class deque {
public:
    typedef T                                           value_type;
    typedef T*                                          pointer;
    typedef const T*                                    const_pointer;
    typedef T&                                          reference;
    typedef const T&                                    const_reference;
    typedef size_t                                      size_type;
    typedef ptrdiff_t                                   difference_type;

    /** @brief  Dynamic buffer size: emulates std::deque's 512-byte block strategy. */
    static constexpr size_t buffer_size() {
        return sizeof(T) < 512 ? 512 / sizeof(T) : 1;
    }
    static const size_t kBufferSize = buffer_size();

    typedef detail::deque_iterator<T, kBufferSize>       iterator;
    typedef detail::deque_iterator<const T, kBufferSize> const_iterator;
    typedef detail::reverse_iterator<iterator>           reverse_iterator;
    typedef detail::reverse_iterator<const_iterator>     const_reverse_iterator;
    typedef Alloc                                       allocator_type;

    typedef simple_alloc<T, malloc_alloc>               data_allocator;
    typedef simple_alloc<T*, malloc_alloc>              map_allocator;

private:
    iterator start_;     ///< Iterator to the first element.
    iterator finish_;    ///< Iterator past the last element.
    T**      map_;       ///< Array of pointers to buffers ("map").
    size_type map_size_; ///< Number of pointers in the map.

    static const size_t kMinMapNodes = 8;

    /** @brief  Grows or reshuffles the map to provide space at the front or back. */
    void reallocate_map(size_type nodes_to_add, bool add_at_front);
    void reserve_map_at_front(size_type nodes_to_add = 1);
    void reserve_map_at_back(size_type nodes_to_add = 1);
    T* allocate_buffer() { return data_allocator::allocate(kBufferSize); }

public:
    // ---- Construction ----
    deque();
    explicit deque(size_type n, const T& value = T());
    deque(const deque& other);
    ~deque() { destroy_all(); }
    deque& operator=(const deque& other) {
        if (this != &other) { deque tmp(other); swap(tmp); }
        return *this;
    }

    // ---- Element Access ----
    reference operator[](size_type n)       { return start_[n]; }
    const_reference operator[](size_type n) const { return start_[n]; }
    reference at(size_type n);
    const_reference at(size_type n) const;
    reference front()             { return *start_; }
    const_reference front() const { return *start_; }
    reference back()              { iterator tmp = finish_; --tmp; return *tmp; }
    const_reference back() const  { const_iterator tmp = finish_; --tmp; return *tmp; }

    // ---- Iterators ----
    iterator begin()             { return start_; }
    const_iterator begin() const { return start_; }
    const_iterator cbegin() const { return begin(); }
    iterator end()               { return finish_; }
    const_iterator end() const   { return finish_; }
    reverse_iterator rbegin() { return reverse_iterator(finish_); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator(finish_); }
    reverse_iterator rend() { return reverse_iterator(start_); }
    const_reverse_iterator rend() const { return const_reverse_iterator(start_); }

    // ---- Capacity ----
    size_type size() const { return static_cast<size_type>(finish_ - start_); }
    bool empty() const { return start_ == finish_; }

    // ---- Modifiers ----
    void push_front(const T& value) {
        if (start_.cur != start_.first) {
            --start_.cur;
            construct(start_.cur, value);
        } else {
            reserve_map_at_front();
            *(start_.node - 1) = allocate_buffer();
            start_.set_node(start_.node - 1);
            start_.cur = start_.last - 1;
            construct(start_.cur, value);
        }
    }
    void push_front(T&& value) {
        if (start_.cur != start_.first) {
            --start_.cur;
            construct(start_.cur, lstl::move(value));
        } else {
            reserve_map_at_front();
            *(start_.node - 1) = allocate_buffer();
            start_.set_node(start_.node - 1);
            start_.cur = start_.last - 1;
            construct(start_.cur, lstl::move(value));
        }
    }
    void push_back(const T& value) {
        if (finish_.cur != finish_.last - 1) {
            construct(finish_.cur, value);
            ++finish_.cur;
        } else {
            reserve_map_at_back();
            *(finish_.node + 1) = allocate_buffer();
            construct(finish_.cur, value);
            finish_.set_node(finish_.node + 1);
            finish_.cur = finish_.first;
        }
    }
    void push_back(T&& value) {
        if (finish_.cur != finish_.last - 1) {
            construct(finish_.cur, lstl::move(value));
            ++finish_.cur;
        } else {
            reserve_map_at_back();
            *(finish_.node + 1) = allocate_buffer();
            construct(finish_.cur, lstl::move(value));
            finish_.set_node(finish_.node + 1);
            finish_.cur = finish_.first;
        }
    }
    void pop_front() {
        lstl::destroy(start_.cur);
        ++start_.cur;
        if (start_.cur == start_.last) {
            if (start_.node != finish_.node) {
                data_allocator::deallocate(*start_.node, kBufferSize);
                *start_.node = nullptr;  // prevent double-free
                start_.set_node(start_.node + 1);
                start_.cur = start_.first;
            } else {
                start_.cur = start_.first;
                finish_.cur = start_.first;
            }
        }
    }
    void pop_back() {
        if (finish_.cur != finish_.first) {
            --finish_.cur;
        } else {
            if (finish_.node != start_.node) {
                data_allocator::deallocate(*finish_.node, kBufferSize);
                *finish_.node = nullptr;
                finish_.set_node(finish_.node - 1);
                finish_.cur = finish_.last;
            } else {
                finish_.cur = finish_.first;
            }
            --finish_.cur;
        }
        lstl::destroy(finish_.cur);
    }
    void clear();
    void destroy_all();  ///< Full cleanup: destroys elements + frees all buffers + map.
    void swap(deque& other);

private:
    // Direct assignment for trivially-copyable types
public:
};

// =========================================================================
// Out-of-line implementations
// =========================================================================

template <typename T, typename A>
deque<T,A>::deque() {
    map_size_ = kMinMapNodes;
    map_ = map_allocator::allocate(map_size_);
    T** start_node = map_ + map_size_ / 2;
    *start_node = allocate_buffer();
    start_.set_node(start_node);
    start_.cur = start_.first;
    finish_.set_node(start_node);
    finish_.cur = start_.first;
}

template <typename T, typename A>
deque<T,A>::deque(size_type n, const T& value) {
    map_size_ = kMinMapNodes;
    map_ = map_allocator::allocate(map_size_);
    T** start_node = map_ + map_size_ / 2;
    *start_node = allocate_buffer();
    start_.set_node(start_node); start_.cur = start_.first;
    finish_.set_node(start_node); finish_.cur = start_.first;
    for (size_type i = 0; i < n; ++i) push_back(value);
}

template <typename T, typename A>
deque<T,A>::deque(const deque& other) {
    map_size_ = kMinMapNodes;
    map_ = map_allocator::allocate(map_size_);
    T** start_node = map_ + map_size_ / 2;
    *start_node = allocate_buffer();
    start_.set_node(start_node); start_.cur = start_.first;
    finish_.set_node(start_node); finish_.cur = start_.first;
    for (auto& x : other) push_back(x);
}

template <typename T, typename A>
auto deque<T,A>::at(size_type n) -> reference {
    if (n >= size()) throw std::out_of_range("deque::at");
    return start_[n];
}

template <typename T, typename A>
auto deque<T,A>::at(size_type n) const -> const_reference {
    if (n >= size()) throw std::out_of_range("deque::at");
    return start_[n];
}

template <typename T, typename A>
void deque<T,A>::clear() {
    // Destroy elements in all buffers between start_ and finish_
    for (T** node = start_.node; node <= finish_.node; ++node) {
        T* buf_begin = *node;
        T* buf_end   = (node == finish_.node) ? finish_.cur : buf_begin + kBufferSize;
        T* buf_start = (node == start_.node)  ? start_.cur  : buf_begin;
        for (T* p = buf_start; p != buf_end; ++p) destroy(p);
        // Free the buffer (except the middle one we'll reuse)
        if (node != map_ + map_size_ / 2) {
            data_allocator::deallocate(*node, kBufferSize);
            *node = nullptr;
        }
    }
    // Reset to single reusable buffer in the middle of the map
    T** mid_node = map_ + map_size_ / 2;
    if (!*mid_node) *mid_node = allocate_buffer();
    start_.set_node(mid_node);
    start_.cur = start_.first;
    finish_.set_node(mid_node);
    finish_.cur = start_.first;
}

template <typename T, typename A>
void deque<T,A>::destroy_all() {
    clear();
    // Free the remaining middle buffer and the map itself
    T** mid = map_ + map_size_ / 2;
    if (*mid) {
        data_allocator::deallocate(*mid, kBufferSize);
        *mid = nullptr;
    }
    map_allocator::deallocate(map_, map_size_);
    map_ = nullptr;
    map_size_ = 0;
}

template <typename T, typename A>
void deque<T,A>::swap(deque& other) {
    lstl::swap(start_, other.start_);
    lstl::swap(finish_, other.finish_);
    lstl::swap(map_, other.map_);
    lstl::swap(map_size_, other.map_size_);
}

template <typename T, typename A>
void deque<T,A>::reallocate_map(size_type nodes_to_add, bool add_at_front) {
    size_type old_num_nodes = finish_.node - start_.node + 1;
    size_type new_num_nodes = old_num_nodes + nodes_to_add;

    T** new_map; size_type new_map_size; T** new_start_node;

    if (map_size_ > 2 * new_num_nodes) {
        new_map = map_;
        new_map_size = map_size_;
        if (add_at_front)
            new_start_node = new_map + (map_size_ - new_num_nodes) / 2 + nodes_to_add;
        else
            new_start_node = new_map + (map_size_ - new_num_nodes) / 2;
        if (new_start_node < start_.node)
            std::copy(start_.node, finish_.node + 1, new_start_node);
        else
            std::copy_backward(start_.node, finish_.node + 1, new_start_node + old_num_nodes);
    } else {
        new_map_size = map_size_ + (map_size_ > nodes_to_add ? map_size_ : nodes_to_add) + 2;
        new_map = map_allocator::allocate(new_map_size);
        new_start_node = new_map + (new_map_size - new_num_nodes) / 2;
        std::copy(start_.node, finish_.node + 1, new_start_node);
        map_allocator::deallocate(map_, map_size_);
        map_ = new_map;
        map_size_ = new_map_size;
    }
    start_.set_node(new_start_node);
    finish_.set_node(new_start_node + old_num_nodes - 1);
}

template <typename T, typename A>
void deque<T,A>::reserve_map_at_front(size_type nodes_to_add) {
    if (nodes_to_add > static_cast<size_type>(start_.node - map_))
        reallocate_map(nodes_to_add, true);
}

template <typename T, typename A>
void deque<T,A>::reserve_map_at_back(size_type nodes_to_add) {
    if (nodes_to_add + 1 > map_size_ - (finish_.node - map_))
        reallocate_map(nodes_to_add, false);
}

} // namespace lstl
