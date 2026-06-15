/**
 * @file    skip_list.h
 * @brief   Probabilistic skip list with O(log n) expected performance.
 * @author  lstl team
 * @date    2025
 *
 * Implements a 32-level skip list with 1/4 promotion probability.
 * Each level is a sorted singly-linked list; higher levels provide
 * express lanes for faster search. Supports unique and duplicate keys.
 *
 * @ingroup container_detail
 */
// Use of this source code is governed by a MIT-style license.
//
// skip_list.h - Probabilistic skip list implementation.
//
// A skip list is a layered linked list where each node has a random number
// of forward pointers, providing O(log n) expected search/insert/delete.

#pragma once

#include <cstddef>
#include <cstdlib>
#include <iterator>

#include "../../memory/type_traits.h"
#include "../../memory/utility.h"
#include "../../memory/construct.h"

namespace lstl {
namespace detail {

static const size_t kSkipListMaxLevel = 32;
static const size_t kSkipListProbability = 4;

inline size_t skip_list_random_level() {
    size_t level = 1;
    while ((rand() % kSkipListProbability) == 0 && level < kSkipListMaxLevel) {
        ++level;
    }
    return level;
}

////////////////////////////////////////////////////////////////////////////
// skip_list_node - Plain struct, no constructors
// Memory layout: [value_size bytes][level * sizeof(node_ptr*) bytes for forward[]]
// forward[] is accessed at negative offset or inline depending on allocation
////////////////////////////////////////////////////////////////////////////
template <typename Value>
struct skip_list_node {
    Value  value;
    // forward[] array follows this struct in memory
    // Access via helper: forward_ptr(this, level)

    static skip_list_node** forward_ptr(skip_list_node* node, int /*dummy*/) {
        return reinterpret_cast<skip_list_node**>(
            reinterpret_cast<char*>(node) + sizeof(skip_list_node));
    }
};

////////////////////////////////////////////////////////////////////////////
// skip_list
////////////////////////////////////////////////////////////////////////////
template <typename Value, typename Key, typename KeyOfValue,
          typename Compare, typename Alloc = allocator<Value>>
class skip_list {
public:
    typedef Key                  key_type;
    typedef Value                value_type;
    typedef Value*               pointer;
    typedef const Value*         const_pointer;
    typedef Value&               reference;
    typedef const Value&         const_reference;
    typedef size_t               size_type;
    typedef ptrdiff_t            difference_type;

    typedef skip_list_node<Value>  node_type;
    typedef node_type*             node_ptr;

private:
    node_ptr      header_;
    size_type     level_;
    size_type     size_;
    Compare       compare_;
    KeyOfValue    key_of_value_;

    // Get forward array for a node
    static skip_list_node<Value>** forward(node_ptr n) {
        return node_type::forward_ptr(n, 0);
    }

    // Aligned size: rounds up sizeof(node_type) to alignment of pointer type
    static size_t aligned_node_size() {
        const size_t align = alignof(skip_list_node<Value>*);
        return (sizeof(node_type) + align - 1) & ~(align - 1);
    }

    // Allocate memory for a node with given level
    // Layout: [aligned skip_list_node struct][level * sizeof(node_ptr*)]
    static void* allocate_node_mem(size_t level) {
        size_t bytes = aligned_node_size() + level * sizeof(node_ptr*);
        void* mem = std::malloc(bytes);
        if (!mem) throw std::bad_alloc();
        return mem;
    }

    node_ptr create_node(size_t level, const value_type& v) {
        void* mem = allocate_node_mem(level);
        node_ptr node = reinterpret_cast<node_ptr>(mem);
        ::new (&node->value) value_type(v);
        skip_list_node<Value>** fwd = reinterpret_cast<skip_list_node<Value>**>(
            reinterpret_cast<char*>(mem) + aligned_node_size());
        for (size_t i = 0; i < level; ++i) fwd[i] = nullptr;
        return node;
    }

    void destroy_node(node_ptr node) {
        node->value.~value_type();
        std::free(node);
    }

    node_ptr create_header() {
        void* mem = allocate_node_mem(kSkipListMaxLevel);
        node_ptr node = reinterpret_cast<node_ptr>(mem);
        ::new (&node->value) value_type();
        skip_list_node<Value>** fwd = reinterpret_cast<skip_list_node<Value>**>(
            reinterpret_cast<char*>(mem) + aligned_node_size());
        for (size_t i = 0; i < kSkipListMaxLevel; ++i) fwd[i] = nullptr;
        return node;
    }

    // Access forward[i] for a node (uses aligned offset)
    skip_list_node<Value>*& forward_at(node_ptr n, size_t i) {
        return (reinterpret_cast<skip_list_node<Value>**>(
            reinterpret_cast<char*>(n) + aligned_node_size()))[i];
    }

    skip_list_node<Value>* forward_at(node_ptr n, size_t i) const {
        return (reinterpret_cast<skip_list_node<Value>* const*>(
            reinterpret_cast<const char*>(n) + aligned_node_size()))[i];
    }

public:
    skip_list()
        : level_(1), size_(0), compare_(), key_of_value_() {
        header_ = create_header();
    }

    skip_list(const skip_list& other)
        : level_(1), size_(0), compare_(other.compare_),
          key_of_value_(other.key_of_value_) {
        header_ = create_header();
        for (auto it = other.begin(); it != other.end(); ++it) {
            insert_equal(*it);
        }
    }

    ~skip_list() {
        clear();
        header_->value.~value_type();
        std::free(header_);
    }

    skip_list& operator=(const skip_list& other) {
        if (this != &other) {
            clear();
            compare_ = other.compare_;
            for (auto it = other.begin(); it != other.end(); ++it) {
                insert_equal(*it);
            }
        }
        return *this;
    }

    size_type size() const { return size_; }
    bool empty() const { return size_ == 0; }

    /// @brief  O(1) swap — exchanges header and counters.
    void swap(skip_list& other) noexcept {
        lstl::swap(header_, other.header_);
        lstl::swap(level_, other.level_);
        lstl::swap(size_, other.size_);
    }

    node_ptr find(const key_type& k) const {
        node_ptr x = header_;
        for (size_t i = level_; i-- > 0; ) {
            while (forward_at(x, i) &&
                   compare_(key_of_value_(forward_at(x, i)->value), k)) {
                x = forward_at(x, i);
            }
        }
        x = forward_at(x, 0);
        if (x && !compare_(k, key_of_value_(x->value)) &&
            !compare_(key_of_value_(x->value), k)) {
            return x;
        }
        return nullptr;
    }

    size_type count(const key_type& k) const {
        node_ptr x = header_;
        for (size_t i = level_; i-- > 0; ) {
            while (forward_at(x, i) &&
                   compare_(key_of_value_(forward_at(x, i)->value), k)) {
                x = forward_at(x, i);
            }
        }
        size_type n = 0;
        x = forward_at(x, 0);
        while (x && !compare_(k, key_of_value_(x->value)) &&
               !compare_(key_of_value_(x->value), k)) {
            ++n;
            x = forward_at(x, 0);
        }
        return n;
    }

    node_ptr lower_bound(const key_type& k) const {
        node_ptr x = header_;
        for (size_t i = level_; i-- > 0; ) {
            while (forward_at(x, i) &&
                   compare_(key_of_value_(forward_at(x, i)->value), k)) {
                x = forward_at(x, i);
            }
        }
        return forward_at(x, 0);
    }

    node_ptr upper_bound(const key_type& k) const {
        node_ptr x = header_;
        for (size_t i = level_; i-- > 0; ) {
            while (forward_at(x, i) &&
                   !compare_(k, key_of_value_(forward_at(x, i)->value))) {
                x = forward_at(x, i);
            }
        }
        return forward_at(x, 0);
    }

    pair<node_ptr, bool> insert_unique(const value_type& v) {
        node_ptr update[kSkipListMaxLevel];
        node_ptr x = header_;
        const key_type& k = key_of_value_(v);

        for (size_t i = level_; i-- > 0; ) {
            while (forward_at(x, i) &&
                   compare_(key_of_value_(forward_at(x, i)->value), k)) {
                x = forward_at(x, i);
            }
            update[i] = x;
        }
        x = forward_at(x, 0);

        if (x && !compare_(k, key_of_value_(x->value)) &&
            !compare_(key_of_value_(x->value), k)) {
            return lstl::make_pair(x, false);
        }

        size_t new_level = skip_list_random_level();
        if (new_level > level_) {
            for (size_t i = level_; i < new_level; ++i) {
                update[i] = header_;
            }
            level_ = new_level;
        }

        node_ptr n = create_node(new_level, v);
        for (size_t i = 0; i < new_level; ++i) {
            forward_at(n, i) = forward_at(update[i], i);
            forward_at(update[i], i) = n;
        }
        ++size_;
        return lstl::make_pair(n, true);
    }

    node_ptr insert_equal(const value_type& v) {
        node_ptr update[kSkipListMaxLevel];
        node_ptr x = header_;
        const key_type& k = key_of_value_(v);

        for (size_t i = level_; i-- > 0; ) {
            while (forward_at(x, i) &&
                   compare_(key_of_value_(forward_at(x, i)->value), k)) {
                x = forward_at(x, i);
            }
            update[i] = x;
        }

        size_t new_level = skip_list_random_level();
        if (new_level > level_) {
            for (size_t i = level_; i < new_level; ++i) {
                update[i] = header_;
            }
            level_ = new_level;
        }

        node_ptr n = create_node(new_level, v);
        for (size_t i = 0; i < new_level; ++i) {
            forward_at(n, i) = forward_at(update[i], i);
            forward_at(update[i], i) = n;
        }
        ++size_;
        return n;
    }

    size_type erase_unique(const key_type& k) {
        node_ptr update[kSkipListMaxLevel];
        node_ptr x = header_;

        for (size_t i = level_; i-- > 0; ) {
            while (forward_at(x, i) &&
                   compare_(key_of_value_(forward_at(x, i)->value), k)) {
                x = forward_at(x, i);
            }
            update[i] = x;
        }
        x = forward_at(x, 0);

        if (!x || compare_(k, key_of_value_(x->value)) ||
            compare_(key_of_value_(x->value), k)) {
            return 0;
        }

        for (size_t i = 0; i < level_; ++i) {
            if (forward_at(update[i], i) == x) {
                forward_at(update[i], i) = forward_at(x, i);
            }
        }

        destroy_node(x);

        while (level_ > 1 && forward_at(header_, level_ - 1) == nullptr) {
            --level_;
        }

        --size_;
        return 1;
    }

    void erase(node_ptr x) {
        if (!x) return;
        // Find predecessors at each level and splice out x
        node_ptr update[kSkipListMaxLevel];
        node_ptr cur = header_;
        const key_type& k = key_of_value_(x->value);
        for (size_t i = level_; i-- > 0; ) {
            while (forward_at(cur, i) && forward_at(cur, i) != x &&
                   compare_(key_of_value_(forward_at(cur, i)->value), k)) {
                cur = forward_at(cur, i);
            }
            update[i] = cur;
        }
        // Splice out at each level where x is present
        for (size_t i = 0; i < level_; ++i) {
            if (forward_at(update[i], i) == x)
                forward_at(update[i], i) = forward_at(x, i);
        }
        destroy_node(x);
        while (level_ > 1 && forward_at(header_, level_ - 1) == nullptr) --level_;
        --size_;
    }

    node_ptr begin_node() const { return forward_at(header_, 0); }
    node_ptr end_node() const { return nullptr; }

    void clear() {
        node_ptr x = forward_at(header_, 0);
        while (x) {
            node_ptr next = forward_at(x, 0);
            destroy_node(x);
            x = next;
        }
        for (size_t i = 0; i < kSkipListMaxLevel; ++i) {
            forward_at(header_, i) = nullptr;
        }
        level_ = 1;
        size_ = 0;
    }
};

////////////////////////////////////////////////////////////////////////////
// skip_list iterator
////////////////////////////////////////////////////////////////////////////
template <typename Value>
class skip_list_iterator {
public:
    typedef Value                            value_type;
    typedef Value&                           reference;
    typedef Value*                           pointer;
    typedef ptrdiff_t                        difference_type;
    typedef std::forward_iterator_tag        iterator_category;

    typedef skip_list_node<Value>*           node_ptr;

    skip_list_iterator() : node_(nullptr) {}
    explicit skip_list_iterator(node_ptr n) : node_(n) {}

    reference operator*() const { return node_->value; }
    pointer operator->() const { return &node_->value; }

    skip_list_iterator& operator++() {
        node_ = skip_list_node<Value>::forward_ptr(node_, 0)[0];
        return *this;
    }

    skip_list_iterator operator++(int) {
        skip_list_iterator tmp = *this;
        node_ = skip_list_node<Value>::forward_ptr(node_, 0)[0];
        return tmp;
    }

    bool operator==(const skip_list_iterator& other) const {
        return node_ == other.node_;
    }
    bool operator!=(const skip_list_iterator& other) const {
        return node_ != other.node_;
    }

    node_ptr base() const { return node_; }

private:
    node_ptr node_;
};

} // namespace detail
} // namespace lstl
