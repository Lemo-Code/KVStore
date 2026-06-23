/**
 * @file    deque_iterator.h
 * @brief   Random-access iterator for segmented deque storage.
 * @author  lstl team
 * @date    2025
 * @ingroup container_detail
 */
// Use of this source code is governed by a MIT-style license.
//
// deque_iterator.h - Random-access iterator for lstl::deque.
//
// Deque is implemented as a segmented array (array of pointers to blocks).
// The iterator maintains:
//   - cur: pointer to current element within a block
//   - first/last: boundaries of the current block
//   - node: pointer to the current block pointer in the map

#pragma once

#include <cstddef>      // ptrdiff_t
#include <iterator>     // random_access_iterator_tag
#include <type_traits>  // enable_if, is_const

namespace lstl {
namespace detail {

////////////////////////////////////////////////////////////////////////////
// deque_iterator - Random-access iterator for segmented deque
////////////////////////////////////////////////////////////////////////////
template <typename T, size_t BufSize>
class deque_iterator {
public:
    typedef T                               value_type;
    typedef T*                              pointer;
    typedef T&                              reference;
    typedef ptrdiff_t                       difference_type;
    typedef std::random_access_iterator_tag iterator_category;

    typedef deque_iterator<T, BufSize>      self;

    T*    cur;    // current element in current buffer
    T*    first;  // start of current buffer
    T*    last;   // end of current buffer (one past last)
    T**   node;   // pointer into the map (points to current buffer pointer)

    deque_iterator()
        : cur(nullptr), first(nullptr), last(nullptr), node(nullptr) {}

    deque_iterator(T* c, T** n)
        : cur(c), first(*n), last(*n + BufSize), node(n) {}

    // Implicit conversion from iterator to const_iterator.
    // Enabled only when T is const and U is the matching non-const type.
    template <typename U,
              typename std::enable_if<
                  std::is_const<T>::value &&
                  std::is_same<typename std::remove_const<T>::type, U>::value,
                  int>::type = 0>
    deque_iterator(const deque_iterator<U, BufSize>& other)
        : cur(other.cur),
          first(other.first),
          last(other.last),
          node((T**)other.node) {}

    // Set the iterator into a new buffer node
    void set_node(T** new_node) {
        node = new_node;
        first = *new_node;
        last = first + BufSize;
    }

    reference operator*() const  { return *cur; }
    pointer operator->() const   { return cur; }

    difference_type operator-(const self& other) const {
        return (difference_type(BufSize) * (node - other.node - 1))
               + (cur - first) + (other.last - other.cur);
    }

    self& operator++() {
        ++cur;
        if (cur == last) {
            set_node(node + 1);
            cur = first;
        }
        return *this;
    }

    self operator++(int) {
        self tmp = *this;
        ++*this;
        return tmp;
    }

    self& operator--() {
        if (cur == first) {
            set_node(node - 1);
            cur = last;
        }
        --cur;
        return *this;
    }

    self operator--(int) {
        self tmp = *this;
        --*this;
        return tmp;
    }

    self& operator+=(difference_type n) {
        if (n != 0) {
            difference_type offset = n + (cur - first);
            if (offset >= 0 && offset < difference_type(BufSize)) {
                cur += n;
            } else {
                // Move to a new buffer
                difference_type node_offset =
                    offset >= 0 ? offset / difference_type(BufSize)
                                : (-difference_type(BufSize) + 1 + offset)
                                  / difference_type(BufSize);
                set_node(node + node_offset);
                cur = first + (offset - node_offset * difference_type(BufSize));
            }
        }
        return *this;
    }

    self& operator-=(difference_type n) {
        return *this += -n;
    }

    self operator+(difference_type n) const {
        self tmp = *this;
        return tmp += n;
    }

    friend self operator+(difference_type n, const self& it) {
        self tmp = it;
        return tmp += n;
    }

    self operator-(difference_type n) const {
        self tmp = *this;
        return tmp -= n;
    }

    reference operator[](difference_type n) const {
        return *(*this + n);
    }

    bool operator==(const self& other) const { return cur == other.cur && node == other.node; }
    bool operator!=(const self& other) const { return !(*this == other); }
    bool operator<(const self& other) const  {
        return (node == other.node) ? (cur < other.cur) : (node < other.node);
    }
    bool operator>(const self& other) const  { return other < *this; }
    bool operator<=(const self& other) const { return !(other < *this); }
    bool operator>=(const self& other) const { return !(*this < other); }
};

} // namespace detail
} // namespace lstl
