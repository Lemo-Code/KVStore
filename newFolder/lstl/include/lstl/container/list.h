/**
 * @file    list.h
 * @brief   Doubly-linked list with O(1) insert/erase anywhere.
 * @author  lstl team
 * @date    2025
 *
 * lstl::list is a doubly-linked list providing:
 * - O(1) insert/erase at any position (given an iterator).
 * - Bidirectional iteration.
 * - O(n) search and random access.
 * - Splice, reverse, unique, and remove operations.
 *
 * The sentinel node design eliminates special-casing for begin/end.
 *
 * @tparam T     Element type.
 * @tparam Alloc Allocator type.
 *
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
#include "detail/list_node.h"
#include "detail/list_iterator.h"
#include "detail/reverse_iterator.h"

namespace lstl {

/**
 * @brief  Doubly-linked list.
 *
 * Elements are stored in individually allocated nodes connected by
 * prev/next pointers. Insert and erase at any position are O(1).
 *
 * @tparam T     Element type.
 * @tparam Alloc Allocator type (default: lstl::allocator<T>).
 *
 * @note  Iterators remain valid after insert/erase of other elements.
 * @note  Pointers to elements remain valid unless the element is erased.
 */
template <typename T, typename Alloc = allocator<T>>
class list {
public:
    // ---- Standard typedefs ----
    typedef T                                           value_type;
    typedef T*                                          pointer;
    typedef const T*                                    const_pointer;
    typedef T&                                          reference;
    typedef const T&                                    const_reference;
    typedef size_t                                      size_type;
    typedef ptrdiff_t                                   difference_type;

    typedef detail::list_iterator<T>                    iterator;
    typedef detail::list_iterator<const T>              const_iterator;
    typedef detail::reverse_iterator<iterator>          reverse_iterator;
    typedef detail::reverse_iterator<const_iterator>    const_reverse_iterator;
    typedef Alloc                                       allocator_type;

private:
    typedef detail::list_node_base                      node_base;
    typedef detail::list_node<T>                        node_type;
    typedef simple_alloc<node_type, default_alloc>      node_allocator;

    node_base sentinel_;  ///< Sentinel node
    size_type count_;       ///< Cached element count: sentinel_.next=head, sentinel_.prev=tail

    /** @brief  Allocates and constructs a new node. */
    node_type* create_node(const T& value) {
        node_type* node = node_allocator::allocate();
        try {
            construct(&node->data, value);
        } catch (...) {
            node_allocator::deallocate(node);
            throw;
        }
        return node;
    }

    /** @brief  Destroys a node and returns its memory. */
    void destroy_node(node_type* node) {
        destroy(&node->data);
        node_allocator::deallocate(node);
    }

public:
    // ---- Construction ----
    list() : sentinel_(), count_(0) {}

    /** @brief  Constructs with @p n copies of @p value. */
    explicit list(size_type n, const T& value = T()) : sentinel_() {
        for (size_type i = 0; i < n; ++i) push_back(value);
    }

    /** @brief  Constructs from an iterator range. */
    template <typename InputIterator, typename = typename enable_if<!is_integral<InputIterator>::value>::type>
    list(InputIterator first, InputIterator last) : sentinel_() {
        for (; first != last; ++first) push_back(*first);
    }

    /** @brief  Constructs from an initializer list. */
    list(std::initializer_list<T> il) : sentinel_() {
        for (auto& x : il) push_back(x);
    }

    /** @brief  Copy constructor. */
    list(const list& other) : sentinel_() {
        for (auto& x : other) push_back(x);
    }

    /** @brief  Move constructor — transfers nodes in O(1). */
    list(list&& other) noexcept : sentinel_() {
        if (!other.empty()) {
            sentinel_.next = other.sentinel_.next;
            sentinel_.prev = other.sentinel_.prev;
            sentinel_.next->prev = &sentinel_;
            sentinel_.prev->next = &sentinel_;
            other.sentinel_.next = other.sentinel_.prev = &other.sentinel_;
        count_ = other.count_; other.count_ = 0;
        }
    }

    /** @brief  Destructor — destroys all nodes. */
    ~list() { clear(); }

    /** @brief  Copy assignment (copy-swap idiom). */
    list& operator=(const list& other) {
        if (this != &other) { list tmp(other); swap(tmp); }
        return *this;
    }
    /** @brief  Move assignment. */
    list& operator=(list&& other) noexcept { swap(other); return *this; }

    // ---- Element Access ----
    reference front() { return *begin(); }
    const_reference front() const { return *begin(); }
    reference back() { return *(--end()); }
    const_reference back() const { return *(--end()); }

    // ---- Iterators ----
    iterator begin()             { return iterator(sentinel_.next); }
    const_iterator begin() const { return const_iterator(sentinel_.next); }
    const_iterator cbegin() const { return begin(); }
    iterator end()               { return iterator(&sentinel_); }
    const_iterator end() const   { return const_iterator(const_cast<node_base*>(&sentinel_)); }

    reverse_iterator rbegin() { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    reverse_iterator rend() { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }

    // ---- Capacity ----
    bool empty() const { return sentinel_.next == &sentinel_; }
    size_type size() const { return count_; }

    // ---- Modifiers ----
    void push_front(const T& value) { node_type* n = create_node(value); n->hook(sentinel_.next); ++count_; }
    void push_back(const T& value)  { node_type* n = create_node(value); n->hook(&sentinel_); ++count_; }
    void pop_front() { erase(begin()); }
    void pop_back()  { erase(--end()); }

    /**
     * @brief  Inserts @p value before @p pos. O(1).
     * @param  pos    Iterator before which to insert.
     * @param  value  Value to insert.
     * @return        Iterator to the newly inserted element.
     */
    iterator insert(const_iterator pos, const T& value) {
        node_type* node = create_node(value); node->hook(pos.base()); ++count_; return iterator(node);
    }
    iterator insert(const_iterator pos, T&& value) {
        node_type* node = create_node(lstl::move(value)); node->hook(pos.base()); ++count_; return iterator(node);
    }

    /**
     * @brief  Erases the element at @p pos. O(1).
     * @param  pos  Iterator to the element to erase.
     * @return      Iterator following the erased element.
     */
    iterator erase(const_iterator pos) {
        node_base* n = pos.base();
        iterator next(n->next);
        n->unhook();
        destroy_node(static_cast<node_type*>(n));
        --count_;
        return next;
    }

    /** @brief  Destroys all nodes. O(n). */
    void clear() {
        node_base* cur = sentinel_.next;
        while (cur != &sentinel_) {
            node_type* n = static_cast<node_type*>(cur);
            cur = cur->next;
            destroy_node(n);
        }
        sentinel_.next = sentinel_.prev = &sentinel_; count_ = 0; }

    void resize(size_type n, const T& value = T()) {
        size_type sz = size();
        if (n < sz) { while (sz > n) { pop_back(); --sz; } }
        else { while (sz < n) { push_back(value); ++sz; } }
    }

    void swap(list& other) noexcept {
        lstl::swap(sentinel_.next, other.sentinel_.next);
        lstl::swap(sentinel_.prev, other.sentinel_.prev);
    }

    // ---- List-specific operations ----

    /** @brief  Transfers all elements from @p other before @p pos. O(1). */
    void splice(const_iterator pos, list& other) {
        if (!other.empty()) {
            node_base::transfer(other.sentinel_.next, &other.sentinel_, pos.base());
        }
    }

    /** @brief  Transfers the element at @p it from @p other before @p pos. */
    void splice(const_iterator pos, list&, const_iterator it) {
        node_base* n = it.base();
        n->unhook();
        n->hook(pos.base());
    }

    /** @brief  Removes all elements equal to @p value. O(n). */
    void remove(const T& value) {
        iterator it = begin();
        while (it != end()) {
            iterator next = it; ++next;
            if (*it == value) erase(it);
            it = next;
        }
    }

    /** @brief  Reverses the order of elements in-place. O(n). */
    void reverse() { node_base::reverse(&sentinel_); }

    /** @brief  Removes consecutive duplicate elements. O(n). */
    void unique() {
        if (empty()) return;
        iterator first = begin(), last = end(), next = first;
        while (++next != last) {
            if (*first == *next) { erase(next); next = first; }
            else { first = next; }
        }
    }
};

} // namespace lstl
