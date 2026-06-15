/**
 * @file    skip_map.h
 * @brief   Ordered map backed by skip list — probabilistic O(log n).
 * @author  lstl team
 * @date    2025
 *
 * lstl::skip_map is an ordered associative container backed by a
 * probabilistic skip list. Provides expected O(log n) search, insert,
 * and erase with simpler implementation than a red-black tree.
 *
 * The skip list uses 32 max levels with 1/4 promotion probability,
 * giving good expected performance with low variance.
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
#include "detail/skip_list.h"
#include "detail/key_of_value.h"

namespace lstl {

/**
 * @brief  Ordered map using skip list (unique keys, sorted iteration).
 *
 * Compared to map (RB-tree), skip_map is:
 * - Simpler to implement and verify.
 * - More cache-friendly for sequential access.
 * - Potentially better for concurrent access (lock-free possible).
 *
 * @tparam Key      Key type.
 * @tparam T        Mapped type (default-constructible for operator[]).
 * @tparam Compare  Strict weak ordering on keys.
 * @tparam Alloc    Allocator for pair<const Key, T>.
 */
template <typename Key, typename T,
          typename Compare = lstl::less<Key>,
          typename Alloc = allocator<pair<const Key, T>>>
class skip_map {
public:
    typedef Key                 key_type;
    typedef T                   mapped_type;
    typedef pair<const Key, T>  value_type;
    typedef Compare             key_compare;
    typedef size_t              size_type;

private:
    typedef detail::skip_list<value_type, key_type,
            detail::select1st_key<value_type>,
            Compare, Alloc> list_type;
    list_type list_;

public:
    typedef detail::skip_list_iterator<value_type>       iterator;
    typedef detail::skip_list_iterator<const value_type> const_iterator;

    skip_map() : list_() {}
    explicit skip_map(const Compare& comp) : list_(comp) {}
    template <typename InputIterator>
    skip_map(InputIterator first, InputIterator last) : list_() {
        for (; first != last; ++first) insert(*first);
    }
    skip_map(std::initializer_list<value_type> il) : list_() {
        for (auto& v : il) insert(v);
    }

    /** @brief  Forward iteration in sorted key order. */
    iterator begin()             { return iterator(list_.begin_node()); }
    const_iterator begin() const { return const_iterator(list_.begin_node()); }
    iterator end()               { return iterator(list_.end_node()); }
    const_iterator end() const   { return const_iterator(list_.end_node()); }

    bool empty() const      { return list_.empty(); }
    size_type size() const  { return list_.size(); }

    /** @brief  Accesses or creates element with key @p k. */
    mapped_type& operator[](const key_type& k) {
        auto result = list_.insert_unique(value_type(k, T()));
        return result.first->value.second;
    }

    /** @brief  Inserts a key-value pair. @return pair<iterator, bool>. */
    pair<iterator, bool> insert(value_type&& v) {
        auto result = list_.insert_unique(lstl::move(v));
        return lstl::make_pair(iterator(result.first), result.second);
    }
    pair<iterator, bool> insert(const value_type& v) {
        auto result = list_.insert_unique(v);
        return lstl::make_pair(iterator(result.first), result.second);
    }

    iterator erase(const_iterator pos)            { list_.erase(pos.base()); }
    size_type erase(const key_type& k)  { return list_.erase_unique(k); }
    void clear()                        { list_.clear(); }

    iterator find(const key_type& k)             { return iterator(list_.find(k)); }
    const_iterator find(const key_type& k) const { return const_iterator(list_.find(k)); }
    size_type count(const key_type& k) const     { return list_.count(k); }

    void swap(skip_map& other) noexcept { list_.swap(other.list_); }
};

} // namespace lstl
