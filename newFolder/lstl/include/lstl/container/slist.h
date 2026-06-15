/**
 * @file    slist.h
 * @brief   Singly-linked list. Forward-only iteration, lower memory overhead (one pointer per node).
 * @author  lstl team
 * @date    2025
 * @ingroup container
 */

#pragma once

#include <cstddef>
#include <iterator>
#include <initializer_list>

#include "../memory/utility.h"
#include "../memory/construct.h"
#include "../memory/allocator.h"
#include "../memory/alloc.h"
#include "../memory/pool.h"
#include "detail/slist_node.h"

namespace lstl {

template <typename T, typename Alloc = allocator<T>>
class slist {
public:
    typedef T                                           value_type;
    typedef T*                                          pointer;
    typedef const T*                                    const_pointer;
    typedef T&                                          reference;
    typedef const T&                                    const_reference;
    typedef size_t                                      size_type;
    typedef ptrdiff_t                                   difference_type;
    typedef Alloc                                       allocator_type;

private:
    typedef detail::slist_node_base                     node_base;
    typedef detail::slist_node<T>                       node_type;
    typedef simple_alloc<node_type, default_alloc>      node_allocator;

    node_base head_;  // sentinel node before first element

    node_type* create_node(const T& value) {
        node_type* node = node_allocator::allocate();
        try {
            construct(&node->data, value);
            node->next = nullptr;
        } catch (...) {
            node_allocator::deallocate(node);
            throw;
        }
        return node;
    }

    void destroy_node(node_type* node) {
        destroy(&node->data);
        node_allocator::deallocate(node);
    }

public:
    // Forward iterator
    class iterator {
    public:
        typedef T                             value_type;
        typedef T&                            reference;
        typedef T*                            pointer;
        typedef ptrdiff_t                     difference_type;
        typedef std::forward_iterator_tag     iterator_category;

        iterator() : node_(nullptr) {}
        explicit iterator(node_base* n) : node_(n) {}

        reference operator*() const { return node_type::value(node_); }
        pointer operator->() const { return &node_type::value(node_); }

        iterator& operator++() { node_ = node_->next; return *this; }
        iterator operator++(int) { iterator tmp = *this; node_ = node_->next; return tmp; }

        bool operator==(const iterator& o) const { return node_ == o.node_; }
        bool operator!=(const iterator& o) const { return node_ != o.node_; }

        node_base* base() const { return node_; }

    private:
        node_base* node_;
    };

    // Proper const iterator (for const-correct iteration)
    class const_iterator {
    public:
        typedef const T                       value_type;
        typedef const T&                      reference;
        typedef const T*                      pointer;
        typedef ptrdiff_t                     difference_type;
        typedef std::forward_iterator_tag     iterator_category;

        const_iterator() : node_(nullptr) {}
        explicit const_iterator(const node_base* n) : node_(n) {}
        const_iterator(const iterator& it) : node_(it.base()) {}

        reference operator*() const { return node_type::value(const_cast<node_base*>(node_)); }
        pointer operator->() const { return &node_type::value(const_cast<node_base*>(node_)); }

        const_iterator& operator++() { node_ = node_->next; return *this; }
        const_iterator operator++(int) { const_iterator tmp = *this; node_ = node_->next; return tmp; }

        bool operator==(const const_iterator& o) const { return node_ == o.node_; }
        bool operator!=(const const_iterator& o) const { return node_ != o.node_; }

    private:
        const node_base* node_;
    };

    slist() {}

    explicit slist(size_type n, const T& value = T()) {
        for (size_type i = 0; i < n; ++i) push_front(value);
    }

    slist(std::initializer_list<T> il) {
        // Insert at front reverses order, so insert in reverse
        auto it = il.end();
        while (it != il.begin()) {
            --it;
            push_front(*it);
        }
    }

    template <typename InputIterator, typename = typename enable_if<!is_integral<InputIterator>::value>::type>
    slist(InputIterator first, InputIterator last) {
        // Build in reverse to maintain order
        slist tmp;
        for (; first != last; ++first) tmp.push_front(*first);
        // Now reverse from tmp
        node_base* cur = tmp.head_.next;
        while (cur) {
            push_front(node_type::value(cur));
            cur = cur->next;
        }
    }

    slist(const slist& other) {
        node_base* cur = other.head_.next;
        while (cur) {
            push_front(node_type::value(cur));
            cur = cur->next;
        }
        reverse();
    }

    ~slist() { clear(); }

    slist& operator=(const slist& other) {
        if (this != &other) { slist tmp(other); swap(tmp); }
        return *this;
    }

    iterator begin() { return iterator(head_.next); }
    const_iterator begin() const { return const_iterator(head_.next); }
    const_iterator cbegin() const { return begin(); }
    iterator end() { return iterator(nullptr); }
    const_iterator end() const { return iterator(nullptr); }
    const_iterator cend() const { return end(); }

    bool empty() const { return head_.next == nullptr; }

    size_type size() const {
        return detail::slist_size(head_.next);
    }

    reference front() { return node_type::value(head_.next); }
    const_reference front() const { return node_type::value(head_.next); }

    void push_front(const T& value) {
        node_type* node = create_node(value);
        detail::slist_insert_after(&head_, node);
    }

    void pop_front() {
        node_base* first = head_.next;
        if (first) {
            head_.next = first->next;
            destroy_node(static_cast<node_type*>(first));
        }
    }

    // Insert after position
    iterator insert_after(const_iterator pos, T&& value) {
        node_type* node = create_node(lstl::move(value));
        detail::slist_insert_after(pos.base(), node);
        return iterator(node);
    }
    iterator insert_after(const_iterator pos, const T& value) {
        node_type* node = create_node(value);
        detail::slist_insert_after(pos.base(), node);
        return iterator(node);
    }

    // Erase after position
    iterator erase_after(const_iterator pos) {
        node_base* n = pos.base();
        node_base* to_erase = n->next;
        if (to_erase) {
            n->next = to_erase->next;
            destroy_node(static_cast<node_type*>(to_erase));
        }
        return iterator(n->next);
    }

    void clear() {
        node_base* cur = head_.next;
        while (cur) {
            node_base* next = cur->next;
            destroy_node(static_cast<node_type*>(cur));
            cur = next;
        }
        head_.next = nullptr;
    }

    void reverse() {
        head_.next = detail::slist_reverse(head_.next);
    }

    void swap(slist& other) noexcept {
        lstl::swap(head_.next, other.head_.next);
    }
};

} // namespace lstl
