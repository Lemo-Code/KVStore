/**
 * @file    multimap.h
 * @brief   Ordered multimap backed by red-black tree. Allows duplicate keys.
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
#include "detail/rb_tree.h"
#include "detail/key_of_value.h"

namespace lstl {

template <typename Key, typename T,
          typename Compare = lstl::less<Key>,
          typename Alloc = allocator<pair<const Key, T>>>
class multimap {
public:
    typedef Key                                     key_type;
    typedef T                                       mapped_type;
    typedef pair<const Key, T>                      value_type;
    typedef Compare                                 key_compare;
    typedef Alloc                                   allocator_type;

    class value_compare {
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
    tree_type tree_;

public:
    typedef detail::rb_tree_iterator<value_type>         iterator;
    typedef detail::rb_tree_iterator<const value_type>   const_iterator;
    typedef size_t                                       size_type;

    multimap() : tree_() {}
    explicit multimap(const Compare& comp) : tree_(comp) {}

    template <typename InputIterator, typename = typename enable_if<!is_integral<InputIterator>::value>::type>
    multimap(InputIterator first, InputIterator last) : tree_() {
        for (; first != last; ++first) insert(*first);
    }

    multimap(std::initializer_list<value_type> il) : tree_() {
        for (auto& v : il) insert(v);
    }

    iterator begin() { return iterator(tree_.begin_node()); }
    const_iterator begin() const { return const_iterator(tree_.begin_node()); }
    const_iterator cbegin() const { return begin(); }
    iterator end() { return iterator(tree_.end_node()); }
    const_iterator end() const { return const_iterator(tree_.end_node()); }
    const_iterator cend() const { return end(); }

    bool empty() const { return tree_.empty(); }
    size_type size() const { return tree_.size(); }

    iterator insert(const value_type& v) {
        return iterator(tree_.insert_equal(v));
    }

    iterator erase(const_iterator pos) {
        auto base = pos.base();
        auto next_base = detail::rb_tree_node_base::successor(base);
        tree_.erase(base);
        return iterator(next_base);
    }

    size_type erase(const key_type& k) {
        auto lb = tree_.lower_bound(k);
        auto ub = tree_.upper_bound(k);
        size_type n = 0;
        while (lb != ub) { auto next = detail::rb_tree_node_base::successor(lb); tree_.erase(lb); lb = next; ++n; }
        return n;
    }

    void clear() { tree_.clear(); }

    iterator find(const key_type& k) {
        return iterator(tree_.find(k));
    }
    const_iterator find(const key_type& k) const {
        return const_iterator(const_cast<tree_type&>(tree_).find(k));
    }

    size_type count(const key_type& k) const {
        return tree_.count(k);
    }

    iterator lower_bound(const key_type& k) {
        return iterator(tree_.lower_bound(k));
    }
    iterator upper_bound(const key_type& k) {
        return iterator(tree_.upper_bound(k));
    }

    void swap(multimap& other) noexcept { tree_.swap(other.tree_); }
};

} // namespace lstl
