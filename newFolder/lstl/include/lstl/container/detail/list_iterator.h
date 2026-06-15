/**
 * @file    list_iterator.h
 * @brief   Bidirectional iterator for doubly-linked list.
 * @author  lstl team
 * @date    2025
 * @ingroup container_detail
 */
// Use of this source code is governed by a MIT-style license.
//
// list_iterator.h - Bidirectional iterator for doubly-linked list.

#pragma once

#include <iterator>

#include "list_node.h"

namespace lstl {
namespace detail {

////////////////////////////////////////////////////////////////////////////
// list_iterator - Bidirectional iterator for lstl::list
////////////////////////////////////////////////////////////////////////////
template <typename T>
class list_iterator {
public:
    typedef T                           value_type;
    typedef T*                          pointer;
    typedef T&                          reference;
    typedef ptrdiff_t                   difference_type;
    typedef std::bidirectional_iterator_tag iterator_category;

    typedef list_node_base*             base_ptr;
    typedef list_node<T>                node_type;

    list_iterator() : node_(nullptr) {}
    explicit list_iterator(base_ptr n) : node_(n) {}

    // Conversion from iterator to const_iterator
    template <typename U>
    list_iterator(const list_iterator<U>& other,
                  typename std::enable_if<std::is_convertible<U*, T*>::value>::type* = nullptr)
        : node_(other.base()) {}

    reference operator*() const {
        return list_node<T>::value(node_);
    }

    pointer operator->() const {
        return &(operator*());
    }

    list_iterator& operator++() {
        node_ = node_->next;
        return *this;
    }

    list_iterator operator++(int) {
        list_iterator tmp = *this;
        node_ = node_->next;
        return tmp;
    }

    list_iterator& operator--() {
        node_ = node_->prev;
        return *this;
    }

    list_iterator operator--(int) {
        list_iterator tmp = *this;
        node_ = node_->prev;
        return tmp;
    }

    bool operator==(const list_iterator& other) const {
        return node_ == other.node_;
    }

    bool operator!=(const list_iterator& other) const {
        return node_ != other.node_;
    }

    base_ptr base() const { return node_; }

private:
    base_ptr node_;
};

} // namespace detail
} // namespace lstl
