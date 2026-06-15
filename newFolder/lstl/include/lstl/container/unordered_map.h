/**
 * @file    unordered_map.h
 * @brief   Hash map with unique keys — average O(1) operations.
 * @author  lstl team
 * @date    2025
 *
 * lstl::unordered_map stores key-value pairs in a hash table with
 * separate chaining. Provides average O(1) insert, find, and erase.
 * Uses prime-number bucket counts with FNV-1a hashing.
 *
 * @tparam Key       Key type.
 * @tparam T         Mapped value type.
 * @tparam Hash      Hash functor (default: lstl::hash<Key>).
 * @tparam KeyEqual  Equality comparison (default: lstl::equal_to<Key>).
 * @tparam Alloc     Allocator type.
 *
 * @ingroup container
 */

#pragma once

#include <cstddef>
#include <initializer_list>

#include "../memory/utility.h"
#include "../memory/functional.h"
#include "../memory/allocator.h"
#include "detail/hashtable.h"
#include "detail/key_of_value.h"

namespace lstl {

/**
 * @brief  Hash map (unique keys, separate chaining).
 *
 * Elements are NOT stored in any particular order. Iteration order
 * depends on bucket layout and may change after rehash.
 *
 * @tparam Key       Key type (must be hashable and equality-comparable).
 * @tparam T         Mapped type (default-constructible for operator[]).
 * @tparam Hash      Hash function object.
 * @tparam KeyEqual  Key equality predicate.
 * @tparam Alloc     Allocator for pair<const Key, T>.
 */
template <typename Key, typename T,
          typename Hash = lstl::hash<Key>,
          typename KeyEqual = lstl::equal_to<Key>,
          typename Alloc = allocator<pair<const Key, T>>>
class unordered_map {
public:
    typedef Key                 key_type;       ///< Key type.
    typedef T                   mapped_type;    ///< Mapped value type.
    typedef pair<const Key, T>  value_type;     ///< Key-value pair.
    typedef Hash                hasher;         ///< Hash functor type.
    typedef KeyEqual            key_equal;      ///< Equality predicate type.
    typedef Alloc               allocator_type; ///< Allocator type.
    typedef size_t              size_type;      ///< Size type.

private:
    typedef detail::hashtable<value_type, key_type,
            detail::select1st_key<value_type>,
            Hash, KeyEqual, Alloc> table_type;
    table_type table_;

public:
    typedef detail::hashtable_iterator<value_type>        iterator;
    typedef detail::hashtable_iterator<const value_type>  const_iterator;

    // ---- Construction ----
    unordered_map() : table_() {}
    template <typename InputIterator, typename = typename enable_if<!is_integral<InputIterator>::value>::type>
    unordered_map(InputIterator first, InputIterator last) : table_() {
        for (; first != last; ++first) insert(*first);
    }
    unordered_map(std::initializer_list<value_type> il) : table_() {
        for (auto& v : il) insert(v);
    }

    // ---- Iterators ----
    iterator begin()             { return table_.begin(); }
    const_iterator begin() const { return table_.begin(); }
    iterator end()               { return table_.end(); }
    const_iterator end() const   { return table_.end(); }

    // ---- Capacity ----
    bool empty() const      { return table_.empty(); }
    size_type size() const  { return table_.size(); }

    /** @brief  Accesses or creates an element with key @p k. */
    mapped_type& operator[](const key_type& k) {
        auto result = table_.insert_unique(value_type(k, T()));
        return result.first->second;
    }

    /** @brief  Accesses with bounds check. @throws std::out_of_range if not found. */
    mapped_type& at(const key_type& k) {
        auto it = find(k);
        if (it == end()) throw std::out_of_range("unordered_map::at");
        return it->second;
    }

    /** @brief  Inserts a key-value pair. @return pair<iterator, bool> (iterator to element, true if inserted). */
    pair<iterator, bool> insert(const value_type& v) { return table_.insert_unique(v); }

    /** @brief  Erases the element at @p pos. @return end() (element erased, iterator invalidated). */
    iterator erase(const_iterator pos) { auto it = find(pos->first); if (it != end()) { table_.erase(it); } return end(); }

    /** @brief  Erases by key. @return Number of elements removed (0 or 1). */
    size_type erase(const key_type& k) { return table_.erase_unique(k); }

    void clear() { table_.clear(); }

    /** @brief  Finds an element by key. @return Iterator to element or end(). */
    iterator find(const key_type& k)       { return table_.find(k); }
    const_iterator find(const key_type& k) const { return table_.find(k); }

    size_type count(const key_type& k) const { return table_.count(k); }
    size_type bucket_count() const { return table_.bucket_count(); }

    void swap(unordered_map& other) noexcept { table_.swap(other.table_); }
};

} // namespace lstl
