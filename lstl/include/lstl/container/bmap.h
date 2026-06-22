/**
 * @file    bmap.h
 * @brief   Ordered map backed by B+ tree — cache-optimized for large datasets.
 * @author  lstl team
 * @date    2025
 *
 * lstl::bmap uses an in-memory B+ tree where all data resides in leaf
 * nodes linked together for efficient range scans. Internal nodes
 * contain only routing keys, improving cache utilization.
 *
 * Key features:
 * - Cache-friendly large nodes (default order 256 → ~4KB leaves).
 * - O(log n) search/insert/erase.
 * - O(1) range iteration via leaf node linked list.
 * - Excellent for workloads with many range queries.
 *
 * @tparam Key      Key type.
 * @tparam T        Mapped value type.
 * @tparam Order    Maximum children per internal node (default: 256).
 * @tparam Compare  Key comparison functor.
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
#include "detail/bplus_tree.h"

namespace lstl {

/**
 * @brief  B+ tree backed ordered map (unique keys).
 *
 * @tparam Key      Key type.
 * @tparam T        Mapped type.
 * @tparam Order    B+ tree order (max children per internal node, default 256).
 * @tparam Compare  Strict weak ordering on keys.
 * @tparam Alloc    Allocator for pair<const Key, T>.
 */
template <typename Key, typename T,
          size_t Order = 256,
          typename Compare = lstl::less<Key>,
          typename Alloc = allocator<pair<const Key, T>>>
class bmap {
public:
    typedef Key                 key_type;
    typedef T                   mapped_type;
    typedef pair<const Key, T>  value_type;
    typedef Compare             key_compare;
    typedef size_t              size_type;

private:
    typedef pair<Key, T> stored_pair;  ///< Internal storage uses non-const first for move support.
    typedef detail::bplus_tree<Key, stored_pair, Order, Compare, Alloc> tree_type;
    tree_type tree_;

public:
    /**
     * @brief  Forward iterator over sorted key-value pairs.
     *
     * Traverses the linked list of leaf nodes for O(1) increment.
     */
    class iterator {
    public:
        typedef typename bmap::value_type       value_type;
        typedef value_type&                     reference;
        typedef value_type*                     pointer;
        typedef ptrdiff_t                       difference_type;
        typedef std::forward_iterator_tag       iterator_category;

        iterator() : leaf_(nullptr), pos_(0) {}
        iterator(typename tree_type::leaf_node* leaf, size_t pos)
            : leaf_(leaf), pos_(pos) {}

        reference operator*() { return reinterpret_cast<reference>(leaf_->values[pos_]); }
        pointer operator->()  { return &(operator*()); }

        iterator& operator++() {
            ++pos_;
            if (pos_ >= leaf_->num_keys) { leaf_ = leaf_->next; pos_ = 0; }
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
        typedef pair<const Key,T>                      value_type;
        typedef const pair<const Key,T>&               reference;
        typedef const pair<const Key,T>*               pointer;
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

        friend class bmap;
    private:
        typename tree_type::leaf_node* leaf_;
        size_t pos_;
    };
    

    bmap() : tree_() {}
    explicit bmap(const Compare& comp) : tree_(comp) {}
    template <typename InputIterator>
    bmap(InputIterator first, InputIterator last) : tree_() {
        for (; first != last; ++first) insert(*first);
    }
    bmap(std::initializer_list<value_type> il) : tree_() {
        for (auto& v : il) insert(v);
    }

    /** @brief  Forward iteration in sorted key order. */
    iterator begin() { return iterator(tree_.first_leaf(), 0); }
    iterator end()   { return iterator(nullptr, 0); }
    const_iterator begin() const { return const_iterator(tree_.first_leaf(), 0); }
    const_iterator end() const { return const_iterator(nullptr, 0); }

    bool empty() const      { return tree_.empty(); }
    size_type size() const  { return tree_.size(); }

    /** @brief  Accesses or creates element with key @p k. */
    mapped_type& operator[](const key_type& k) {
        auto result = find(k);
        if (result == end()) {
            insert(value_type(k, T()));
            result = find(k);
        }
        return result->second;
    }

    pair<iterator,bool> insert(const value_type& v) { auto sz = tree_.size(); tree_.insert(v.first, stored_pair(v.first, v.second)); bool inserted = tree_.size() > sz; return lstl::make_pair(find(v.first), inserted); }
    size_type erase(const key_type& k)     { return tree_.erase(k) ? 1 : 0; }
    void clear()                      { tree_.clear(); }

    iterator find(const key_type& k) {
        auto result = tree_.find(k);
        if (result.first) return iterator(result.first, result.second);
        return end();
    }

    void swap(bmap& other) noexcept { lstl::swap(tree_, other.tree_); }
};

} // namespace lstl
