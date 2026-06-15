/**
 * @file    queue.h
 * @brief   Queue adapter (FIFO). Default container: deque.
 * @author  lstl team
 * @date    2025
 * @ingroup container
 */

#pragma once

#include <cstddef>
#include "../memory/utility.h"
#include "deque.h"

namespace lstl {

template <typename T, typename Container = deque<T>>
class queue {
public:
    typedef typename Container::value_type       value_type;
    typedef typename Container::reference        reference;
    typedef typename Container::const_reference  const_reference;
    typedef typename Container::size_type        size_type;
    typedef Container                            container_type;

public:
    Container c_;

public:
    queue() : c_() {}
    explicit queue(const Container& c) : c_(c) {}

    bool empty() const { return c_.empty(); }
    size_type size() const { return c_.size(); }

    reference front() { return c_.front(); }
    const_reference front() const { return c_.front(); }
    reference back() { return c_.back(); }
    const_reference back() const { return c_.back(); }

    void push(const value_type& x) { c_.push_back(x); }
    void push(value_type&& x) { c_.push_back(lstl::move(x)); }
    void pop() { c_.pop_front(); }

    void swap(queue& other) noexcept { c_.swap(other.c_); }
};

} // namespace lstl

template <typename T, typename C>
bool operator==(const queue<T,C>& a, const queue<T,C>& b) { return a.c_ == b.c_; }
template <typename T, typename C>
bool operator!=(const queue<T,C>& a, const queue<T,C>& b) { return !(a == b); }
template <typename T, typename C>
bool operator<(const queue<T,C>& a, const queue<T,C>& b) { return a.c_ < b.c_; }
template <typename T, typename C>
bool operator<=(const queue<T,C>& a, const queue<T,C>& b) { return !(b < a); }
template <typename T, typename C>
bool operator>(const queue<T,C>& a, const queue<T,C>& b) { return b < a; }
template <typename T, typename C>
bool operator>=(const queue<T,C>& a, const queue<T,C>& b) { return !(a < b); }
