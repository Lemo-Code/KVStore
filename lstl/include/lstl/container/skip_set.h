/**
 * @file    skip_set.h
 * @brief   Ordered set backed by skip list — probabilistic O(log n).
 * @author  lstl team
 * @date    2025
 *
 * lstl::skip_set stores unique keys in sorted order using a skip list.
 * Provides expected O(log n) search, insert, and erase.
 *
 * @tparam Key      Key type.
 * @tparam Compare  Comparison functor.
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
 * @brief  Ordered set using skip list (unique keys, sorted iteration).
 *
 * @tparam Key      Key type.
 * @tparam Compare  Strict weak ordering on keys.
 * @tparam Alloc    Allocator for Key.
 */
template <typename Key,
          typename Compare = lstl::less<Key>,
          typename Alloc = allocator<Key>>
class skip_set {
public:
    typedef Key      key_type;
    typedef Key      value_type;
    typedef Compare  key_compare;
    typedef size_t   size_type;

private:
    typedef detail::skip_list<value_type, key_type,
            detail::identity_key<value_type>,
            Compare, Alloc> list_type;
    list_type list_;

public:
    typedef detail::skip_list_iterator<value_type>       iterator;
    typedef detail::skip_list_iterator<const value_type> const_iterator;

    skip_set() : list_() {}
    explicit skip_set(const Compare& comp) : list_(comp) {}
    template <typename InputIterator>
    skip_set(InputIterator first, InputIterator last) : list_() {
        for (; first != last; ++first) insert(*first);
    }
    skip_set(std::initializer_list<value_type> il) : list_() {
        for (auto& v : il) insert(v);
    }

    iterator begin()             { return iterator(list_.begin_node()); }
    const_iterator begin() const { return const_iterator(list_.begin_node()); }
    iterator end()               { return iterator(list_.end_node()); }
    const_iterator end() const   { return const_iterator(list_.end_node()); }

    bool empty() const      { return list_.empty(); }
    size_type size() const  { return list_.size(); }

    pair<iterator, bool> insert(const value_type& v) {
        auto result = list_.insert_unique(v);
        return lstl::make_pair(iterator(result.first), result.second);
    }

    void erase(iterator pos)            { list_.erase(pos.base()); }
    size_type erase(const key_type& k)  { return list_.erase_unique(k); }
    void clear()                        { list_.clear(); }

    iterator find(const key_type& k)             { return iterator(list_.find(k)); }
    const_iterator find(const key_type& k) const { return const_iterator(list_.find(k)); }
    size_type count(const key_type& k) const     { return list_.count(k); }

    void swap(skip_set& other) noexcept { list_.swap(other.list_); }
};

} // namespace lstl
