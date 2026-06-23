/**
 * @file    bset.h
 * @brief   B+ tree backed ordered set. Excellent cache locality for large datasets.
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
#include "detail/bplus_tree.h"

namespace lstl {

template <typename Key,
          size_t Order = 256,
          typename Compare = lstl::less<Key>,
          typename Alloc = allocator<Key>>
class bset {
public:
    typedef Key      key_type;
    typedef Key      value_type;
    typedef Compare  key_compare;
    typedef size_t   size_type;

private:
    // For bset, key and value are the same
    typedef detail::bplus_tree<Key, Key, Order, Compare, Alloc> tree_type;
    tree_type tree_;

public:
    class iterator {
    public:
        typedef value_type                      value_type;
        typedef value_type&                     reference;
        typedef value_type*                     pointer;
        typedef ptrdiff_t                       difference_type;
        typedef std::forward_iterator_tag       iterator_category;

        iterator() : leaf_(nullptr), pos_(0) {}
        iterator(typename tree_type::leaf_node* leaf, size_t pos)
            : leaf_(leaf), pos_(pos) {}

        reference operator*() { return leaf_->values[pos_]; }
        pointer operator->() { return &(operator*()); }

        iterator& operator++() {
            ++pos_;
            if (pos_ >= leaf_->num_keys) {
                leaf_ = leaf_->next;
                pos_ = 0;
            }
            return *this;
        }

        iterator operator++(int) { iterator tmp = *this; ++(*this); return tmp; }
        bool operator==(const iterator& o) const { return leaf_ == o.leaf_ && pos_ == o.pos_; }
        bool operator!=(const iterator& o) const { return !(*this == o); }

    private:
        typename tree_type::leaf_node* leaf_;
        size_t pos_;
    };
class const_iterator {
    public:
        typedef Key                      value_type;
        typedef const Key&               reference;
        typedef const Key*               pointer;
        typedef ptrdiff_t                 difference_type;
        typedef std::forward_iterator_tag  iterator_category;

        const_iterator() : leaf_(nullptr), pos_(0) {}
        const_iterator(const iterator& it) : leaf_(it.leaf()), pos_(it.pos()) {}
        const_iterator(typename tree_type::leaf_node* leaf, size_t pos) : leaf_(leaf), pos_(pos) {}

        reference operator*() const { return reinterpret_cast<reference>(leaf_->values[pos_]); }
        pointer operator->() const { return &(operator*()); }

        const_iterator& operator++() { ++pos_; if (pos_ >= leaf_->num_keys) { leaf_ = leaf_->next; pos_ = 0; } return *this; }
        const_iterator operator++(int) { const_iterator tmp = *this; ++(*this); return tmp; }
        bool operator==(const const_iterator& o) const { return leaf_ == o.leaf_ && pos_ == o.pos_; }
        bool operator!=(const const_iterator& o) const { return !(*this == o); }

    private:
        typename tree_type::leaf_node* leaf_;
        size_t pos_;
    };

    

    bset() : tree_() {}
    explicit bset(const Compare& comp) : tree_(comp) {}

    template <typename InputIterator>
    bset(InputIterator first, InputIterator last) : tree_() {
        for (; first != last; ++first) insert(*first);
    }

    bset(std::initializer_list<value_type> il) : tree_() {
        for (auto& v : il) insert(v);
    }

    iterator begin() { return iterator(tree_.first_leaf(), 0); }
    iterator end() { return iterator(nullptr, 0); }
    const_iterator begin() const { return const_iterator(tree_.first_leaf(), 0); }
    const_iterator end() const { return const_iterator(nullptr, 0); }

    bool empty() const { return tree_.empty(); }
    size_type size() const { return tree_.size(); }

    pair<iterator,bool> insert(const value_type& v) { auto sz = tree_.size(); tree_.insert(v, v); return lstl::make_pair(find(v), tree_.size() > sz); }
    size_type erase(const key_type& k) { return tree_.erase(k) ? 1 : 0; }
    void clear() { tree_.clear(); }

    iterator find(const key_type& k) {
        auto result = tree_.find(k);
        if (result.first) return iterator(result.first, result.second);
        return end();
    }

    void swap(bset& other) noexcept { lstl::swap(tree_, other.tree_); }
};

} // namespace lstl
