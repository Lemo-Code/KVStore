/**
 * @file    unordered_set.h
 * @brief   Hash set with unique keys — average O(1) operations.
 * @author  lstl team
 * @date    2025
 *
 * lstl::unordered_set stores unique keys in a hash table with
 * separate chaining. Provides average O(1) insert, find, and erase.
 *
 * @tparam Key       Key type.
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
 * @brief  Hash set (unique keys, separate chaining).
 * @tparam Key       Key type (hashable and equality-comparable).
 * @tparam Hash      Hash function object.
 * @tparam KeyEqual  Key equality predicate.
 * @tparam Alloc     Allocator for Key.
 */
template <typename Key,
          typename Hash = lstl::hash<Key>,
          typename KeyEqual = lstl::equal_to<Key>,
          typename Alloc = allocator<Key>>
class unordered_set {
public:
    typedef Key      key_type;       ///< Key type (also value_type).
    typedef Key      value_type;     ///< Value type (same as key for set).
    typedef Hash     hasher;         ///< Hash functor type.
    typedef KeyEqual key_equal;      ///< Equality predicate type.
    typedef Alloc    allocator_type; ///< Allocator type.
    typedef size_t   size_type;      ///< Size type.

private:
    typedef detail::hashtable<value_type, key_type,
            detail::identity_key<value_type>,
            Hash, KeyEqual, Alloc> table_type;
    table_type table_;

public:
    typedef detail::hashtable_iterator<value_type>        iterator;
    typedef detail::hashtable_iterator<const value_type>  const_iterator;

    unordered_set() : table_() {}
    template <typename InputIterator, typename = typename enable_if<!is_integral<InputIterator>::value>::type>
    unordered_set(InputIterator first, InputIterator last) : table_() {
        for (; first != last; ++first) insert(*first);
    }
    unordered_set(std::initializer_list<value_type> il) : table_() {
        for (auto& v : il) insert(v);
    }

    iterator begin()             { return table_.begin(); }
    const_iterator begin() const { return table_.begin(); }
    iterator end()               { return table_.end(); }
    const_iterator end() const   { return table_.end(); }

    bool empty() const      { return table_.empty(); }
    size_type size() const  { return table_.size(); }

    /** @brief  Inserts a key. @return pair<iterator, bool> (iterator to key, true if inserted). */
    pair<iterator, bool> insert(const value_type& v) { return table_.insert_unique(v); }

    iterator erase(const_iterator pos) {
        auto it = find(*pos); if (it != end()) { table_.erase(it); } return end();
    }
    size_type erase(const key_type& k) { return table_.erase_unique(k); }
    void clear() { table_.clear(); }

    iterator find(const key_type& k)       { return table_.find(k); }
    const_iterator find(const key_type& k) const { return table_.find(k); }
    size_type count(const key_type& k) const { return table_.count(k); }
    size_type bucket_count() const { return table_.bucket_count(); }

    void swap(unordered_set& other) noexcept { table_.swap(other.table_); }
};

} // namespace lstl
