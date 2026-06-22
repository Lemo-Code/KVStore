/**
 * @file    map.h
 * @brief   Ordered map backed by red-black tree (unique keys).
 * @author  lstl team
 * @date    2025
 *
 * lstl::map stores key-value pairs sorted by key using a red-black tree.
 * Provides O(log n) insert, find, and erase with bidirectional iteration
 * in sorted key order.
 *
 * @tparam Key      Key type.
 * @tparam T        Mapped value type.
 * @tparam Compare  Key comparison functor (default: lstl::less<Key>).
 * @tparam Alloc    Allocator type.
 *
 * @ingroup container
 */

#pragma once

#include <cstddef>
#include <initializer_list>

#include "../memory/utility.h"
#include "../memory/functional.h"
#include "../memory/allocator.h"
#include "detail/rb_tree.h"
#include "detail/key_of_value.h"

namespace lstl {

/**
 * @brief  Ordered map (red-black tree with unique keys).
 *
 * @tparam Key      Key type (must be LessThanComparable via Compare).
 * @tparam T        Mapped type (default-constructible for operator[]).
 * @tparam Compare  Strict weak ordering on keys.
 * @tparam Alloc    Allocator for pair<const Key, T>.
 */
template <typename Key, typename T,
          typename Compare = lstl::less<Key>,
          typename Alloc = allocator<pair<const Key, T>>>
class map {
public:
    typedef Key                                     key_type;       ///< Key type.
    typedef T                                       mapped_type;    ///< Mapped value type.
    typedef pair<const Key, T>                      value_type;     ///< Key-value pair.
    typedef Compare                                 key_compare;    ///< Key comparison functor.
    typedef Alloc                                   allocator_type; ///< Allocator type.

    /** @brief  Value comparison (compares keys only). */
    class value_compare {
        friend class map;
    protected:
        Compare comp_;
        value_compare(Compare c) : comp_(c) {}
    public:
        bool operator()(const value_type& a, const value_type& b) const {
            return comp_(a.first, b.first);
        }
    };

private:
    typedef detail::rb_tree<key_type, value_type,
            detail::select1st_key<value_type>,
            Compare, Alloc> tree_type;
    tree_type tree_;  ///< Underlying red-black tree.

public:
    typedef detail::rb_tree_iterator<value_type>         iterator;
    typedef detail::rb_tree_iterator<const value_type>   const_iterator;
    typedef size_t                                       size_type;

    // ---- Construction ----
    map() : tree_() {}
    explicit map(const Compare& comp) : tree_(comp) {}
    template <typename InputIterator>
    map(InputIterator first, InputIterator last) : tree_() {
        for (; first != last; ++first) insert(*first);
    }
    map(std::initializer_list<value_type> il) : tree_() {
        for (auto& v : il) insert(v);
    }
    map(const map& other) : tree_(other.tree_) {}
    map(map&& other) noexcept : tree_() { tree_.swap(other.tree_); }
    map& operator=(map&& other) noexcept { tree_.swap(other.tree_); return *this; }

    // ---- Iterators ----
    iterator begin()             { return iterator(tree_.begin_node()); }
    const_iterator begin() const { return const_iterator(tree_.begin_node()); }
    const_iterator cbegin() const { return begin(); }
    iterator end()               { return iterator(tree_.end_node()); }
    const_iterator end() const   { return const_iterator(tree_.end_node()); }

    // ---- Capacity ----
    bool empty() const    { return tree_.empty(); }
    size_type size() const { return tree_.size(); }

    // ---- Element Access ----
    /**
     * @brief  Accesses or creates an element with key @p k.
     * @param  k  Key to look up.
     * @return    Reference to the mapped value. If the key doesn't exist,
     *            a new element is default-constructed and inserted.
     */
    mapped_type& operator[](const key_type& k) {
        auto result = tree_.insert_unique(value_type(k, T()));
        iterator it(result.first);
        return it->second;
    }

    /** @brief  Accesses an element with bounds check. @throws std::out_of_range if not found. */
    mapped_type& at(const key_type& k) {
        auto it = find(k);
        if (it == end()) throw std::out_of_range("map::at");
        return it->second;
    }

    // ---- Modifiers ----
    /**
     * @brief  Inserts a key-value pair (if the key doesn't already exist).
     * @param  v  Pair to insert.
     * @return    pair<iterator, bool>: iterator to the element (existing or new),
     *            and true if insertion occurred, false if the key already existed.
     */
    pair<iterator, bool> insert(const value_type& v) { return tree_.insert_unique(v); }
    pair<iterator, bool> insert(value_type&& v) { return tree_.insert_unique(lstl::move(v)); }

    /** @brief  Erases the element at @p pos. @return Iterator following the erased element. */
    iterator erase(const_iterator pos) {
        auto base = pos.base();
        iterator next(detail::rb_tree_node_base::successor(base));
        tree_.erase(base);
        return next;
    }

    /** @brief  Erases the element with key @p k. @return Number of elements removed (0 or 1). */
    size_type erase(const key_type& k) { return tree_.erase_unique(k); }

    /** @brief  Removes all elements. */
    void clear() { tree_.clear(); }

    // ---- Lookup ----
    /** @brief  Finds an element by key. @return Iterator to the element, or end() if not found. */
    iterator find(const key_type& k) { return iterator(tree_.find(k)); }
    const_iterator find(const key_type& k) const { return const_iterator(const_cast<tree_type&>(tree_).find(k)); }

    /** @brief  Counts elements with key @p k (0 or 1 for map). */
    size_type count(const key_type& k) const { return tree_.count(k); }

    /** @brief  First element not less than @p k. */
    iterator lower_bound(const key_type& k) { return iterator(tree_.lower_bound(k)); }
    /** @brief  First element greater than @p k. */
    iterator upper_bound(const key_type& k) { return iterator(tree_.upper_bound(k)); }

    /** @brief  Swaps contents with another map. */
    void swap(map& other) noexcept { tree_.swap(other.tree_); }
};

} // namespace lstl
