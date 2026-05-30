#ifndef LSTL_SKIP_LIST_H
#define LSTL_SKIP_LIST_H

#include <cstddef>

#include "internal/detail/iterator_facet.h"
#include "memory.h"

namespace lstl {
namespace detail {

enum { kSkipListMaxLevel = 16 };

template <typename Value>
struct skip_list_node {
  typedef skip_list_node<Value>* link_type;

  Value value_field;
  int level;
  link_type forward[kSkipListMaxLevel];
  link_type prev;

  skip_list_node() : level(1), prev(0) {
    for (int i = 0; i < kSkipListMaxLevel; ++i) {
      forward[i] = 0;
    }
  }
};

template <typename Value>
struct skip_list_iterator_base {
  typedef skip_list_node<Value>* link_type;
  typedef ptrdiff_t difference_type;
  typedef detail::bidirectional_iterator_tag iterator_category;

  link_type node;

  skip_list_iterator_base() : node(0) {}
  explicit skip_list_iterator_base(link_type x) : node(x) {}

  void increment() { node = node->forward[0]; }

  void decrement() { node = node->prev; }

  bool operator==(const skip_list_iterator_base& x) const { return node == x.node; }
  bool operator!=(const skip_list_iterator_base& x) const { return node != x.node; }
};

template <typename Value, typename Ref, typename Ptr>
struct skip_list_iterator : skip_list_iterator_base<Value> {
  typedef skip_list_node<Value>* link_type;
  typedef skip_list_iterator<Value, Value&, Value*> iterator;
  typedef skip_list_iterator<Value, const Value&, const Value*> const_iterator;
  typedef Ref reference;
  typedef Ptr pointer;
  typedef Value value_type;
  typedef ptrdiff_t difference_type;
  typedef detail::bidirectional_iterator_tag iterator_category;

  skip_list_iterator() {}
  explicit skip_list_iterator(link_type x) : skip_list_iterator_base<Value>(x) {}
  skip_list_iterator(const iterator& it) : skip_list_iterator_base<Value>(it.node) {}

  reference operator*() const { return this->node->value_field; }
  pointer operator->() const { return &(operator*()); }

  skip_list_iterator& operator++() {
    this->increment();
    return *this;
  }

  skip_list_iterator operator++(int) {
    skip_list_iterator tmp = *this;
    this->increment();
    return tmp;
  }

  skip_list_iterator& operator--() {
    this->decrement();
    return *this;
  }

  skip_list_iterator operator--(int) {
    skip_list_iterator tmp = *this;
    this->decrement();
    return tmp;
  }
};

inline int skip_list_random_level() {
  static unsigned seed = 0x9e3779b9u;
  int lvl = 1;
  while (lvl < kSkipListMaxLevel) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    if ((seed & 0xFFu) >= 128u) {
      break;
    }
    ++lvl;
  }
  return lvl;
}

template <typename Key, typename Value, typename KeyOfValue, typename Compare,
          typename Alloc = allocator<Value> >
class skip_list {
 protected:
  typedef skip_list_node<Value> node;
  typedef typename skip_list_node<Value>::link_type link_type;
  typedef typename allocator_traits<Alloc>::template rebind_alloc<node>::other node_allocator;

 public:
  typedef Key key_type;
  typedef Value value_type;
  typedef Compare key_compare;
  typedef Alloc allocator_type;
  typedef value_type* pointer;
  typedef const value_type* const_pointer;
  typedef value_type& reference;
  typedef const value_type& const_reference;
  typedef skip_list_iterator<value_type, reference, pointer> iterator;
  typedef skip_list_iterator<value_type, const_reference, const_pointer> const_iterator;
  typedef typename allocator_traits<Alloc>::size_type size_type;
  typedef typename allocator_traits<Alloc>::difference_type difference_type;

 protected:
  node_allocator node_alloc_;
  size_type node_count_;
  key_compare key_comp_;
  node header_;
  int level_;

 public:
  skip_list() : node_count_(0), key_comp_(), level_(1) { empty_initialize(); }

  explicit skip_list(const Compare& comp) : node_count_(0), key_comp_(comp), level_(1) {
    empty_initialize();
  }

  skip_list(const skip_list& other)
      : node_count_(0),
        node_alloc_(allocator_traits<Alloc>::select_on_container_copy_construction(
            other.node_alloc_)),
        key_comp_(other.key_comp_),
        level_(1) {
    empty_initialize();
    insert_equal(other.begin(), other.end());
  }

  skip_list(skip_list&& other) throw() : node_count_(0), key_comp_(), level_(1) {
    empty_initialize();
    swap(other);
  }

  ~skip_list() { clear(); }

  skip_list& operator=(const skip_list& other) {
    if (this != &other) {
      clear();
      insert_equal(other.begin(), other.end());
    }
    return *this;
  }

  skip_list& operator=(skip_list&& other) throw() {
    if (this != &other) {
      clear();
      swap(other);
    }
    return *this;
  }

  iterator begin() { return empty() ? end() : iterator(header_.forward[0]); }
  const_iterator begin() const {
    return empty() ? end() : const_iterator(header_.forward[0]);
  }

  iterator end() { return iterator(0); }
  const_iterator end() const { return const_iterator(0); }

  size_type size() const throw() { return node_count_; }
  size_type max_size() const throw() { return node_alloc_.max_size(); }
  bool empty() const throw() { return node_count_ == 0; }

  Compare key_comp() const { return key_comp_; }

  iterator find(const Key& k) {
    link_type x = search_level0_lower(k);
    if (x != 0 && !key_comp()(k, KeyOfValue()(x->value_field)) &&
        !key_comp()(KeyOfValue()(x->value_field), k)) {
      return iterator(x);
    }
    return end();
  }

  const_iterator find(const Key& k) const {
    link_type x = search_level0_lower(k);
    if (x != 0 && !key_comp()(k, KeyOfValue()(x->value_field)) &&
        !key_comp()(KeyOfValue()(x->value_field), k)) {
      return const_iterator(x);
    }
    return end();
  }

  size_type count(const Key& k) const {
    pair<const_iterator, const_iterator> p = equal_range(k);
    return static_cast<size_type>(distance(p.first, p.second));
  }

  iterator lower_bound(const Key& k) {
    link_type x = search_level0_lower(k);
    return x == 0 ? end() : iterator(x);
  }

  const_iterator lower_bound(const Key& k) const {
    link_type x = search_level0_lower(k);
    return x == 0 ? end() : const_iterator(x);
  }

  iterator upper_bound(const Key& k) {
    link_type x = search_level0_upper(k);
    return x == 0 ? end() : iterator(x);
  }

  const_iterator upper_bound(const Key& k) const {
    link_type x = search_level0_upper(k);
    return x == 0 ? end() : const_iterator(x);
  }

  pair<iterator, iterator> equal_range(const Key& k) {
    return pair<iterator, iterator>(lower_bound(k), upper_bound(k));
  }

  pair<const_iterator, const_iterator> equal_range(const Key& k) const {
    return pair<const_iterator, const_iterator>(lower_bound(k), upper_bound(k));
  }

  pair<iterator, bool> insert_unique(const value_type& v) {
    link_type update[kSkipListMaxLevel];
    search_lower(v, update);
    link_type x = update[0]->forward[0];
    if (x != 0 && !key_comp()(KeyOfValue()(v), KeyOfValue()(x->value_field)) &&
        !key_comp()(KeyOfValue()(x->value_field), KeyOfValue()(v))) {
      return pair<iterator, bool>(iterator(x), false);
    }
    const int new_level = skip_list_random_level();
    node* z = create_node(v, new_level);
    insert_node(update, z, new_level);
    ++node_count_;
    return pair<iterator, bool>(iterator(z), true);
  }

  iterator insert_equal(const value_type& v) {
    link_type update[kSkipListMaxLevel];
    search_upper(v, update);
    const int new_level = skip_list_random_level();
    node* z = create_node(v, new_level);
    insert_node(update, z, new_level);
    ++node_count_;
    return iterator(z);
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

  void erase(iterator position) {
    node* z = static_cast<node*>(position.node);
    if (z == 0) {
      return;
    }
    erase_node(z);
    --node_count_;
  }

  size_type erase(const Key& k) {
    pair<iterator, iterator> p = equal_range(k);
    const size_type n = static_cast<size_type>(distance(p.first, p.second));
    erase(p.first, p.second);
    return n;
  }

  iterator erase(iterator first, iterator last) {
    if (first == begin() && last == end()) {
      clear();
    } else {
      while (first != last) {
        erase(first++);
      }
    }
    return last;
  }

  void clear() {
    link_type x = header_.forward[0];
    while (x != 0) {
      link_type next = x->forward[0];
      destroy_node(x);
      x = next;
    }
    empty_initialize();
    node_count_ = 0;
  }

  void swap(skip_list& other) throw() {
    lstl::swap(level_, other.level_);
    lstl::swap(node_count_, other.node_count_);
    lstl::swap(key_comp_, other.key_comp_);
    for (int i = 0; i < kSkipListMaxLevel; ++i) {
      lstl::swap(header_.forward[i], other.header_.forward[i]);
    }
    lstl::swap(header_.prev, other.header_.prev);
    node_allocator tmp = node_alloc_;
    node_alloc_ = other.node_alloc_;
    other.node_alloc_ = tmp;
  }

 protected:
  void empty_initialize() {
    level_ = 1;
    header_.level = kSkipListMaxLevel;
    header_.prev = 0;
    for (int i = 0; i < kSkipListMaxLevel; ++i) {
      header_.forward[i] = 0;
    }
  }

  node* create_node(const value_type& v, int node_level) {
    node* z = node_alloc_.allocate(1);
    z->level = node_level;
    for (int i = 0; i < kSkipListMaxLevel; ++i) {
      z->forward[i] = 0;
    }
    z->prev = 0;
    construct(&z->value_field, v);
    return z;
  }

  void destroy_node(node* p) {
    destroy(&p->value_field);
    node_alloc_.deallocate(p, 1);
  }

  link_type search_level0_lower(const Key& k) const {
    link_type x = header_.forward[0];
    while (x != 0 && key_comp()(KeyOfValue()(x->value_field), k)) {
      x = x->forward[0];
    }
    return x;
  }

  link_type search_level0_upper(const Key& k) const {
    link_type x = header_.forward[0];
    while (x != 0 && !key_comp()(k, KeyOfValue()(x->value_field))) {
      x = x->forward[0];
    }
    return x;
  }

  void search_lower(const value_type& v, link_type update[]) const {
    link_type x = const_cast<link_type>(&header_);
    for (int i = level_ - 1; i >= 0; --i) {
      while (x->forward[i] != 0 &&
             key_comp()(KeyOfValue()(x->forward[i]->value_field), KeyOfValue()(v))) {
        x = x->forward[i];
      }
      update[i] = x;
    }
  }

  void search_upper(const value_type& v, link_type update[]) const {
    link_type x = const_cast<link_type>(&header_);
    for (int i = level_ - 1; i >= 0; --i) {
      while (x->forward[i] != 0 &&
             !key_comp()(KeyOfValue()(v), KeyOfValue()(x->forward[i]->value_field))) {
        x = x->forward[i];
      }
      update[i] = x;
    }
  }

  void insert_node(link_type update[], node* z, int new_level) {
    if (new_level > level_) {
      for (int i = level_; i < new_level; ++i) {
        update[i] = &header_;
      }
      level_ = new_level;
    }
    for (int i = 0; i < new_level; ++i) {
      z->forward[i] = update[i]->forward[i];
      update[i]->forward[i] = z;
    }
    z->prev = update[0];
    if (z->forward[0] != 0) {
      z->forward[0]->prev = z;
    }
  }

  void erase_node(node* z) {
    for (int i = 0; i < z->level; ++i) {
      link_type x = &header_;
      while (x->forward[i] != 0 && x->forward[i] != z) {
        x = x->forward[i];
      }
      if (x->forward[i] == z) {
        x->forward[i] = z->forward[i];
      }
    }
    if (z->forward[0] != 0) {
      z->forward[0]->prev = z->prev;
    }
    while (level_ > 1 && header_.forward[level_ - 1] == 0) {
      --level_;
    }
    destroy_node(z);
  }
};

}  // namespace detail
}  // namespace lstl

#endif  // LSTL_SKIP_LIST_H
