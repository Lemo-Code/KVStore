/**
 * @file    unordered_multiset.h
 * @brief   Hash multiset allowing duplicate keys. Average O(1) operations.
 * @author  lstl team
 * @date    2025
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

template <typename Key,
          typename Hash = lstl::hash<Key>,
          typename KeyEqual = lstl::equal_to<Key>,
          typename Alloc = allocator<Key>>
class unordered_multiset {
public:
    typedef Key         key_type;
    typedef Key         value_type;
    typedef Hash        hasher;
    typedef KeyEqual    key_equal;
    typedef size_t      size_type;

private:
    typedef detail::hashtable<value_type, key_type,
            detail::identity_key<value_type>,
            Hash, KeyEqual, Alloc> table_type;
    table_type table_;

public:
    typedef detail::hashtable_iterator<value_type> iterator;
    typedef detail::hashtable_iterator<const value_type> const_iterator;

    unordered_multiset() : table_() {}
    // move
    decltype(*this)(decltype(*this)&& other) noexcept : table_() { table_.swap(other.table_); }
    decltype(*this)& operator=(decltype(*this)&& other) noexcept { table_.swap(other.table_); return *this; }

    template <typename InputIterator, typename = typename enable_if<!is_integral<InputIterator>::value>::type>
    unordered_multiset(InputIterator first, InputIterator last) : table_() {
        for (; first != last; ++first) insert(*first);
    }

    unordered_multiset(std::initializer_list<value_type> il) : table_() {
        for (auto& v : il) insert(v);
    }

    iterator begin() { return table_.begin(); }
    const_iterator begin() const { return table_.begin(); }
    iterator end() { return table_.end(); }
    const_iterator end() const { return table_.end(); }

    bool empty() const { return table_.empty(); }
    size_type size() const { return table_.size(); }

    iterator insert(const value_type& v) { return table_.insert_equal(v); }

    iterator erase(const_iterator pos) {
        auto it = find(*pos);
        if (it != end()) { table_.erase(it); }
        return end();
    }

    size_type erase(const key_type& k) {
        size_type n = 0;
        while (true) {
            auto it = table_.find(k);
            if (it == end()) break;
            table_.erase(it);
            ++n;
        }
        return n;
    }

    void clear() { table_.clear(); }

    iterator find(const key_type& k) { return table_.find(k); }
    const_iterator find(const key_type& k) const { return table_.find(k); }
    size_type count(const key_type& k) const { return table_.count(k); }
    size_type bucket_count() const { return table_.bucket_count(); }

    void swap(unordered_multiset& other) noexcept { table_.swap(other.table_); }
};

} // namespace lstl
