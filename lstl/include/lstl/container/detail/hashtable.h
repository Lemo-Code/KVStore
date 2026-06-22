/**
 * @file    hashtable.h
 * @brief   Hash table with separate chaining and prime-number bucket sizing.
 * @author  lstl team
 * @date    2025
 *
 * Implements a hash table with FNV-1a hashing, separate chaining,
 * automatic rehashing at load factor 1.0, and support for both
 * unique-key and duplicate-key insertion modes.
 *
 * @ingroup container_detail
 */
// Use of this source code is governed by a MIT-style license.
//
// hashtable.h - Hash table implementation with separate chaining.
//
// Features:
//   - Separate chaining with singly-linked lists
//   - Prime number bucket counts for better distribution
//   - Load factor threshold (1.0 default) triggers rehash
//   - Support for unique and duplicate keys
//   - Support for custom hash and equality functions

#pragma once

#include <cstddef>
#include <iterator>
#include <utility>
#include <algorithm>

#include "../../memory/type_traits.h"
#include "../../memory/utility.h"
#include "../../memory/construct.h"
#include "../../memory/allocator.h"
#include "../../memory/functional.h"
#include "../detail/slist_node.h"

namespace lstl {
namespace detail {

////////////////////////////////////////////////////////////////////////////
// Prime numbers for bucket sizes (from SGI STL)
////////////////////////////////////////////////////////////////////////////
inline unsigned long next_prime(unsigned long n) {
    static const unsigned long primes[] = {
        53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593,
        49157, 98317, 196613, 393241, 786433, 1572869, 3145739,
        6291469, 12582917, 25165843, 50331653, 100663319,
        201326611, 402653189, 805306457, 1610612741, 3221225473UL
    };
    static const size_t num_primes = sizeof(primes) / sizeof(primes[0]);
    for (size_t i = 0; i < num_primes; ++i) {
        if (primes[i] >= n) return primes[i];
    }
    return primes[num_primes - 1];
}

////////////////////////////////////////////////////////////////////////////
// hashtable_node - Node for separate chaining
////////////////////////////////////////////////////////////////////////////
template <typename Value>
struct hashtable_node {
    Value        value;
    hashtable_node* next;

    template <typename... Args>
    hashtable_node(Args&&... args)
        : value(std::forward<Args>(args)...), next(nullptr) {}
};

////////////////////////////////////////////////////////////////////////////
// hashtable iterator
////////////////////////////////////////////////////////////////////////////
template <typename ValueT, typename KeyT, typename KeyOfValueT,
          typename HashFunc, typename KeyEqualFunc, typename Alloc>
class hashtable;

template <typename Value>
class hashtable_iterator {
public:
    typedef Value                              value_type;
    typedef Value&                             reference;
    typedef Value*                             pointer;
    typedef ptrdiff_t                          difference_type;
    typedef std::forward_iterator_tag          iterator_category;

    typedef hashtable_node<Value>*             node_ptr;
    typedef hashtable_node<Value>**            bucket_ptr;

    hashtable_iterator() : cur_(nullptr), buckets_(nullptr), num_buckets_(0) {}
    hashtable_iterator(node_ptr n, bucket_ptr b, size_t nb)
        : cur_(n), buckets_(b), num_buckets_(nb) {}

    reference operator*() const { return cur_->value; }
    pointer operator->() const { return &cur_->value; }

    hashtable_iterator& operator++() {
        if (cur_->next) {
            cur_ = cur_->next;
        } else {
            // Find next non-empty bucket
            size_t idx = bucket_index();
            while (++idx < num_buckets_ && buckets_[idx] == nullptr) {}
            cur_ = (idx < num_buckets_) ? buckets_[idx] : nullptr;
        }
        return *this;
    }

    hashtable_iterator operator++(int) {
        hashtable_iterator tmp = *this;
        ++(*this);
        return tmp;
    }

    bool operator==(const hashtable_iterator& other) const {
        return cur_ == other.cur_;
    }
    bool operator!=(const hashtable_iterator& other) const {
        return cur_ != other.cur_;
    }

    node_ptr node() const { return cur_; }

private:
    size_t bucket_index() const {
        // Find which bucket cur_ is in (needed for iterator advance)
        for (size_t i = 0; i < num_buckets_; ++i) {
            node_ptr p = buckets_[i];
            while (p) {
                if (p == cur_) return i;
                p = p->next;
            }
        }
        return num_buckets_;
    }

    node_ptr   cur_;
    bucket_ptr buckets_;
    size_t     num_buckets_;
};

////////////////////////////////////////////////////////////////////////////
// hashtable - Separate chaining hash table
////////////////////////////////////////////////////////////////////////////
template <typename ValueT, typename KeyT, typename KeyOfValueT,
          typename HashFunc, typename KeyEqualFunc, typename Alloc = allocator<ValueT>>
class hashtable {
public:
    typedef KeyT              key_type;
    typedef ValueT            value_type;
    typedef HashFunc          hasher;
    typedef KeyEqualFunc      key_equal;
    typedef size_t            size_type;
    typedef ptrdiff_t         difference_type;

    typedef hashtable_node<ValueT>     node_type;
    typedef node_type*                 node_ptr;

    typedef hashtable_iterator<ValueT> iterator;

    typedef typename Alloc::template rebind<node_type>::other node_allocator;

    friend class hashtable_iterator<ValueT>;

    static const size_type kDefaultBuckets = 53;

private:
    hasher       hash_;
    key_equal    equals_;
    KeyOfValueT  key_of_value_;
    node_allocator node_alloc_;

    node_ptr*    buckets_;
    size_type    num_buckets_;
    size_type    num_elements_;
    float        max_load_factor_;

    node_ptr create_node(const value_type& v) {
        node_ptr node = node_alloc_.allocate(1);
        try {
            lstl::construct(&node->value, v);
            node->next = nullptr;
        } catch (...) {
            node_alloc_.deallocate(node, 1);
            throw;
        }
        return node;
    }

    void destroy_node(node_ptr node) {
        lstl::destroy(&node->value);
        node_alloc_.deallocate(node, 1);
    }

    size_type bucket_index(const key_type& k, size_type n) const {
        return hash_(k) % n;
    }

    // Rehash to a new bucket count
    void resize(size_type new_bucket_count) {
        if (new_bucket_count <= num_buckets_) return;

        node_ptr* new_buckets = new node_ptr[new_bucket_count]();
        size_type old_num = num_buckets_;
        node_ptr* old_buckets = buckets_;

        // Transfer all nodes
        for (size_type i = 0; i < old_num; ++i) {
            node_ptr cur = old_buckets[i];
            while (cur) {
                node_ptr next = cur->next;
                size_type idx = bucket_index(key_of_value_(cur->value), new_bucket_count);
                cur->next = new_buckets[idx];
                new_buckets[idx] = cur;
                cur = next;
            }
        }

        buckets_ = new_buckets;
        num_buckets_ = new_bucket_count;
        delete[] old_buckets;
    }

    void check_load_factor() {
        if (num_elements_ >= size_type(max_load_factor_ * num_buckets_)) {
            resize(next_prime(num_buckets_ + 1));
        }
    }

public:
    //////////////////////////////////////////////////////////////////////////
    // Construction
    //////////////////////////////////////////////////////////////////////////
    hashtable()
        : hash_(), equals_(), key_of_value_(),
          buckets_(nullptr), num_buckets_(kDefaultBuckets),
          num_elements_(0), max_load_factor_(1.0f) {
        buckets_ = new node_ptr[num_buckets_]();
    }

    hashtable(const hashtable& other)
        : hash_(other.hash_), equals_(other.equals_),
          key_of_value_(other.key_of_value_),
          num_buckets_(other.num_buckets_),
          num_elements_(0), max_load_factor_(other.max_load_factor_) {
        buckets_ = new node_ptr[num_buckets_]();
        for (auto it = other.begin(); it != other.end(); ++it) {
            insert_equal(*it);
        }
    }

    ~hashtable() {
        clear();
        delete[] buckets_;
    }

    hashtable& operator=(const hashtable& other) {
        if (this != &other) {
            clear();
            hash_ = other.hash_;
            equals_ = other.equals_;
            max_load_factor_ = other.max_load_factor_;
            for (auto it = other.begin(); it != other.end(); ++it) {
                insert_equal(*it);
            }
        }
        return *this;
    }

    //////////////////////////////////////////////////////////////////////////
    // Size
    //////////////////////////////////////////////////////////////////////////
    size_type size() const { return num_elements_; }
    bool empty() const { return num_elements_ == 0; }
    size_type bucket_count() const { return num_buckets_; }
    float max_load_factor() const { return max_load_factor_; }
    void max_load_factor(float mlf) { max_load_factor_ = mlf; if (num_elements_ > 0) check_load_factor(); }

    //////////////////////////////////////////////////////////////////////////
    // Insert (unique keys)
    //////////////////////////////////////////////////////////////////////////
    pair<iterator, bool> insert_unique(const value_type& v) {
        check_load_factor();
        const key_type& k = key_of_value_(v);
        size_type idx = bucket_index(k, num_buckets_);

        // Search for duplicate
        for (node_ptr p = buckets_[idx]; p; p = p->next) {
            if (equals_(k, key_of_value_(p->value))) {
                return lstl::make_pair(iterator(p, buckets_, num_buckets_), false);
            }
        }

        node_ptr node = create_node(v);
        node->next = buckets_[idx];
        buckets_[idx] = node;
        ++num_elements_;
        return lstl::make_pair(iterator(node, buckets_, num_buckets_), true);
    }

    //////////////////////////////////////////////////////////////////////////
    // Insert (allow duplicates)
    //////////////////////////////////////////////////////////////////////////
    iterator insert_equal(const value_type& v) {
        check_load_factor();
        const key_type& k = key_of_value_(v);
        size_type idx = bucket_index(k, num_buckets_);

        // Insert at head of bucket for efficiency
        node_ptr node = create_node(v);
        node->next = buckets_[idx];
        buckets_[idx] = node;
        ++num_elements_;
        return iterator(node, buckets_, num_buckets_);
    }

    //////////////////////////////////////////////////////////////////////////
    // Find
    //////////////////////////////////////////////////////////////////////////
    iterator find(const key_type& k) const {
        size_type idx = bucket_index(k, num_buckets_);
        for (node_ptr p = buckets_[idx]; p; p = p->next) {
            if (equals_(k, key_of_value_(p->value))) {
                return iterator(p, buckets_, num_buckets_);
            }
        }
        return end();
    }

    size_type count(const key_type& k) const {
        size_type n = 0;
        size_type idx = bucket_index(k, num_buckets_);
        for (node_ptr p = buckets_[idx]; p; p = p->next) {
            if (equals_(k, key_of_value_(p->value))) ++n;
        }
        return n;
    }

    //////////////////////////////////////////////////////////////////////////
    // Erase
    //////////////////////////////////////////////////////////////////////////
    size_type erase_unique(const key_type& k) {
        size_type idx = bucket_index(k, num_buckets_);
        node_ptr prev = nullptr;

        for (node_ptr p = buckets_[idx]; p; p = p->next) {
            if (equals_(k, key_of_value_(p->value))) {
                if (prev) prev->next = p->next;
                else buckets_[idx] = p->next;
                destroy_node(p);
                --num_elements_;
                return 1;
            }
            prev = p;
        }
        return 0;
    }

    void erase(iterator it) {
        if (it == end()) return;
        node_ptr target = it.node();
        const key_type& k = key_of_value_(target->value);
        size_type idx = bucket_index(k, num_buckets_);

        node_ptr prev = nullptr;
        for (node_ptr p = buckets_[idx]; p; p = p->next) {
            if (p == target) {
                if (prev) prev->next = p->next;
                else buckets_[idx] = p->next;
                destroy_node(p);
                --num_elements_;
                return;
            }
            prev = p;
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Iteration
    //////////////////////////////////////////////////////////////////////////
    iterator begin() const {
        for (size_type i = 0; i < num_buckets_; ++i) {
            if (buckets_[i] != nullptr) {
                return iterator(buckets_[i], buckets_, num_buckets_);
            }
        }
        return end();
    }

    iterator end() const {
        return iterator(nullptr, buckets_, num_buckets_);
    }

    //////////////////////////////////////////////////////////////////////////
    // Clear
    //////////////////////////////////////////////////////////////////////////
    /// @brief  O(1) swap — exchanges bucket arrays and counters.
    void swap(hashtable& other) noexcept {
        lstl::swap(buckets_, other.buckets_);
        lstl::swap(num_buckets_, other.num_buckets_);
        lstl::swap(num_elements_, other.num_elements_);
    }

    void clear() {
        for (size_type i = 0; i < num_buckets_; ++i) {
            node_ptr p = buckets_[i];
            while (p) {
                node_ptr next = p->next;
                destroy_node(p);
                p = next;
            }
            buckets_[i] = nullptr;
        }
        num_elements_ = 0;
    }
};

} // namespace detail
} // namespace lstl
