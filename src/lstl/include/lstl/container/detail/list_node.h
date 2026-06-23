/**
 * @file    list_node.h
 * @brief   Doubly-linked list node type with sentinel-based hook/unhook operations.
 * @author  lstl team
 * @date    2025
 * @ingroup container_detail
 */
// Use of this source code is governed by a MIT-style license.
//
// list_node.h - Doubly-linked list node type.

#pragma once

#include <cstddef>

namespace lstl {
namespace detail {

////////////////////////////////////////////////////////////////////////////
// list_node - Node for doubly-linked list
// Stores prev and next pointers.
////////////////////////////////////////////////////////////////////////////
struct list_node_base {
    list_node_base* next;
    list_node_base* prev;

    list_node_base() : next(this), prev(this) {}

    // Insert this node before `pos`
    void hook(list_node_base* pos) {
        this->next = pos;
        this->prev = pos->prev;
        pos->prev->next = this;
        pos->prev = this;
    }

    // Remove this node from the list
    void unhook() {
        this->prev->next = this->next;
        this->next->prev = this->prev;
    }

    // Transfer range [first, last) before `pos`
    static void transfer(list_node_base* first, list_node_base* last,
                         list_node_base* pos) {
        if (first == last) return;

        // Save the part being moved
        list_node_base* first_prev = first->prev;
        list_node_base* last_prev = last->prev;

        // Remove [first, last) from original list
        first_prev->next = last;
        last->prev = first_prev;

        // Splice into new position before pos
        last_prev->next = pos;
        first->prev = pos->prev;
        pos->prev->next = first;
        pos->prev = last_prev;
    }

    // Reverse the list (swap prev/next for each node)
    static void reverse(list_node_base* sentinel) {
        list_node_base* cur = sentinel;
        do {
            list_node_base* tmp = cur->next;
            cur->next = cur->prev;
            cur->prev = tmp;
            cur = tmp;
        } while (cur != sentinel);
    }

    size_t size() const {
        size_t n = 0;
        const list_node_base* cur = this->next;
        while (cur != this) {
            ++n;
            cur = cur->next;
        }
        return n;
    }
};

// Templated node that holds a value
template <typename T>
struct list_node : public list_node_base {
    T data;

    template <typename... Args>
    list_node(Args&&... args) : data(std::forward<Args>(args)...) {}

    // Access data from base pointer
    static T& value(list_node_base* n) {
        return static_cast<list_node<T>*>(n)->data;
    }

    static const T& value(const list_node_base* n) {
        return static_cast<const list_node<T>*>(n)->data;
    }

    static list_node<T>* from_base(list_node_base* n) {
        return static_cast<list_node<T>*>(n);
    }
};

} // namespace detail
} // namespace lstl
