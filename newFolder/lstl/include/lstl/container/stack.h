/**
 * @file    stack.h
 * @brief   Stack adapter (LIFO). Default container: deque.
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
class stack {
public:
    typedef typename Container::value_type       value_type;
    typedef typename Container::reference        reference;
    typedef typename Container::const_reference  const_reference;
    typedef typename Container::size_type        size_type;
    typedef Container                            container_type;

protected:
    Container c_;

public:
    stack() : c_() {}
    explicit stack(const Container& c) : c_(c) {}

    bool empty() const { return c_.empty(); }
    size_type size() const { return c_.size(); }

    reference top() { return c_.back(); }
    const_reference top() const { return c_.back(); }

    void push(const value_type& x) { c_.push_back(x); }
    void push(value_type&& x) { c_.push_back(lstl::move(x)); }
    void pop() { c_.pop_back(); }

    void swap(stack& other) noexcept { c_.swap(other.c_); }

    template <typename... Args>
    void emplace(Args&&... args) {
        c_.emplace_back(std::forward<Args>(args)...);
    }
};

} // namespace lstl
