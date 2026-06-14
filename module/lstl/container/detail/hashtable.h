#ifndef LSTL_HASHTABLE_H
#define LSTL_HASHTABLE_H

#include <cstddef>

#include "memory.h"
#include "sequence/vector.h"

namespace lstl {
namespace detail {

template <typename Value, typename Hash, typename Pred, typename Alloc = allocator<Value> >
class hashtable {
 public:
  typedef Hash hasher;
  typedef Pred key_equal;
  typedef Value value_type;
  typedef Alloc allocator_type;
  typedef typename allocator_traits<Alloc>::size_type size_type;
  typedef typename allocator_traits<Alloc>::difference_type difference_type;
  typedef value_type* pointer;
  typedef const value_type* const_pointer;
  typedef value_type& reference;
  typedef const value_type& const_reference;

  struct node {
    node* next;
    value_type value;
    node() : next(0), value() {}
    explicit node(const value_type& v) : next(0), value(v) {}
  };

  struct iterator {
    typedef forward_iterator_tag iterator_category;
    typedef ptrdiff_t difference_type;
    typedef Value* pointer;
    typedef Value& reference;

    hashtable* table;
    size_type bucket;
    node* cur;

    iterator() : table(0), bucket(0), cur(0) {}
    iterator(hashtable* t, size_type b, node* n) : table(t), bucket(b), cur(n) {}

    reference operator*() const { return cur->value; }
    pointer operator->() const { return &(operator*()); }

    iterator& operator++() {
      if (cur->next) {
        cur = cur->next;
      } else {
        table->advance_bucket(bucket, cur, *this);
      }
      return *this;
    }

    iterator operator++(int) {
      iterator tmp = *this;
      ++*this;
      return tmp;
    }

    bool operator==(const iterator& x) const { return cur == x.cur; }
    bool operator!=(const iterator& x) const { return cur != x.cur; }
  };

  struct const_iterator {
    typedef forward_iterator_tag iterator_category;
    typedef ptrdiff_t difference_type;
    typedef const Value* pointer;
    typedef const Value& reference;

    const hashtable* table;
    size_type bucket;
    const node* cur;

    const_iterator() : table(0), bucket(0), cur(0) {}
    const_iterator(const hashtable* t, size_type b, const node* n) : table(t), bucket(b), cur(n) {}
    const_iterator(const iterator& it) : table(it.table), bucket(it.bucket), cur(it.cur) {}

    reference operator*() const { return cur->value; }
    pointer operator->() const { return &(operator*()); }

    const_iterator& operator++() {
      if (cur->next) {
        cur = cur->next;
      } else {
        iterator tmp(table->const_cast_table(), bucket, const_cast<node*>(cur));
        table->advance_bucket(bucket, const_cast<node*>(cur), tmp);
        bucket = tmp.bucket;
        cur = tmp.cur;
      }
      return *this;
    }

    const_iterator operator++(int) {
      const_iterator tmp = *this;
      ++*this;
      return tmp;
    }

    bool operator==(const const_iterator& x) const { return cur == x.cur; }
    bool operator!=(const const_iterator& x) const { return cur != x.cur; }
  };

 protected:
  typedef typename allocator_traits<Alloc>::template rebind_alloc<node>::other node_allocator;
  typedef vector<node*> bucket_type;

  enum { initial_bucket_count = 16 };

  node_allocator node_alloc_;
  bucket_type buckets_;
  size_type size_;
  hasher hash_;
  key_equal equal_;
  float max_load_factor_;

 public:
  hashtable(size_type n, const Hash& hf, const Pred& eq)
      : buckets_(n > 0 ? n : initial_bucket_count, (node*)0),
        size_(0),
        hash_(hf),
        equal_(eq),
        max_load_factor_(1.0f) {}

  hashtable(size_type n, const Hash& hf, const Pred& eq, const Alloc& a)
      : node_alloc_(a),
        buckets_(n > 0 ? n : initial_bucket_count, (node*)0, a),
        size_(0),
        hash_(hf),
        equal_(eq),
        max_load_factor_(1.0f) {}

  hashtable(const hashtable& other)
      : buckets_(other.buckets_.size(), (node*)0, other.node_alloc_),
        size_(0),
        hash_(other.hash_),
        equal_(other.equal_),
        max_load_factor_(other.max_load_factor_),
        node_alloc_(other.node_alloc_) {
    insert_equal(other.begin(), other.end());
  }

  hashtable(hashtable&& other) throw()
      : buckets_(lstl::move(other.buckets_)),
        size_(other.size_),
        hash_(other.hash_),
        equal_(other.equal_),
        max_load_factor_(other.max_load_factor_),
        node_alloc_(other.node_alloc_) {
    other.size_ = 0;
  }

  ~hashtable() { clear(); }

  iterator begin() {
    for (size_type i = 0; i < buckets_.size(); ++i) {
      if (buckets_[i]) {
        return iterator(this, i, buckets_[i]);
      }
    }
    return end();
  }

  const_iterator begin() const {
    for (size_type i = 0; i < buckets_.size(); ++i) {
      if (buckets_[i]) {
        return const_iterator(this, i, buckets_[i]);
      }
    }
    return end();
  }

  iterator end() { return iterator(this, buckets_.size(), 0); }

  const_iterator end() const { return const_iterator(this, buckets_.size(), 0); }

  size_type size() const throw() { return size_; }
  bool empty() const throw() { return size_ == 0; }
  size_type bucket_count() const { return buckets_.size(); }
  float max_load_factor() const { return max_load_factor_; }
  void max_load_factor(float ml) { max_load_factor_ = ml > 0.0f ? ml : 1.0f; }

  hasher hash_funct() const { return hash_; }
  key_equal key_eq() const { return equal_; }

  size_type bucket_index(const value_type& value) const {
    return hash_(value) % buckets_.size();
  }

  iterator insert_unique(const value_type& obj) {
    resize_if_needed();
    const size_type n = bucket_index(obj);
    node* first = buckets_[n];
    for (node* cur = first; cur; cur = cur->next) {
      if (equal_(cur->value, obj)) {
        return iterator(this, n, cur);
      }
    }
    node* tmp = create_node(obj);
    tmp->next = buckets_[n];
    buckets_[n] = tmp;
    ++size_;
    return iterator(this, n, tmp);
  }

  iterator insert_equal(const value_type& obj) {
    resize_if_needed();
    const size_type n = bucket_index(obj);
    node* tmp = create_node(obj);
    tmp->next = buckets_[n];
    buckets_[n] = tmp;
    ++size_;
    return iterator(this, n, tmp);
  }

  template <typename InputIterator>
  void insert_unique(InputIterator first, InputIterator last) {
    for (; first != last; ++first) {
      insert_unique(*first);
    }
  }

  template <typename InputIterator>
  void insert_equal(InputIterator first, InputIterator last) {
    for (; first != last; ++first) {
      insert_equal(*first);
    }
  }

  iterator find(const value_type& obj) {
    const size_type n = bucket_index(obj);
    for (node* cur = buckets_[n]; cur; cur = cur->next) {
      if (equal_(cur->value, obj)) {
        return iterator(this, n, cur);
      }
    }
    return end();
  }

  const_iterator find(const value_type& obj) const {
    const size_type n = bucket_index(obj);
    for (node* cur = buckets_[n]; cur; cur = cur->next) {
      if (equal_(cur->value, obj)) {
        return const_iterator(this, n, cur);
      }
    }
    return end();
  }

  size_type erase(const value_type& obj) {
    const size_type n = bucket_index(obj);
    node* first = buckets_[n];
    node* prev = 0;
    size_type erased = 0;
    while (first) {
      if (equal_(first->value, obj)) {
        node* next = first->next;
        if (prev) {
          prev->next = next;
        } else {
          buckets_[n] = next;
        }
        destroy_node(first);
        first = next;
        ++erased;
        --size_;
      } else {
        prev = first;
        first = first->next;
      }
    }
    return erased;
  }

  iterator erase(iterator it) {
    if (it.cur == 0) {
      return end();
    }
    size_type n = it.bucket;
    node* first = buckets_[n];
    if (first == it.cur) {
      buckets_[n] = first->next;
    } else {
      while (first && first->next != it.cur) {
        first = first->next;
      }
      if (first) {
        first->next = it.cur->next;
      }
    }
    node* next = it.cur->next;
    destroy_node(it.cur);
    --size_;
    if (next) {
      return iterator(this, n, next);
    }
    iterator result = end();
    advance_bucket(n, 0, result);
    return result;
  }

  void clear() {
    for (size_type i = 0; i < buckets_.size(); ++i) {
      node* cur = buckets_[i];
      while (cur) {
        node* next = cur->next;
        destroy_node(cur);
        cur = next;
      }
      buckets_[i] = 0;
    }
    size_ = 0;
  }

  void rehash(size_type n) {
    if (n < buckets_.size()) {
      n = buckets_.size();
    }
    bucket_type new_buckets(n, (node*)0);
    for (size_type i = 0; i < buckets_.size(); ++i) {
      node* cur = buckets_[i];
      while (cur) {
        node* next = cur->next;
        const size_type idx = hash_(cur->value) % n;
        cur->next = new_buckets[idx];
        new_buckets[idx] = cur;
        cur = next;
      }
    }
    buckets_.swap(new_buckets);
  }

  void swap(hashtable& other) throw() {
    buckets_.swap(other.buckets_);
    swap(size_, other.size_);
    swap(hash_, other.hash_);
    swap(equal_, other.equal_);
    swap(max_load_factor_, other.max_load_factor_);
    node_allocator tmp = node_alloc_;
    node_alloc_ = other.node_alloc_;
    other.node_alloc_ = tmp;
  }

 protected:
  hashtable* const_cast_table() const { return const_cast<hashtable*>(this); }

  void resize_if_needed() {
    if (size_ + 1 > buckets_.size() * static_cast<size_type>(max_load_factor_)) {
      rehash(buckets_.size() * 2 + 1);
    }
  }

  node* create_node(const value_type& v) {
    node* p = node_alloc_.allocate(1);
    construct(&p->value, v);
    p->next = 0;
    return p;
  }

  void destroy_node(node* p) {
    destroy(&p->value);
    node_alloc_.deallocate(p, 1);
  }

  void advance_bucket(size_type& bucket, node* /*cur*/, iterator& result) const {
    for (++bucket; bucket < buckets_.size(); ++bucket) {
      if (buckets_[bucket]) {
        result.bucket = bucket;
        result.cur = buckets_[bucket];
        return;
      }
    }
    result.bucket = buckets_.size();
    result.cur = 0;
  }
};

}  // namespace detail
}  // namespace lstl

#endif  // LSTL_HASHTABLE_H
