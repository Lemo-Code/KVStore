/**
 * @file    rb_tree.h
 * @brief   Red-black tree — balanced binary search tree with O(log n) operations.
 * @author  lstl team
 * @date    2025
 *
 * Implements an iterative (non-recursive) red-black tree per CLRS chapter 13.
 * Serves as the backend for map, set, multimap, and multiset.
 *
 * RB-tree invariants (maintained by insert_fixup / erase_fixup):
 * 1. Every node is red or black.
 * 2. Root is always black.
 * 3. Leaves (nullptr) are black.
 * 4. A red node's children are always black (no double-red).
 * 5. All paths from root to leaf have equal black-node count.
 *
 * The sentinel node (header_) serves dual purpose:
 * - Acts as the end() iterator marker.
 * - Stores root() in header_.parent, leftmost() in header_.left,
 *   and rightmost() in header_.right for O(1) begin() and end().
 *
 * @tparam Key         Key type for lookups.
 * @tparam Value       Stored value type (pair<const Key, T> for map, Key for set).
 * @tparam KeyOfValue  Functor extracting Key from Value.
 * @tparam Compare     Strict weak ordering on keys.
 * @tparam Alloc       Allocator for tree nodes.
 *
 * @ingroup container_detail
 */

#pragma once

#include <cstddef>
#include <iterator>
#include <utility>

#include "../../memory/type_traits.h"
#include "../../memory/utility.h"
#include "../../memory/construct.h"
#include "../../memory/allocator.h"

namespace lstl {
namespace detail {

// =========================================================================
// Color constants
// =========================================================================

/** @brief  Red-black tree node color. */
enum rb_tree_color {
    rb_red = false,   ///< Red node.
    rb_black = true   ///< Black node.
};

// =========================================================================
// rb_tree_node_base — non-templated base to reduce code bloat
// =========================================================================

/**
 * @brief  Non-templated base struct for RB-tree nodes.
 *
 * Contains the structural pointers (parent, left, right, color) and
 * static helper methods (minimum, maximum, successor, predecessor).
 * The templated rb_tree_node<Value> inherits from this to add the value.
 */
struct rb_tree_node_base {
    typedef rb_tree_node_base*       base_ptr;
    typedef const rb_tree_node_base* const_base_ptr;

    rb_tree_color color;   ///< Node color (red or black).
    base_ptr parent;       ///< Parent node (or &header_ for root).
    base_ptr left;         ///< Left child (or nullptr for nil leaf).
    base_ptr right;        ///< Right child (or nullptr for nil leaf).
    base_ptr sentinel_;    ///< Pointer to tree's header sentinel (for successor detection).

    // ---- Tree navigation (static, O(log n)) ----

    /** @brief  Returns the leftmost (smallest) node in the subtree rooted at x. */
    static base_ptr minimum(base_ptr x) {
        while (x->left != nullptr && x->left != x) x = x->left;  // guard against self-loop
        return x;
    }

    /** @brief  Returns the rightmost (largest) node in the subtree rooted at x. */
    static base_ptr maximum(base_ptr x) {
        while (x->right != nullptr && x->right != x) x = x->right;  // guard against self-loop
        return x;
    }

    static const_base_ptr minimum(const_base_ptr x) {
        while (x->left != nullptr && x->left != x) x = x->left;
        return x;
    }
    static const_base_ptr maximum(const_base_ptr x) {
        while (x->right != nullptr && x->right != x) x = x->right;
        return x;
    }

    /** @brief  Returns the in-order predecessor of x (the largest node < x). */
    static base_ptr predecessor(base_ptr x) {
        // Guard: sentinel node or empty tree
        if (x->sentinel_ == x) return x->right;  // rightmost for sentinel
        if (x->left != nullptr) return maximum(x->left);
        base_ptr y = x->parent;
        while (x == y->left) { x = y; y = y->parent; }
        return y;
    }

    /** @brief  Returns the in-order successor of x (the smallest node > x),
     *          or the sentinel if x is the rightmost. */
    static base_ptr successor(base_ptr x) {
        if (x->right != nullptr) return minimum(x->right);
        base_ptr y = x->parent;
        bool climbed = false;
        while (x == y->right) { x = y; y = y->parent; climbed = true; }
        if (!climbed) return y;  // x is a left child: return parent
        // We climbed the right-child chain. If x is the sentinel, return it.
        if (x->sentinel_ == x) return x;
        return y;
    }
};

// =========================================================================
// rb_tree_node — templated node holding a Value
// =========================================================================

/**
 * @brief  Templated RB-tree node storing a value of type Value.
 *
 * @tparam Value  The type stored in the node (pair<const Key, T> or Key).
 */
template <typename Value>
struct rb_tree_node : public rb_tree_node_base {
    Value value_field;  ///< The stored value.

    template <typename... Args>
    rb_tree_node(Args&&... args) : value_field(std::forward<Args>(args)...) {}

    Value* value_ptr() { return &value_field; }
    const Value* value_ptr() const { return &value_field; }
};

// =========================================================================
// rb_tree — main RB-tree implementation
// =========================================================================

/**
 * @brief  Red-black tree with unique-key and duplicate-key insertion modes.
 *
 * All operations are O(log n). The tree automatically rebalances
 * after insert and erase to maintain the RB invariants.
 *
 * @tparam Key         Key type.
 * @tparam Value       Stored value type.
 * @tparam KeyOfValue  Functor: Value -> Key.
 * @tparam Compare     Key comparison (strict weak ordering).
 * @tparam Alloc       Node allocator.
 */
template <typename Key, typename Value, typename KeyOfValue,
          typename Compare, typename Alloc = allocator<Value>>
class rb_tree {
public:
    typedef Key                  key_type;
    typedef Value                value_type;
    typedef Value*               pointer;
    typedef const Value*         const_pointer;
    typedef Value&               reference;
    typedef const Value&         const_reference;
    typedef size_t               size_type;
    typedef ptrdiff_t            difference_type;

    typedef rb_tree_node<Value>   node_type;
    typedef rb_tree_node_base    base_type;
    typedef rb_tree_node_base*   base_ptr;

    typedef typename Alloc::template rebind<node_type>::other node_allocator;

private:
    base_type  header_;          ///< Sentinel: root=header_.parent, leftmost=header_.left, rightmost=header_.right
    size_type  node_count_;      ///< Number of nodes in the tree.
    Compare    key_compare_;     ///< Key comparison functor.
    KeyOfValue key_of_value_;    ///< Key extraction functor.
    node_allocator node_alloc_;  ///< Node allocator.

    // ---- Header accessors ----
    base_ptr& root()                { return header_.parent; }
    base_ptr root() const           { return header_.parent; }
    base_ptr& leftmost()            { return header_.left; }
    base_ptr leftmost() const       { return header_.left; }
    base_ptr& rightmost()           { return header_.right; }
    base_ptr rightmost() const      { return header_.right; }

    // ---- Node management ----
    node_type* create_node(const value_type& v);
    void destroy_node(node_type* node);

    /** @brief  Initializes an empty tree. */
    void init() {
        header_.color = rb_red; // distinguish from root (always black)
        header_.sentinel_ = &header_;
        root() = nullptr;
        leftmost() = &header_;
        rightmost() = &header_;
        node_count_ = 0;
    }

    // ---- Rotations ----
    void rotate_left(base_ptr x);
    void rotate_right(base_ptr x);

    // ---- Rebalancing ----
    /** @brief  nullptr nodes are treated as black (nil leaves). */
    static bool is_red(base_ptr x)  { return x != nullptr && x->color == rb_red; }
    static bool is_black(base_ptr x) { return x == nullptr || x->color == rb_black; }

    void insert_fixup(base_ptr z);
    void transplant(base_ptr u, base_ptr v);
    void erase_fixup(base_ptr x, base_ptr x_parent);
    void erase_node(base_ptr z);

    /** @brief  Recursively destroys a subtree. */
    void destroy_subtree(base_ptr node) {
        if (node == nullptr) return;
        destroy_subtree(node->left);
        destroy_subtree(node->right);
        destroy_node(static_cast<node_type*>(node));
    }

public:
    // ---- Construction ----
    rb_tree() : node_count_(0), key_compare_(), key_of_value_() { init(); }
    explicit rb_tree(const Compare& comp) : node_count_(0), key_compare_(comp), key_of_value_() { init(); }
    rb_tree(const rb_tree& other);
    ~rb_tree() { clear(); }
    rb_tree& operator=(const rb_tree& other);

    // ---- Accessors ----
    Compare key_comp() const { return key_compare_; }
    size_type size() const   { return node_count_; }
    bool empty() const       { return node_count_ == 0; }

    // ---- Insertion ----
    /** @brief  Inserts with unique-key semantics. Returns pair<iterator, bool>. */
    pair<base_ptr, bool> insert_unique(const value_type& v);
    /** @brief  Inserts with hint (for position hint optimization). */
    base_ptr insert_unique(base_ptr hint, const value_type& v);
    /** @brief  Inserts allowing duplicates. Always succeeds. */
    base_ptr insert_equal(const value_type& v);

    // ---- Erasure ----
    void erase(base_ptr z);
    size_type erase_unique(const key_type& k);

    // ---- Lookup ----
    base_ptr find(const key_type& k) const;
    size_type count(const key_type& k) const;
    base_ptr lower_bound(const key_type& k) const;
    base_ptr upper_bound(const key_type& k) const;
    pair<base_ptr, base_ptr> equal_range(const key_type& k) const;

    // ---- Iteration support ----
    base_ptr begin_node() const { return leftmost(); }
    base_ptr end_node() const   { return const_cast<base_ptr>(&header_); }

    // begin()/end() are defined out-of-line after rb_tree_iterator is declared

    /** @brief  Swaps contents with another tree (O(1)). */
    void swap(rb_tree& other) noexcept {
        lstl::swap(header_, other.header_);
        lstl::swap(node_count_, other.node_count_);
        lstl::swap(key_compare_, other.key_compare_);
    }

    // ---- Bulk operations ----
    void clear();
    base_ptr insert_unique_at(base_ptr pos, const value_type& v, bool left);
};

// =========================================================================
// rb_tree_iterator — bidirectional iterator
// =========================================================================

/**
 * @brief  Bidirectional iterator for red-black tree traversal.
 *
 * Wraps an rb_tree_node_base* and traverses in in-order (sorted) sequence.
 *
 * @tparam Value  The value type exposed by the iterator.
 */
template <typename Value>
class rb_tree_iterator {
public:
    typedef Value                         value_type;
    typedef Value&                        reference;
    typedef Value*                        pointer;
    typedef ptrdiff_t                     difference_type;
    typedef std::bidirectional_iterator_tag iterator_category;

    typedef rb_tree_node_base*            base_ptr;

    rb_tree_iterator() : node_(nullptr) {}
    explicit rb_tree_iterator(base_ptr n) : node_(n) {}

    reference operator*() const { return *static_cast<rb_tree_node<Value>*>(node_)->value_ptr(); }
    pointer operator->() const  { return static_cast<rb_tree_node<Value>*>(node_)->value_ptr(); }

    /** @brief  Moves to the next in-order node via successor(). */
    rb_tree_iterator& operator++()    { node_ = rb_tree_node_base::successor(node_); return *this; }
    rb_tree_iterator operator++(int)  { rb_tree_iterator tmp = *this; node_ = rb_tree_node_base::successor(node_); return tmp; }

    /** @brief  Moves to the previous in-order node via predecessor(). */
    rb_tree_iterator& operator--()    { node_ = rb_tree_node_base::predecessor(node_); return *this; }
    rb_tree_iterator operator--(int)  { rb_tree_iterator tmp = *this; node_ = rb_tree_node_base::predecessor(node_); return tmp; }

    bool operator==(const rb_tree_iterator& o) const { return node_ == o.node_; }
    bool operator!=(const rb_tree_iterator& o) const { return node_ != o.node_; }

    base_ptr base() const { return node_; }

private:
    base_ptr node_;  ///< Pointer to the current tree node.
};

} // namespace detail
} // namespace lstl

// =========================================================================
// Template implementation (must be in header for implicit instantiation)
// =========================================================================

// Include the inline implementations from a separate file to keep the
// interface clean. In a production library, these would be in a -inl.h file.
// For simplicity, the implementations follow below.

// =========================================================================
// rb_tree — inline template implementations (must be in namespace)
// =========================================================================

namespace lstl {
namespace detail {

template <typename K, typename V, typename KOV, typename C, typename A>
typename rb_tree<K,V,KOV,C,A>::node_type*
rb_tree<K,V,KOV,C,A>::create_node(const value_type& v) {
    node_type* node = node_alloc_.allocate(1);
    try {
        lstl::construct(&node->value_field, v);
    } catch (...) { node_alloc_.deallocate(node, 1); throw; }
    node->color = rb_red;
    node->parent = nullptr;
    node->left = nullptr;
    node->right = nullptr;
    node->sentinel_ = &header_;
    return node;
}

template <typename K, typename V, typename KOV, typename C, typename A>
void rb_tree<K,V,KOV,C,A>::destroy_node(node_type* node) {
    lstl::destroy(&node->value_field);
    node_alloc_.deallocate(node, 1);
}

template <typename K, typename V, typename KOV, typename C, typename A>
void rb_tree<K,V,KOV,C,A>::rotate_left(base_ptr x) {
    base_ptr y = x->right;
    x->right = y->left;
    if (y->left != nullptr) y->left->parent = x;
    y->parent = x->parent;
    if (x == root()) root() = y;
    else if (x == x->parent->left) x->parent->left = y;
    else x->parent->right = y;
    y->left = x;
    x->parent = y;
}

template <typename K, typename V, typename KOV, typename C, typename A>
void rb_tree<K,V,KOV,C,A>::rotate_right(base_ptr x) {
    base_ptr y = x->left;
    x->left = y->right;
    if (y->right != nullptr) y->right->parent = x;
    y->parent = x->parent;
    if (x == root()) root() = y;
    else if (x == x->parent->right) x->parent->right = y;
    else x->parent->left = y;
    y->right = x;
    x->parent = y;
}

template <typename K, typename V, typename KOV, typename C, typename A>
void rb_tree<K,V,KOV,C,A>::insert_fixup(base_ptr z) {
    while (z != root() && is_red(z->parent)) {
        if (z->parent == z->parent->parent->left) {
            base_ptr y = z->parent->parent->right;
            if (is_red(y)) {
                z->parent->color = rb_black;
                y->color = rb_black;
                z->parent->parent->color = rb_red;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) { z = z->parent; rotate_left(z); }
                z->parent->color = rb_black;
                z->parent->parent->color = rb_red;
                rotate_right(z->parent->parent);
            }
        } else {
            base_ptr y = z->parent->parent->left;
            if (is_red(y)) {
                z->parent->color = rb_black;
                y->color = rb_black;
                z->parent->parent->color = rb_red;
                z = z->parent->parent;
            } else {
                if (z == z->parent->left) { z = z->parent; rotate_right(z); }
                z->parent->color = rb_black;
                z->parent->parent->color = rb_red;
                rotate_left(z->parent->parent);
            }
        }
    }
    root()->color = rb_black;
}

template <typename K, typename V, typename KOV, typename C, typename A>
void rb_tree<K,V,KOV,C,A>::transplant(base_ptr u, base_ptr v) {
    if (u == root()) root() = v;
    else if (u == u->parent->left) u->parent->left = v;
    else u->parent->right = v;
    if (v != nullptr) v->parent = u->parent;
}

template <typename K, typename V, typename KOV, typename C, typename A>
void rb_tree<K,V,KOV,C,A>::erase_fixup(base_ptr x, base_ptr x_parent) {
    while (x != root() && is_black(x)) {
        if (x == x_parent->left) {
            base_ptr w = x_parent->right;
            if (is_red(w)) { w->color = rb_black; x_parent->color = rb_red; rotate_left(x_parent); w = x_parent->right; }
            if (w && is_black(w->left) && is_black(w->right)) { w->color = rb_red; x = x_parent; x_parent = x_parent->parent; }
            else {
                if (is_black(w->right)) { if (w->left) w->left->color = rb_black; w->color = rb_red; rotate_right(w); w = x_parent->right; }
                w->color = x_parent->color; x_parent->color = rb_black;
                if (w->right) w->right->color = rb_black;
                rotate_left(x_parent); break;
            }
        } else {
            base_ptr w = x_parent->left;
            if (is_red(w)) { w->color = rb_black; x_parent->color = rb_red; rotate_right(x_parent); w = x_parent->left; }
            if (w && is_black(w->right) && is_black(w->left)) { w->color = rb_red; x = x_parent; x_parent = x_parent->parent; }
            else {
                if (is_black(w->left)) { if (w->right) w->right->color = rb_black; w->color = rb_red; rotate_left(w); w = x_parent->left; }
                w->color = x_parent->color; x_parent->color = rb_black;
                if (w->left) w->left->color = rb_black;
                rotate_right(x_parent); break;
            }
        }
    }
    if (x) x->color = rb_black;
}

template <typename K, typename V, typename KOV, typename C, typename A>
void rb_tree<K,V,KOV,C,A>::erase_node(base_ptr z) {
    base_ptr y = z;
    base_ptr x = nullptr;
    base_ptr x_parent = nullptr;
    rb_tree_color y_original_color = y->color;

    if (z->left == nullptr) {
        x = z->right; x_parent = z->parent; transplant(z, z->right);
    } else if (z->right == nullptr) {
        x = z->left; x_parent = z->parent; transplant(z, z->left);
    } else {
        y = base_type::minimum(z->right);
        y_original_color = y->color;
        x = y->right;
        if (y->parent == z) { x_parent = y; }
        else { x_parent = y->parent; transplant(y, y->right); y->right = z->right; y->right->parent = y; }
        transplant(z, y);
        y->left = z->left; y->left->parent = y; y->color = z->color;
    }
    if (y_original_color == rb_black) erase_fixup(x, x_parent);
}

template <typename K, typename V, typename KOV, typename C, typename A>
pair<typename rb_tree<K,V,KOV,C,A>::base_ptr, bool>
rb_tree<K,V,KOV,C,A>::insert_unique(const value_type& v) {
    const key_type& k = key_of_value_(v);
    if (root() == nullptr) {
        root() = create_node(v); root()->parent = &header_; root()->color = rb_black;
        leftmost() = root(); rightmost() = root(); ++node_count_;
        return lstl::make_pair(static_cast<base_ptr>(root()), true);
    }
    base_ptr y = &header_; base_ptr x = root(); bool go_left = false;
    while (x != nullptr) {
        y = x;
        go_left = key_compare_(k, key_of_value_(static_cast<node_type*>(x)->value_field));
        if (!go_left && !key_compare_(key_of_value_(static_cast<node_type*>(x)->value_field), k))
            return lstl::make_pair(x, false);
        x = go_left ? x->left : x->right;
    }
    base_ptr z = create_node(v); z->parent = y;
    if (go_left) { y->left = z; if (y == leftmost()) leftmost() = z; }
    else { y->right = z; if (y == rightmost()) rightmost() = z; }
    insert_fixup(z); ++node_count_;
    return lstl::make_pair(z, true);
}

template <typename K, typename V, typename KOV, typename C, typename A>
typename rb_tree<K,V,KOV,C,A>::base_ptr
rb_tree<K,V,KOV,C,A>::insert_unique(base_ptr hint, const value_type& v) {
    if (hint == end_node()) {
        if (!empty() && key_compare_(key_of_value_(rightmost()->value_ptr()), key_of_value_(v)))
            return insert_unique_at(rightmost(), v, false);
        return insert_unique(v).first;
    }
    return insert_unique(v).first;
}

template <typename K, typename V, typename KOV, typename C, typename A>
typename rb_tree<K,V,KOV,C,A>::base_ptr
rb_tree<K,V,KOV,C,A>::insert_equal(const value_type& v) {
    const key_type& k = key_of_value_(v);
    if (root() == nullptr) {
        root() = create_node(v); root()->parent = &header_; root()->color = rb_black;
        leftmost() = root(); rightmost() = root(); ++node_count_;
        return root();
    }
    base_ptr y = &header_; base_ptr x = root();
    while (x != nullptr) { y = x; x = key_compare_(k, key_of_value_(static_cast<node_type*>(x)->value_field)) ? x->left : x->right; }
    base_ptr z = create_node(v); z->parent = y;
    if (y == &header_ || key_compare_(k, key_of_value_(static_cast<node_type*>(y)->value_field))) {
        y->left = z; if (y == leftmost()) leftmost() = z;
    } else { y->right = z; if (y == rightmost()) rightmost() = z; }
    insert_fixup(z); ++node_count_;
    return z;
}

template <typename K, typename V, typename KOV, typename C, typename A>
void rb_tree<K,V,KOV,C,A>::erase(base_ptr z) {
    if (z == nullptr || z == &header_) return;
    if (z == leftmost()) leftmost() = base_type::successor(z);
    if (z == rightmost()) rightmost() = base_type::predecessor(z);
    erase_node(z); destroy_node(static_cast<node_type*>(z)); --node_count_;
    if (node_count_ == 0) init();
}

template <typename K, typename V, typename KOV, typename C, typename A>
typename rb_tree<K,V,KOV,C,A>::size_type
rb_tree<K,V,KOV,C,A>::erase_unique(const key_type& k) {
    base_ptr z = find(k);
    if (z == end_node()) return 0;
    erase(z); return 1;
}

template <typename K, typename V, typename KOV, typename C, typename A>
typename rb_tree<K,V,KOV,C,A>::base_ptr
rb_tree<K,V,KOV,C,A>::find(const key_type& k) const {
    base_ptr x = root();
    while (x != nullptr) {
        if (key_compare_(k, key_of_value_(static_cast<const node_type*>(x)->value_field))) x = x->left;
        else if (key_compare_(key_of_value_(static_cast<const node_type*>(x)->value_field), k)) x = x->right;
        else return x;
    }
    return const_cast<base_ptr>(&header_);
}

template <typename K, typename V, typename KOV, typename C, typename A>
typename rb_tree<K,V,KOV,C,A>::size_type
rb_tree<K,V,KOV,C,A>::count(const key_type& k) const {
    // For unique-key trees, count is always 0 or 1
    base_ptr lb = lower_bound(k);
    if (lb == end_node()) return 0;
    if (key_compare_(k, key_of_value_(static_cast<const node_type*>(lb)->value_field))) return 0;
    // For equal_range support (multi-key), iterate successors
    size_type n = 0;
    base_ptr ub = upper_bound(k);
    base_ptr it = lb;
    while (it != ub && it != end_node()) {
        ++n;
        it = base_type::successor(it);
        if (it == lb) break; // safety: detect cycle
    }
    return n;
}

template <typename K, typename V, typename KOV, typename C, typename A>
typename rb_tree<K,V,KOV,C,A>::base_ptr
rb_tree<K,V,KOV,C,A>::lower_bound(const key_type& k) const {
    base_ptr x = root(); base_ptr result = const_cast<base_ptr>(&header_);
    while (x != nullptr) {
        if (!key_compare_(key_of_value_(static_cast<const node_type*>(x)->value_field), k)) { result = x; x = x->left; }
        else x = x->right;
    }
    return result;
}

template <typename K, typename V, typename KOV, typename C, typename A>
typename rb_tree<K,V,KOV,C,A>::base_ptr
rb_tree<K,V,KOV,C,A>::upper_bound(const key_type& k) const {
    base_ptr x = root(); base_ptr result = const_cast<base_ptr>(&header_);
    while (x != nullptr) {
        if (key_compare_(k, key_of_value_(static_cast<const node_type*>(x)->value_field))) { result = x; x = x->left; }
        else x = x->right;
    }
    return result;
}

template <typename K, typename V, typename KOV, typename C, typename A>
pair<typename rb_tree<K,V,KOV,C,A>::base_ptr, typename rb_tree<K,V,KOV,C,A>::base_ptr>
rb_tree<K,V,KOV,C,A>::equal_range(const key_type& k) const {
    return lstl::make_pair(lower_bound(k), upper_bound(k));
}

template <typename K, typename V, typename KOV, typename C, typename A>
void rb_tree<K,V,KOV,C,A>::clear() {
    if (node_count_ == 0) return;
    destroy_subtree(root());
    init();
}

template <typename K, typename V, typename KOV, typename C, typename A>
rb_tree<K,V,KOV,C,A>::rb_tree(const rb_tree& other)
    : node_count_(0), key_compare_(other.key_compare_), key_of_value_(other.key_of_value_) {
    init();
    size_type n = other.size();
    base_ptr it = other.begin_node();
    for (size_type i = 0; i < n; ++i) {
        insert_unique(*static_cast<const node_type*>(it)->value_ptr());
        it = base_type::successor(it);
    }
}

template <typename K, typename V, typename KOV, typename C, typename A>
rb_tree<K,V,KOV,C,A>& rb_tree<K,V,KOV,C,A>::operator=(const rb_tree& other) {
    if (this != &other) { clear(); key_compare_ = other.key_compare_; key_of_value_ = other.key_of_value_;
        size_type n = other.size();
        base_ptr it = other.begin_node();
        for (size_type i = 0; i < n; ++i) {
            insert_unique(*static_cast<const node_type*>(it)->value_ptr());
            it = base_type::successor(it);
        }
    }
    return *this;
}

template <typename K, typename V, typename KOV, typename C, typename A>
typename rb_tree<K,V,KOV,C,A>::base_ptr
rb_tree<K,V,KOV,C,A>::insert_unique_at(base_ptr pos, const value_type& v, bool left) {
    base_ptr z = create_node(v); z->parent = pos;
    if (left) { pos->left = z; if (pos == leftmost()) leftmost() = z; }
    else { pos->right = z; if (pos == rightmost()) rightmost() = z; }
    insert_fixup(z); ++node_count_;
    return z;
}

} // namespace detail
} // namespace lstl
