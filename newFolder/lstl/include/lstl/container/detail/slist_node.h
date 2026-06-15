/**
 * @file    slist_node.h
 * @brief   Singly-linked list node type for forward-only traversal.
 * @author  lstl team
 * @date    2025
 * @ingroup container_detail
 */
// Use of this source code is governed by a MIT-style license.
//
// slist_node.h - Singly-linked list node type.

#pragma once

#include <cstddef>

namespace lstl {
namespace detail {

////////////////////////////////////////////////////////////////////////////
// slist_node_base - Base type for singly-linked list nodes
// Only has a next pointer.
////////////////////////////////////////////////////////////////////////////
struct slist_node_base {
    slist_node_base* next;

    slist_node_base() : next(nullptr) {}
};

// Insert `n` after `pos`
inline void slist_insert_after(slist_node_base* pos, slist_node_base* n) {
    n->next = pos->next;
    pos->next = n;
}

// Reverse the list
inline slist_node_base* slist_reverse(slist_node_base* head) {
    slist_node_base* prev = nullptr;
    slist_node_base* cur = head;
    while (cur) {
        slist_node_base* next = cur->next;
        cur->next = prev;
        prev = cur;
        cur = next;
    }
    return prev;
}

// Count nodes (O(n))
inline size_t slist_size(const slist_node_base* head) {
    size_t n = 0;
    while (head) {
        ++n;
        head = head->next;
    }
    return n;
}

// Templated node that holds a value
template <typename T>
struct slist_node : public slist_node_base {
    T data;

    template <typename... Args>
    slist_node(Args&&... args) : data(std::forward<Args>(args)...) {}

    static T& value(slist_node_base* n) {
        return static_cast<slist_node<T>*>(n)->data;
    }

    static const T& value(const slist_node_base* n) {
        return static_cast<const slist_node<T>*>(n)->data;
    }
};

} // namespace detail
} // namespace lstl
