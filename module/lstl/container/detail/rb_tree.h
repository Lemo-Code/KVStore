#ifndef LSTL_RB_TREE_H
#define LSTL_RB_TREE_H

#include <cstddef>

#include "detail/key_of_value.h"
#include "internal/detail/iterator_facet.h"
#include "memory.h"

namespace lstl {
namespace detail {

enum rb_tree_color { rb_red = 0, rb_black = 1 };

struct rb_tree_node_base {
  typedef rb_tree_color color_type;
  typedef rb_tree_node_base* base_ptr;

  color_type color;
  base_ptr parent;
  base_ptr left;
  base_ptr right;

  static base_ptr minimum(base_ptr x) {
    while (x->left != 0) {
      x = x->left;
    }
    return x;
  }

  static base_ptr maximum(base_ptr x) {
    while (x->right != 0) {
      x = x->right;
    }
    return x;
  }
};

template <typename Value>
struct rb_tree_node : rb_tree_node_base {
  Value value_field;
};

struct rb_tree_iterator_base {
  typedef rb_tree_node_base* base_ptr;
  typedef ptrdiff_t difference_type;
  typedef detail::bidirectional_iterator_tag iterator_category;

  base_ptr node;

  rb_tree_iterator_base() : node(0) {}
  explicit rb_tree_iterator_base(base_ptr x) : node(x) {}

  void increment() {
    if (node->right != 0) {
      node = rb_tree_node_base::minimum(node->right);
    } else {
      base_ptr parent = node->parent;
      while (node == parent->right) {
        node = parent;
        parent = parent->parent;
      }
      if (node->right != parent) {
        node = parent;
      }
    }
  }

  void decrement() {
    if (node->color == rb_red && node->parent->parent == node) {
      node = node->right;
    } else if (node->left != 0) {
      node = rb_tree_node_base::maximum(node->left);
    } else {
      base_ptr parent = node->parent;
      while (node == parent->left) {
        node = parent;
        parent = parent->parent;
      }
      node = parent;
    }
  }

  bool operator==(const rb_tree_iterator_base& x) const { return node == x.node; }
  bool operator!=(const rb_tree_iterator_base& x) const { return node != x.node; }
};

template <typename Value, typename Ref, typename Ptr>
struct rb_tree_iterator : rb_tree_iterator_base {
  typedef rb_tree_iterator<Value, Value&, Value*> iterator;
  typedef rb_tree_iterator<Value, const Value&, const Value*> const_iterator;
  typedef rb_tree_node<Value>* link_type;
  typedef Ref reference;
  typedef Ptr pointer;
  typedef Value value_type;
  typedef ptrdiff_t difference_type;
  typedef detail::bidirectional_iterator_tag iterator_category;

  rb_tree_iterator() {}
  explicit rb_tree_iterator(link_type x) : rb_tree_iterator_base(x) {}
  rb_tree_iterator(const iterator& it) : rb_tree_iterator_base(it.node) {}

  reference operator*() const { return static_cast<link_type>(node)->value_field; }
  pointer operator->() const { return &(operator*()); }

  rb_tree_iterator& operator++() {
    increment();
    return *this;
  }

  rb_tree_iterator operator++(int) {
    rb_tree_iterator tmp = *this;
    increment();
    return tmp;
  }

  rb_tree_iterator& operator--() {
    decrement();
    return *this;
  }

  rb_tree_iterator operator--(int) {
    rb_tree_iterator tmp = *this;
    decrement();
    return tmp;
  }
};

inline void rb_tree_rotate_left(rb_tree_node_base* x, rb_tree_node_base*& root) {
  rb_tree_node_base* y = x->right;
  x->right = y->left;
  if (y->left != 0) {
    y->left->parent = x;
  }
  y->parent = x->parent;
  if (x == root) {
    root = y;
  } else if (x == x->parent->left) {
    x->parent->left = y;
  } else {
    x->parent->right = y;
  }
  y->left = x;
  x->parent = y;
}

inline void rb_tree_rotate_right(rb_tree_node_base* x, rb_tree_node_base*& root) {
  rb_tree_node_base* y = x->left;
  x->left = y->right;
  if (y->right != 0) {
    y->right->parent = x;
  }
  y->parent = x->parent;
  if (x == root) {
    root = y;
  } else if (x == x->parent->right) {
    x->parent->right = y;
  } else {
    x->parent->left = y;
  }
  y->right = x;
  x->parent = y;
}

inline void rb_tree_rebalance(rb_tree_node_base* x, rb_tree_node_base*& root) {
  x->color = rb_red;
  while (x != root && x->parent->color == rb_red) {
    if (x->parent == x->parent->parent->left) {
      rb_tree_node_base* y = x->parent->parent->right;
      if (y && y->color == rb_red) {
        x->parent->color = rb_black;
        y->color = rb_black;
        x->parent->parent->color = rb_red;
        x = x->parent->parent;
      } else {
        if (x == x->parent->right) {
          x = x->parent;
          rb_tree_rotate_left(x, root);
        }
        x->parent->color = rb_black;
        x->parent->parent->color = rb_red;
        rb_tree_rotate_right(x->parent->parent, root);
      }
    } else {
      rb_tree_node_base* y = x->parent->parent->left;
      if (y && y->color == rb_red) {
        x->parent->color = rb_black;
        y->color = rb_black;
        x->parent->parent->color = rb_red;
        x = x->parent->parent;
      } else {
        if (x == x->parent->left) {
          x = x->parent;
          rb_tree_rotate_right(x, root);
        }
        x->parent->color = rb_black;
        x->parent->parent->color = rb_red;
        rb_tree_rotate_left(x->parent->parent, root);
      }
    }
  }
  root->color = rb_black;
}

inline bool rb_tree_is_left_child(rb_tree_node_base* node) {
  return node == node->parent->left;
}

inline void rb_tree_rebalance_after_erase_with_sibling(rb_tree_node_base* w,
                                                       rb_tree_node_base*& root) {
  while (true) {
    if (!rb_tree_is_left_child(w)) {
      if (w->color == rb_red) {
        w->color = rb_black;
        w->parent->color = rb_red;
        rb_tree_rotate_left(w->parent, root);
        if (root == w->left) {
          root = w;
        }
        w = w->left->right;
      }
      if ((w->left == 0 || w->left->color == rb_black) &&
          (w->right == 0 || w->right->color == rb_black)) {
        w->color = rb_red;
        rb_tree_node_base* x = w->parent;
        if (x == root || x->color == rb_red) {
          x->color = rb_black;
          break;
        }
        w = rb_tree_is_left_child(x) ? x->parent->right : x->parent->left;
      } else {
        if (w->right == 0 || w->right->color == rb_black) {
          w->left->color = rb_black;
          w->color = rb_red;
          rb_tree_rotate_right(w, root);
          w = w->parent;
        }
        w->color = w->parent->color;
        w->parent->color = rb_black;
        w->right->color = rb_black;
        rb_tree_rotate_left(w->parent, root);
        break;
      }
    } else {
      if (w->color == rb_red) {
        w->color = rb_black;
        w->parent->color = rb_red;
        rb_tree_rotate_right(w->parent, root);
        if (root == w->right) {
          root = w;
        }
        w = w->right->left;
      }
      if ((w->left == 0 || w->left->color == rb_black) &&
          (w->right == 0 || w->right->color == rb_black)) {
        w->color = rb_red;
        rb_tree_node_base* x = w->parent;
        if (x == root || x->color == rb_red) {
          x->color = rb_black;
          break;
        }
        w = rb_tree_is_left_child(x) ? x->parent->right : x->parent->left;
      } else {
        if (w->left == 0 || w->left->color == rb_black) {
          w->right->color = rb_black;
          w->color = rb_red;
          rb_tree_rotate_left(w, root);
          w = w->parent;
        }
        w->color = w->parent->color;
        w->parent->color = rb_black;
        w->left->color = rb_black;
        rb_tree_rotate_right(w->parent, root);
        break;
      }
    }
  }
}

inline rb_tree_node_base* rb_tree_rebalance_for_erase(rb_tree_node_base* z,
                                                      rb_tree_node_base*& root,
                                                      rb_tree_node_base* header,
                                                      rb_tree_node_base*& leftmost,
                                                      rb_tree_node_base*& rightmost) {
  rb_tree_node_base* y = z;
  rb_tree_node_base* x = 0;
  if (y->left == 0) {
    x = y->right;
  } else if (y->right == 0) {
    x = y->left;
  } else {
    y = rb_tree_node_base::minimum(y->right);
    x = y->right;
  }
  rb_tree_node_base* w = 0;

  if (x != 0) {
    x->parent = y->parent;
  }
  if (y == leftmost) {
    leftmost = (x != 0) ? rb_tree_node_base::minimum(x) : y->parent;
  }
  if (y == rightmost) {
    rightmost = (x != 0) ? rb_tree_node_base::maximum(x) : y->parent;
  }

  if (y == root) {
    root = x;
  } else if (rb_tree_is_left_child(y)) {
    y->parent->left = x;
    if (y != root) {
      w = y->parent->right;
    }
  } else {
    y->parent->right = x;
    w = y->parent->left;
  }

  const bool removed_black = (y->color == rb_black);

  if (y != z) {
    static_cast<rb_tree_node_base*>(z->left)->parent = y;
    y->left = z->left;
    y->right = z->right;
    if (y->right != 0) {
      static_cast<rb_tree_node_base*>(y->right)->parent = y;
    }
    y->parent = z->parent;
    y->color = z->color;
    if (z == root) {
      root = y;
    } else if (z == z->parent->left) {
      z->parent->left = y;
    } else {
      z->parent->right = y;
    }
    if (header->parent == z) {
      header->parent = y;
    }
  }

  if (removed_black && root != 0) {
    if (x != 0) {
      x->color = rb_black;
    } else if (w != 0) {
      rb_tree_rebalance_after_erase_with_sibling(w, root);
    }
  }

  return z;
}

template <typename Key, typename Value, typename KeyOfValue, typename Compare,
          typename Alloc = allocator<Value> >
class rb_tree {
 protected:
  typedef rb_tree_node_base* base_ptr;
  typedef rb_tree_node<Value> node;
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
  typedef rb_tree_iterator<value_type, reference, pointer> iterator;
  typedef rb_tree_iterator<value_type, const_reference, const_pointer> const_iterator;
  typedef typename allocator_traits<Alloc>::size_type size_type;
  typedef typename allocator_traits<Alloc>::difference_type difference_type;

 protected:
  node_allocator node_alloc_;
  size_type node_count_;
  key_compare key_comp_;
  rb_tree_node_base header_;
  base_ptr root_;

 public:
  rb_tree() : node_count_(0), key_comp_() { empty_initialize(); }

  explicit rb_tree(const Compare& comp) : node_count_(0), key_comp_(comp) { empty_initialize(); }

  rb_tree(const rb_tree& other)
      : node_count_(0),
        node_alloc_(allocator_traits<Alloc>::select_on_container_copy_construction(
            other.node_alloc_)),
        key_comp_(other.key_comp_) {
    empty_initialize();
    insert_equal(other.begin(), other.end());
  }

  rb_tree(rb_tree&& other) throw()
      : node_count_(other.node_count_),
        node_alloc_(other.node_alloc_),
        key_comp_(other.key_comp_),
        root_(other.root_) {
    header_.color = other.header_.color;
    header_.parent = other.header_.parent;
    header_.left = other.header_.left;
    header_.right = other.header_.right;
    if (other.root_ == 0) {
      empty_initialize();
    } else {
      rebind_header_links();
      other.empty_initialize();
    }
    other.node_count_ = 0;
  }

  ~rb_tree() { clear(); }

  rb_tree& operator=(const rb_tree& other) {
    if (this != &other) {
      clear();
      insert_equal(other.begin(), other.end());
    }
    return *this;
  }

  rb_tree& operator=(rb_tree&& other) throw() {
    if (this != &other) {
      clear();
      root_ = other.root_;
      node_count_ = other.node_count_;
      node_alloc_ = other.node_alloc_;
      key_comp_ = other.key_comp_;
      header_.color = other.header_.color;
      header_.parent = other.header_.parent;
      header_.left = other.header_.left;
      header_.right = other.header_.right;
      rebind_header_links();
      other.empty_initialize();
      other.node_count_ = 0;
    }
    return *this;
  }

  iterator begin() {
    return empty() ? end() : iterator(static_cast<node*>(rb_tree_node_base::minimum(root_)));
  }
  const_iterator begin() const {
    return empty() ? end()
                     : const_iterator(static_cast<node*>(
                           const_cast<base_ptr>(rb_tree_node_base::minimum(root_))));
  }

  iterator end() { return iterator(static_cast<node*>(&header_)); }
  const_iterator end() const {
    return const_iterator(static_cast<node*>(const_cast<base_ptr>(&header_)));
  }

  size_type size() const throw() { return node_count_; }
  size_type max_size() const throw() { return node_alloc_.max_size(); }
  bool empty() const throw() { return node_count_ == 0; }

  Compare key_comp() const { return key_comp_; }

  iterator find(const Key& k) {
    base_ptr y = &header_;
    base_ptr x = root_;
    while (x != 0) {
      if (key_comp()(KeyOfValue()(static_cast<node*>(x)->value_field), k)) {
        x = x->right;
      } else {
        y = x;
        x = x->left;
      }
    }
    iterator j = iterator(static_cast<node*>(y));
    return (j == end() || key_comp()(k, KeyOfValue()(*j))) ? end() : j;
  }

  const_iterator find(const Key& k) const {
    base_ptr y = const_cast<base_ptr>(&header_);
    base_ptr x = root_;
    while (x != 0) {
      if (key_comp()(KeyOfValue()(static_cast<node*>(x)->value_field), k)) {
        x = x->right;
      } else {
        y = x;
        x = x->left;
      }
    }
    const_iterator j = const_iterator(static_cast<node*>(y));
    return (j == end() || key_comp()(k, KeyOfValue()(*j))) ? end() : j;
  }

  size_type count(const Key& k) const {
    pair<const_iterator, const_iterator> p = equal_range(k);
    return static_cast<size_type>(distance(p.first, p.second));
  }

  iterator lower_bound(const Key& k) {
    base_ptr y = &header_;
    base_ptr x = root_;
    while (x != 0) {
      if (key_comp()(KeyOfValue()(static_cast<node*>(x)->value_field), k)) {
        x = x->right;
      } else {
        y = x;
        x = x->left;
      }
    }
    iterator j = iterator(static_cast<node*>(y));
    return (j == end() || key_comp()(KeyOfValue()(*j), k)) ? end() : j;
  }

  const_iterator lower_bound(const Key& k) const {
    base_ptr y = const_cast<base_ptr>(&header_);
    base_ptr x = root_;
    while (x != 0) {
      if (key_comp()(KeyOfValue()(static_cast<node*>(x)->value_field), k)) {
        x = x->right;
      } else {
        y = x;
        x = x->left;
      }
    }
    const_iterator j = const_iterator(static_cast<node*>(y));
    return (j == end() || key_comp()(KeyOfValue()(*j), k)) ? end() : j;
  }

  iterator upper_bound(const Key& k) {
    base_ptr y = &header_;
    base_ptr x = root_;
    while (x != 0) {
      if (key_comp()(k, KeyOfValue()(static_cast<node*>(x)->value_field))) {
        y = x;
        x = x->left;
      } else {
        x = x->right;
      }
    }
    return iterator(static_cast<node*>(y));
  }

  const_iterator upper_bound(const Key& k) const {
    base_ptr y = const_cast<base_ptr>(&header_);
    base_ptr x = root_;
    while (x != 0) {
      if (key_comp()(k, KeyOfValue()(static_cast<node*>(x)->value_field))) {
        y = x;
        x = x->left;
      } else {
        x = x->right;
      }
    }
    return const_iterator(static_cast<node*>(y));
  }

  pair<iterator, iterator> equal_range(const Key& k) {
    return pair<iterator, iterator>(lower_bound(k), upper_bound(k));
  }

  pair<const_iterator, const_iterator> equal_range(const Key& k) const {
    return pair<const_iterator, const_iterator>(lower_bound(k), upper_bound(k));
  }

  pair<iterator, bool> insert_unique(const value_type& v) {
    base_ptr y = &header_;
    base_ptr x = root_;
    bool insert_left = true;
    while (x != 0) {
      y = x;
      insert_left = key_comp()(KeyOfValue()(v), KeyOfValue()(static_cast<node*>(x)->value_field));
      x = insert_left ? x->left : x->right;
    }
    if (!insert_left) {
      iterator j(static_cast<node*>(y));
      if (j != end() && !key_comp()(KeyOfValue()(*j), KeyOfValue()(v)) &&
          !key_comp()(KeyOfValue()(v), KeyOfValue()(*j))) {
        return pair<iterator, bool>(j, false);
      }
      return pair<iterator, bool>(insert_node(y, create_node(v), false), true);
    }
    iterator j(static_cast<node*>(y));
    if (j != begin()) {
      iterator prev = j;
      --prev;
      if (!key_comp()(KeyOfValue()(v), KeyOfValue()(*prev))) {
        return pair<iterator, bool>(prev, false);
      }
    }
    return pair<iterator, bool>(insert_node(y, create_node(v), true), true);
  }

  iterator insert_equal(const value_type& v) {
    base_ptr y = &header_;
    base_ptr x = root_;
    bool insert_left = true;
    while (x != 0) {
      y = x;
      insert_left = key_comp()(KeyOfValue()(v), KeyOfValue()(static_cast<node*>(x)->value_field));
      x = insert_left ? x->left : x->right;
    }
    return insert_node(y, create_node(v), insert_left);
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
    if (z == static_cast<node*>(&header_)) {
      return;
    }
    base_ptr y = rb_tree_rebalance_for_erase(z, root_, &header_, header_.left, header_.right);
    destroy_node(static_cast<node*>(y));
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
    if (node_count_ != 0) {
      erase_aux(root_);
      root_ = 0;
      header_.left = header_.right = &header_;
      header_.parent = 0;
      node_count_ = 0;
    }
  }

  void swap(rb_tree& other) throw() {
    lstl::swap(root_, other.root_);
    lstl::swap(node_count_, other.node_count_);
    lstl::swap(key_comp_, other.key_comp_);
    lstl::swap(header_.parent, other.header_.parent);
    lstl::swap(header_.left, other.header_.left);
    lstl::swap(header_.right, other.header_.right);
    lstl::swap(header_.color, other.header_.color);
    node_allocator tmp = node_alloc_;
    node_alloc_ = other.node_alloc_;
    other.node_alloc_ = tmp;
  }

 protected:
  void empty_initialize() {
    header_.color = rb_red;
    header_.parent = 0;
    header_.left = &header_;
    header_.right = &header_;
    root_ = 0;
  }

  void rebind_header_links() {
    if (root_ != 0) {
      root_->parent = &header_;
      header_.parent = root_;
      header_.left = rb_tree_node_base::minimum(root_);
      header_.right = rb_tree_node_base::maximum(root_);
    }
  }

  node* create_node(const value_type& v) {
    node* z = node_alloc_.allocate(1);
    construct(&z->value_field, v);
    return z;
  }

  void destroy_node(node* p) {
    destroy(&p->value_field);
    node_alloc_.deallocate(p, 1);
  }

  iterator insert_node(base_ptr parent, node* z, bool insert_left) {
    z->parent = parent;
    z->left = 0;
    z->right = 0;
    z->color = rb_red;
    if (parent == &header_) {
      root_ = z;
      header_.parent = z;
      header_.left = z;
      header_.right = z;
    } else if (insert_left) {
      parent->left = z;
      if (parent == header_.left) {
        header_.left = z;
      }
    } else {
      parent->right = z;
      if (parent == header_.right) {
        header_.right = z;
      }
    }
    rb_tree_rebalance(z, root_);
    ++node_count_;
    return iterator(z);
  }

  void erase_aux(base_ptr x) {
    while (x != 0) {
      erase_aux(x->right);
      node* n = static_cast<node*>(x);
      x = x->left;
      destroy_node(n);
    }
  }
};

}  // namespace detail
}  // namespace lstl

#endif  // LSTL_RB_TREE_H
