/**
 * @file    priority_queue.h
 * @brief   Priority queue adapter (max-heap by default). Default container: vector.
 * @author  lstl team
 * @date    2025
 * @ingroup container
 */

#pragma once
#include <cstddef>
#include "../memory/utility.h"
#include "../memory/functional.h"
#include "vector.h"
#include "detail/heap.h"

namespace lstl {

template <typename T, typename Container = vector<T>,
          typename Compare = lstl::less<typename Container::value_type>>
class priority_queue {
public:
    typedef typename Container::value_type       value_type;
    typedef typename Container::reference        reference;
    typedef typename Container::const_reference  const_reference;
    typedef typename Container::size_type        size_type;
    typedef Container                            container_type;

protected:
    Container c_;
    Compare   comp_;

public:
    priority_queue() : c_(), comp_() {}
    explicit priority_queue(const Compare& comp) : c_(), comp_(comp) {}
    priority_queue(priority_queue&& other) noexcept : c_(lstl::move(other.c_)), comp_(lstl::move(other.comp_)) {}

    template <typename InputIterator>
    priority_queue(InputIterator first, InputIterator last,
                   const Compare& comp = Compare())
        : c_(first, last), comp_(comp) {
        detail::make_heap(c_.begin(), c_.end(), comp_);
    }

    bool empty() const { return c_.empty(); }
    size_type size() const { return c_.size(); }

    const_reference top() const { return c_.front(); }

    void push(const value_type& x) {
        c_.push_back(x);
        detail::push_heap(c_.begin(), c_.end(), comp_);
    }

    void pop() {
        detail::pop_heap(c_.begin(), c_.end(), comp_);
        c_.pop_back();
    }

    void swap(priority_queue& other) noexcept {
        lstl::swap(c_, other.c_);
        lstl::swap(comp_, other.comp_);
    }
};

} // namespace lstl
