#ifndef LSTL_BPLUS_TREE_H
#define LSTL_BPLUS_TREE_H

#include <cstddef>

#include "internal/detail/iterator_facet.h"
#include "memory.h"

namespace lstl {
namespace detail {

template <typename Value>
struct bplus_tree_leaf;

struct bplus_tree_node_base {
  bool is_leaf;
  bplus_tree_node_base* parent;

  explicit bplus_tree_node_base(bool leaf) : is_leaf(leaf), parent(0) {}
};

template <typename Value>
struct bplus_tree_leaf : bplus_tree_node_base {
  typedef bplus_tree_leaf<Value> leaf_type;
  typedef Value value_type;

  size_t size;
  leaf_type* next;
  leaf_type* prev;

  value_type data[1];

  static size_t leaf_allocation_size(size_t order) {
    return sizeof(bplus_tree_leaf) + (order > 0 ? order - 1 : 0) * sizeof(value_type);
  }
};

template <typename Key>
struct bplus_tree_internal : bplus_tree_node_base {
  typedef Key key_type;

  size_t size;
  key_type keys[1];
  bplus_tree_node_base* children[2];

  static size_t internal_allocation_size(size_t order) {
    return sizeof(bplus_tree_internal) + (order > 0 ? order - 1 : 0) * sizeof(key_type) +
           (order > 0 ? order - 1 : 0) * sizeof(bplus_tree_node_base*);
  }
};

template <typename Value, typename Ref, typename Ptr>
struct bplus_tree_iterator {
  typedef bplus_tree_leaf<Value> leaf_type;
  typedef bplus_tree_iterator<Value, Value&, Value*> iterator;
  typedef bplus_tree_iterator<Value, const Value&, const Value*> const_iterator;
  typedef Ref reference;
  typedef Ptr pointer;
  typedef Value value_type;
  typedef ptrdiff_t difference_type;
  typedef detail::bidirectional_iterator_tag iterator_category;

  leaf_type* leaf;
  size_t index;

  bplus_tree_iterator() : leaf(0), index(0) {}

  bplus_tree_iterator(leaf_type* l, size_t i) : leaf(l), index(i) {}

  bplus_tree_iterator(const iterator& it) : leaf(it.leaf), index(it.index) {}

  bplus_tree_iterator& operator=(const bplus_tree_iterator& it) {
    leaf = it.leaf;
    index = it.index;
    return *this;
  }

  reference operator*() const { return leaf->data[index]; }
  pointer operator->() const { return &(operator*()); }

  bool operator==(const bplus_tree_iterator& x) const {
    return leaf == x.leaf && index == x.index;
  }
  bool operator!=(const bplus_tree_iterator& x) const { return !(*this == x); }

  bplus_tree_iterator& operator++() {
    ++index;
    if (index >= leaf->size) {
      leaf = leaf->next;
      index = 0;
    }
    return *this;
  }

  bplus_tree_iterator operator++(int) {
    bplus_tree_iterator tmp = *this;
    ++*this;
    return tmp;
  }

  bplus_tree_iterator& operator--() {
    if (index == 0) {
      if (leaf->prev == 0 || leaf->prev->size == 0) {
        return *this;
      }
      leaf = leaf->prev;
      index = leaf->size;
    }
    --index;
    return *this;
  }

  bplus_tree_iterator operator--(int) {
    bplus_tree_iterator tmp = *this;
    --*this;
    return tmp;
  }
};

template <typename Key, typename Value, typename KeyOfValue, typename Compare,
          typename Alloc = allocator<Value>, size_t Order = 8>
class bplus_tree {
 public:
  typedef Key key_type;
  typedef Value value_type;
  typedef Compare key_compare;
  typedef Alloc allocator_type;
  typedef value_type* pointer;
  typedef const value_type* const_pointer;
  typedef value_type& reference;
  typedef const value_type& const_reference;
  typedef bplus_tree_iterator<value_type, reference, pointer> iterator;
  typedef bplus_tree_iterator<value_type, const_reference, const_pointer> const_iterator;
  typedef typename allocator_traits<Alloc>::size_type size_type;
  typedef typename allocator_traits<Alloc>::difference_type difference_type;

  static const size_type kOrder = Order;
  static const size_type kMaxKeys = (kOrder > 2 ? kOrder : 2);
  static const size_type kMinKeys = (kMaxKeys + 1) / 2;

 protected:
  typedef bplus_tree_leaf<value_type> leaf_type;
  typedef bplus_tree_internal<key_type> internal_type;
  typedef bplus_tree_node_base node_base;
  typedef unsigned char raw_storage;

  typedef typename allocator_traits<Alloc>::template rebind_alloc<raw_storage>::other
      raw_allocator;

  raw_allocator raw_alloc_;
  size_type node_count_;
  key_compare key_comp_;
  node_base* root_;
  leaf_type* leftmost_;
  leaf_type* rightmost_;
  leaf_type* header_leaf_;

 public:
  bplus_tree() : node_count_(0), key_comp_(), root_(0), leftmost_(0), rightmost_(0), header_leaf_(0) {
    init_header();
  }

  explicit bplus_tree(const Compare& comp)
      : node_count_(0), key_comp_(comp), root_(0), leftmost_(0), rightmost_(0), header_leaf_(0) {
    init_header();
  }

  bplus_tree(const bplus_tree& other)
      : node_count_(0),
        raw_alloc_(allocator_traits<Alloc>::select_on_container_copy_construction(
            other.raw_alloc_)),
        key_comp_(other.key_comp_),
        root_(0),
        leftmost_(0),
        rightmost_(0),
        header_leaf_(0) {
    init_header();
    insert_equal(other.begin(), other.end());
  }

  bplus_tree(bplus_tree&& other) throw()
      : node_count_(other.node_count_),
        raw_alloc_(other.raw_alloc_),
        key_comp_(other.key_comp_),
        root_(other.root_),
        leftmost_(other.leftmost_),
        rightmost_(other.rightmost_),
        header_leaf_(other.header_leaf_) {
    if (other.leftmost_ != 0) {
      other.leftmost_->prev = header_leaf_;
      other.rightmost_->next = header_leaf_;
    }
    other.header_leaf_ = 0;
    other.node_count_ = 0;
    other.root_ = 0;
    other.leftmost_ = 0;
    other.rightmost_ = 0;
    other.init_header();
  }

  ~bplus_tree() {
    clear();
    destroy_header();
  }

  bplus_tree& operator=(const bplus_tree& other) {
    if (this != &other) {
      clear();
      insert_equal(other.begin(), other.end());
    }
    return *this;
  }

  bplus_tree& operator=(bplus_tree&& other) throw() {
    if (this != &other) {
      clear();
      node_count_ = other.node_count_;
      raw_alloc_ = other.raw_alloc_;
      key_comp_ = other.key_comp_;
      root_ = other.root_;
      leftmost_ = other.leftmost_;
      rightmost_ = other.rightmost_;
      header_leaf_ = other.header_leaf_;
      if (other.leftmost_ != 0) {
        other.leftmost_->prev = header_leaf_;
        other.rightmost_->next = header_leaf_;
      }
      other.header_leaf_ = 0;
      other.init_header();
      other.node_count_ = 0;
      other.root_ = 0;
      other.leftmost_ = 0;
      other.rightmost_ = 0;
    }
    return *this;
  }

  iterator begin() { return empty() ? end() : iterator(leftmost_, 0); }
  const_iterator begin() const { return empty() ? end() : const_iterator(leftmost_, 0); }

  iterator end() { return iterator(header_leaf_, 0); }
  const_iterator end() const { return const_iterator(header_leaf_, 0); }

  size_type size() const throw() { return node_count_; }
  size_type max_size() const throw() { return raw_alloc_.max_size(); }
  bool empty() const throw() { return node_count_ == 0; }

  Compare key_comp() const { return key_comp_; }

  iterator find(const Key& k) {
    leaf_type* leaf = 0;
    size_t pos = 0;
    if (!find_leaf(k, leaf, pos)) {
      return end();
    }
    return iterator(leaf, pos);
  }

  const_iterator find(const Key& k) const {
    leaf_type* leaf = 0;
    size_t pos = 0;
    if (!find_leaf(k, leaf, pos)) {
      return end();
    }
    return const_iterator(leaf, pos);
  }

  size_type count(const Key& k) const {
    pair<const_iterator, const_iterator> p = equal_range(k);
    return static_cast<size_type>(distance(p.first, p.second));
  }

  iterator lower_bound(const Key& k) {
    leaf_type* leaf = 0;
    size_t pos = 0;
    lower_bound_leaf(k, leaf, pos);
    if (leaf == header_leaf_) {
      return end();
    }
    return iterator(leaf, pos);
  }

  const_iterator lower_bound(const Key& k) const {
    leaf_type* leaf = 0;
    size_t pos = 0;
    lower_bound_leaf(k, leaf, pos);
    if (leaf == header_leaf_) {
      return end();
    }
    return const_iterator(leaf, pos);
  }

  iterator upper_bound(const Key& k) {
    iterator it = lower_bound(k);
    while (it != end() && !key_comp()(k, KeyOfValue()(*it))) {
      ++it;
    }
    return it;
  }

  const_iterator upper_bound(const Key& k) const {
    const_iterator it = lower_bound(k);
    while (it != end() && !key_comp()(k, KeyOfValue()(*it))) {
      ++it;
    }
    return it;
  }

  pair<iterator, iterator> equal_range(const Key& k) {
    return pair<iterator, iterator>(lower_bound(k), upper_bound(k));
  }

  pair<const_iterator, const_iterator> equal_range(const Key& k) const {
    return pair<const_iterator, const_iterator>(lower_bound(k), upper_bound(k));
  }

  pair<iterator, bool> insert_unique(const value_type& v) {
    if (empty()) {
      leaf_type* leaf = create_leaf();
      construct_value(leaf, 0, v);
      leaf->size = 1;
      set_singleton_root(leaf);
      ++node_count_;
      return pair<iterator, bool>(iterator(leaf, 0), true);
    }

    leaf_type* leaf = 0;
    size_t pos = 0;
    const bool found = find_leaf(KeyOfValue()(v), leaf, pos);
    if (found) {
      return pair<iterator, bool>(iterator(leaf, pos), false);
    }

    return insert_into_leaf(leaf, pos, v);
  }

  iterator insert_equal(const value_type& v) {
    if (empty()) {
      leaf_type* leaf = create_leaf();
      construct_value(leaf, 0, v);
      leaf->size = 1;
      set_singleton_root(leaf);
      ++node_count_;
      return iterator(leaf, 0);
    }

    leaf_type* leaf = 0;
    size_t pos = 0;
    (void)find_leaf(KeyOfValue()(v), leaf, pos);
    return insert_into_leaf(leaf, pos, v).first;
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
    if (position.leaf == header_leaf_) {
      return;
    }
    erase_from_leaf(position.leaf, position.index);
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
    if (node_count_ != 0) {
      destroy_subtree(root_);
      root_ = 0;
      leftmost_ = 0;
      rightmost_ = 0;
      init_header();
      node_count_ = 0;
    }
  }

  void swap(bplus_tree& other) throw() {
    lstl::swap(root_, other.root_);
    lstl::swap(node_count_, other.node_count_);
    lstl::swap(key_comp_, other.key_comp_);
    lstl::swap(leftmost_, other.leftmost_);
    lstl::swap(rightmost_, other.rightmost_);
    lstl::swap(header_leaf_, other.header_leaf_);
    if (leftmost_ != 0) {
      leftmost_->prev = header_leaf_;
    }
    if (rightmost_ != 0) {
      rightmost_->next = header_leaf_;
    }
    if (other.leftmost_ != 0) {
      other.leftmost_->prev = other.header_leaf_;
    }
    if (other.rightmost_ != 0) {
      other.rightmost_->next = other.header_leaf_;
    }
    raw_allocator tmp = raw_alloc_;
    raw_alloc_ = other.raw_alloc_;
    other.raw_alloc_ = tmp;
  }

 protected:
  void init_header() {
    if (header_leaf_ == 0) {
      header_leaf_ = create_leaf();
    }
    header_leaf_->size = 0;
    header_leaf_->next = header_leaf_;
    header_leaf_->prev = header_leaf_;
  }

  void destroy_header() {
    if (header_leaf_ != 0) {
      destroy_leaf(header_leaf_);
      header_leaf_ = 0;
    }
  }

  void set_singleton_root(leaf_type* leaf) {
    root_ = leaf;
    leaf->parent = 0;
    leftmost_ = leaf;
    rightmost_ = leaf;
    leaf->next = header_leaf_;
    leaf->prev = header_leaf_;
    header_leaf_->next = leaf;
    header_leaf_->prev = leaf;
  }

  void link_leaf_after(leaf_type* pos, leaf_type* leaf) {
    leaf->next = pos->next;
    leaf->prev = pos;
    pos->next->prev = leaf;
    pos->next = leaf;
    if (rightmost_ == pos) {
      rightmost_ = leaf;
    }
  }

  leaf_type* create_leaf() {
    raw_storage* block = raw_alloc_.allocate(leaf_type::leaf_allocation_size(kMaxKeys));
    leaf_type* leaf = reinterpret_cast<leaf_type*>(block);
    leaf->is_leaf = true;
    leaf->parent = 0;
    leaf->size = 0;
    leaf->next = 0;
    leaf->prev = 0;
    return leaf;
  }

  internal_type* create_internal() {
    raw_storage* block = raw_alloc_.allocate(internal_type::internal_allocation_size(kMaxKeys));
    internal_type* node = reinterpret_cast<internal_type*>(block);
    node->is_leaf = false;
    node->parent = 0;
    node->size = 0;
    return node;
  }

  void set_child_parent(node_base* child, node_base* parent) {
    if (child != 0) {
      child->parent = parent;
    }
  }

  key_type* internal_keys(internal_type* node) { return node->keys; }

  const key_type* internal_keys(const internal_type* node) const { return node->keys; }

  node_base** internal_children(internal_type* node) {
    return reinterpret_cast<node_base**>(node->keys + kMaxKeys);
  }

  node_base* const* internal_children(const internal_type* node) const {
    return reinterpret_cast<node_base* const*>(node->keys + kMaxKeys);
  }

  void construct_value(leaf_type* leaf, size_t pos, const value_type& v) {
    construct(leaf->data + pos, v);
  }

  void destroy_value(leaf_type* leaf, size_t pos) { destroy(leaf->data + pos); }

  void shift_leaf_right(leaf_type* leaf, size_t pos) {
    for (size_t i = leaf->size; i > pos; --i) {
      construct_value(leaf, i, leaf->data[i - 1]);
      destroy_value(leaf, i - 1);
    }
  }

  void shift_leaf_left(leaf_type* leaf, size_t pos) {
    destroy_value(leaf, pos);
    for (size_t i = pos; i + 1 < leaf->size; ++i) {
      construct_value(leaf, i, leaf->data[i + 1]);
      destroy_value(leaf, i + 1);
    }
    --leaf->size;
  }

  size_t lower_bound_pos(leaf_type* leaf, const Key& k) const {
    size_t lo = 0;
    size_t hi = leaf->size;
    while (lo < hi) {
      const size_t mid = lo + (hi - lo) / 2;
      if (key_comp()(KeyOfValue()(leaf->data[mid]), k)) {
        lo = mid + 1;
      } else {
        hi = mid;
      }
    }
    return lo;
  }

  bool find_leaf(const Key& k, leaf_type*& out_leaf, size_t& out_pos) const {
    if (root_ == 0) {
      out_leaf = header_leaf_;
      out_pos = 0;
      return false;
    }
    node_base* node = root_;
    while (!node->is_leaf) {
      const internal_type* in = static_cast<const internal_type*>(node);
      size_t idx = 0;
      while (idx < in->size && key_comp()(internal_keys(in)[idx], k)) {
        ++idx;
      }
      node = internal_children(in)[idx];
    }
    out_leaf = const_cast<leaf_type*>(static_cast<const leaf_type*>(node));
    out_pos = lower_bound_pos(out_leaf, k);
    return out_pos < out_leaf->size && !key_comp()(k, KeyOfValue()(out_leaf->data[out_pos])) &&
           !key_comp()(KeyOfValue()(out_leaf->data[out_pos]), k);
  }

  void lower_bound_leaf(const Key& k, leaf_type*& out_leaf, size_t& out_pos) const {
    if (root_ == 0) {
      out_leaf = header_leaf_;
      out_pos = 0;
      return;
    }
    node_base* node = root_;
    while (!node->is_leaf) {
      const internal_type* in = static_cast<const internal_type*>(node);
      size_t idx = 0;
      while (idx < in->size && key_comp()(internal_keys(in)[idx], k)) {
        ++idx;
      }
      node = internal_children(in)[idx];
    }
    out_leaf = const_cast<leaf_type*>(static_cast<const leaf_type*>(node));
    out_pos = lower_bound_pos(out_leaf, k);
    if (out_pos >= out_leaf->size) {
      if (out_leaf->next == header_leaf_) {
        out_leaf = header_leaf_;
        out_pos = 0;
      } else {
        out_leaf = out_leaf->next;
        out_pos = 0;
      }
    }
  }

  pair<iterator, bool> insert_into_leaf(leaf_type* leaf, size_t pos, const value_type& v) {
    if (leaf->size >= kMaxKeys) {
      split_leaf(leaf);
      (void)find_leaf(KeyOfValue()(v), leaf, pos);
    }

    shift_leaf_right(leaf, pos);
    construct_value(leaf, pos, v);
    ++leaf->size;
    ++node_count_;
    return pair<iterator, bool>(iterator(leaf, pos), true);
  }

  leaf_type* split_leaf(leaf_type* leaf) {
    const size_t split_at = (leaf->size + 1) / 2;
    leaf_type* right = create_leaf();
    right->size = leaf->size - split_at;
    leaf->size = split_at;

    for (size_t i = 0; i < right->size; ++i) {
      construct_value(right, i, leaf->data[split_at + i]);
      destroy_value(leaf, split_at + i);
    }

    right->next = leaf->next;
    right->prev = leaf;
    leaf->next->prev = right;
    leaf->next = right;
    if (rightmost_ == leaf) {
      rightmost_ = right;
    }

    const key_type promote = KeyOfValue()(right->data[0]);
    insert_into_parent(leaf, promote, right);
    return right;
  }

  void insert_into_parent(node_base* left, const key_type& key, node_base* right) {
    if (left == root_) {
      internal_type* parent = create_internal();
      internal_keys(parent)[0] = key;
      internal_children(parent)[0] = left;
      internal_children(parent)[1] = right;
      parent->size = 1;
      set_child_parent(left, parent);
      set_child_parent(right, parent);
      root_ = parent;
      return;
    }

    internal_type* parent = static_cast<internal_type*>(left->parent);
    if (parent->size >= kMaxKeys) {
      split_internal(parent);
      parent = static_cast<internal_type*>(left->parent);
    }

    size_t idx = 0;
    while (idx < parent->size && internal_children(parent)[idx] != left) {
      ++idx;
    }

    for (size_t i = parent->size; i > idx; --i) {
      internal_keys(parent)[i] = internal_keys(parent)[i - 1];
      internal_children(parent)[i + 1] = internal_children(parent)[i];
    }
    internal_keys(parent)[idx] = key;
    internal_children(parent)[idx + 1] = right;
    set_child_parent(right, parent);
    ++parent->size;
  }

  void split_internal(internal_type* node) {
    const size_t split_at = (node->size + 1) / 2;
    internal_type* right = create_internal();
    right->size = node->size - split_at;
    node->size = split_at - 1;

    for (size_t i = 0; i < right->size; ++i) {
      internal_keys(right)[i] = internal_keys(node)[split_at + i];
    }
    for (size_t i = 0; i <= right->size; ++i) {
      internal_children(right)[i] = internal_children(node)[split_at + i];
      set_child_parent(internal_children(right)[i], right);
    }

    const key_type promote = internal_keys(node)[split_at - 1];
    node_base* left = node;
    insert_into_parent(left, promote, right);
  }

  void erase_from_leaf(leaf_type* leaf, size_t pos) {
    shift_leaf_left(leaf, pos);
    --node_count_;

    if (leaf == root_ && leaf->is_leaf) {
      if (leaf->size == 0) {
        destroy_leaf(leaf);
        root_ = 0;
        leftmost_ = 0;
        rightmost_ = 0;
        init_header();
      }
      return;
    }

    if (leaf->size >= kMinKeys) {
      return;
    }

    bplus_tree rebuilt(key_comp_);
    for (const_iterator it = begin(); it != end(); ++it) {
      rebuilt.insert_equal(*it);
    }
    swap(rebuilt);
  }

  void destroy_leaf(leaf_type* leaf) {
    for (size_t i = 0; i < leaf->size; ++i) {
      destroy_value(leaf, i);
    }
    raw_alloc_.deallocate(reinterpret_cast<raw_storage*>(leaf),
                          leaf_type::leaf_allocation_size(kMaxKeys));
  }

  void destroy_internal(internal_type* node) {
    raw_alloc_.deallocate(reinterpret_cast<raw_storage*>(node),
                          internal_type::internal_allocation_size(kMaxKeys));
  }

  void destroy_subtree(node_base* node) {
    if (node == 0 || node == header_leaf_) {
      return;
    }
    if (node->is_leaf) {
      destroy_leaf(static_cast<leaf_type*>(node));
      return;
    }
    internal_type* in = static_cast<internal_type*>(node);
    for (size_t i = 0; i <= in->size; ++i) {
      destroy_subtree(internal_children(in)[i]);
    }
    destroy_internal(in);
  }
};

}  // namespace detail
}  // namespace lstl

#endif  // LSTL_BPLUS_TREE_H
