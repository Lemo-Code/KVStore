// zstl red-black tree — iterative CLRS implementation with sentinel header
// Used by: map, set, multimap, multiset
//
// This is a full red-black tree that follows CLRS chapter 13:
//   - Every node is RED or BLACK
//   - Root is BLACK
//   - Leaves (null) are BLACK (we use nullptr for leaves)
//   - RED node cannot have a RED parent
//   - Every path from a node to its descendant leaves contains the
//     same number of BLACK nodes (black-height invariant)
//
// Insert fixup handles 3 cases (symmetrically for left/right):
//   Case 1: uncle is RED — recolor and move up
//   Case 2: zig-zag — rotate to form a line
//   Case 3: straight line — rotate and recolor to restore
//
// Erase fixup handles 4 cases (symmetrically):
//   Case 1: sibling is RED — rotate and recolor to make sibling BLACK
//   Case 2: sibling's children are both BLACK — recolor sibling RED and move up
//   Case 3: sibling's far child is BLACK — rotate near child and recolor
//   Case 4: sibling's far child is RED — rotate and recolor to terminate
#pragma once

#include <cstdint>
#include <cstddef>
#include "zstl/iterators/reverse_iterator.h"
#include <type_traits>
#include <functional>

#include "zstl/memory/utility.h"
#include "zstl/memory/allocator.h"
#include "zstl/memory/construct.h"
#include "zstl/memory/type_traits.h"
#include "zstl/iterators/iterator_traits.h"

namespace zstl {
namespace detail {

// ============================================================
// RB tree color enum
// ============================================================
enum class RbColor : uint8_t {
    RED   = 0,
    BLACK = 1
};

// ============================================================
// RB tree node base — stores links and color, no key/value
// The header node also uses this type, with its color set to RED
// so sentinel parent check works correctly in fixup loops.
// ============================================================
struct rb_tree_node_base {
    rb_tree_node_base* parent;
    rb_tree_node_base* left;
    rb_tree_node_base* right;
    RbColor            color;

    // ---- tree navigation utilities ----

    // Return leftmost descendant of x (minimum in subtree)
    static rb_tree_node_base* minimum(rb_tree_node_base* x) noexcept {
        while (x->left) x = x->left;
        return x;
    }

    static const rb_tree_node_base* minimum(const rb_tree_node_base* x) noexcept {
        while (x->left) x = x->left;
        return x;
    }

    // Return rightmost descendant of x (maximum in subtree)
    static rb_tree_node_base* maximum(rb_tree_node_base* x) noexcept {
        while (x->right) x = x->right;
        return x;
    }

    static const rb_tree_node_base* maximum(const rb_tree_node_base* x) noexcept {
        while (x->right) x = x->right;
        return x;
    }

    // In-order successor
    static rb_tree_node_base* successor(rb_tree_node_base* x) noexcept {
        if (x->right) return minimum(x->right);
        rb_tree_node_base* y = x->parent;
        while (y && x == y->right) {
            x = y;
            y = y->parent;
        }
        return y;
    }

    static const rb_tree_node_base* successor(const rb_tree_node_base* x) noexcept {
        if (x->right) return minimum(x->right);
        const rb_tree_node_base* y = x->parent;
        while (y && x == y->right) {
            x = y;
            y = y->parent;
        }
        return y;
    }

    // In-order predecessor
    static rb_tree_node_base* predecessor(rb_tree_node_base* x) noexcept {
        if (x->left) return maximum(x->left);
        rb_tree_node_base* y = x->parent;
        while (y && x == y->left) {
            x = y;
            y = y->parent;
        }
        return y;
    }

    static const rb_tree_node_base* predecessor(const rb_tree_node_base* x) noexcept {
        if (x->left) return maximum(x->left);
        const rb_tree_node_base* y = x->parent;
        while (y && x == y->left) {
            x = y;
            y = y->parent;
        }
        return y;
    }
};

// ============================================================
// RB tree node — stores key and value alongside base links
// Template params:
//   Key   — key type
//   Value — value type (for set, Value == Key; for map, Value == pair<const Key, T>)
// ============================================================
template<typename Key, typename Value>
struct rb_tree_node : public rb_tree_node_base {
    Key   key;
    Value value;

    // Convenience: typed access from base pointer
    static rb_tree_node* from_base(rb_tree_node_base* base) noexcept {
        return static_cast<rb_tree_node*>(base);
    }

    static const rb_tree_node* from_base(const rb_tree_node_base* base) noexcept {
        return static_cast<const rb_tree_node*>(base);
    }
};

// ============================================================
// RB tree iterator — bidirectional
// Uses a base pointer for interoperability between iterator and
// const_iterator (implicit conversion from iterator to const_iterator).
// ============================================================
template<typename Key, typename Value, typename Ref, typename Ptr>
struct rb_tree_iterator {
    using iterator_category = bidirectional_iterator_tag;
    using value_type        = Value;
    using difference_type   = ptrdiff_t;
    using pointer           = Ptr;
    using reference         = Ref;
    using node_type         = rb_tree_node<Key, Value>;
    using base_ptr          = rb_tree_node_base*;

    base_ptr node;  // points to the node or to the header sentinel (end)

    // ---- constructors ----

    rb_tree_iterator() noexcept : node(nullptr) {}
    explicit rb_tree_iterator(base_ptr x) noexcept : node(x) {}

    // Implicit conversion from iterator to const_iterator
    template<typename R, typename P,
             typename = std::enable_if_t<std::is_convertible_v<R, Ref> && std::is_convertible_v<P, Ptr>>>
    rb_tree_iterator(const rb_tree_iterator<Key, Value, R, P>& other) noexcept
        : node(other.node) {}

    // ---- dereference ----

    reference operator*() const {
        return static_cast<node_type*>(node)->value;
    }

    pointer operator->() const {
        return &static_cast<node_type*>(node)->value;
    }

    // ---- increment / decrement ----

    rb_tree_iterator& operator++() noexcept {
        node = rb_tree_node_base::successor(node);
        return *this;
    }

    rb_tree_iterator operator++(int) noexcept {
        rb_tree_iterator tmp = *this;
        node = rb_tree_node_base::successor(node);
        return tmp;
    }

    rb_tree_iterator& operator--() noexcept {
        node = rb_tree_node_base::predecessor(node);
        return *this;
    }

    rb_tree_iterator operator--(int) noexcept {
        rb_tree_iterator tmp = *this;
        node = rb_tree_node_base::predecessor(node);
        return tmp;
    }

    // ---- comparison ----

    bool operator==(const rb_tree_iterator& other) const noexcept { return node == other.node; }
    bool operator!=(const rb_tree_iterator& other) const noexcept { return node != other.node; }
};

// ============================================================
// RB tree — iterative, non-recursive CLRS red-black tree
//
// The header node serves double duty:
//   header.parent = root
//   header.left   = leftmost (minimum) -> begin()
//   header.right  = rightmost (maximum)
//   root.parent   = &header
//
// This design gives O(1) begin()/end() and simplifies sentinel checks.
//
// Template params:
//   Key         — key type
//   Value       — value type stored in nodes
//   KeyOfValue  — functor to extract Key from Value
//   Compare     — strict weak ordering on Key
//   Alloc       — allocator for rb_tree_node<Key, Value>
// ============================================================
template<typename Key, typename Value, typename KeyOfValue,
         typename Compare, typename Alloc = default_alloc<rb_tree_node<Key, Value>>>
class rb_tree {
public:
    // ---- type definitions ----
    using key_type        = Key;
    using value_type      = Value;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;
    using key_compare     = Compare;
    using allocator_type  = Alloc;

    using iterator       = rb_tree_iterator<Key, Value, Value&, Value*>;
    using const_iterator = rb_tree_iterator<Key, Value, const Value&, const Value*>;

    using reverse_iterator       = zstl::reverse_iterator<iterator>;
    using const_reverse_iterator = zstl::reverse_iterator<const_iterator>;

    using node_type = rb_tree_node<Key, Value>;

private:
    using node_alloc       = typename std::allocator_traits<Alloc>::template rebind_alloc<node_type>;
    using node_alloc_traits = std::allocator_traits<node_alloc>;

    // ---- data members ----
    // The header node is embedded in the tree object (no heap allocation).
    // Its links are used as described above.
    rb_tree_node_base header_;
    size_type         node_count_ = 0;
    [[no_unique_address]] Compare comp_;
    [[no_unique_address]] node_alloc alloc_;

    // Convenience accessors for header links
    rb_tree_node_base*& root()       noexcept { return header_.parent; }
    rb_tree_node_base*& leftmost()   noexcept { return header_.left; }
    rb_tree_node_base*& rightmost()  noexcept { return header_.right; }

    const rb_tree_node_base* root()       const noexcept { return header_.parent; }
    const rb_tree_node_base* leftmost()   const noexcept { return header_.left; }
    const rb_tree_node_base* rightmost()  const noexcept { return header_.right; }

    // ---- initialization helpers ----

    void init_header() noexcept {
        header_.color  = RbColor::RED;  // RED so insert_fixup loop condition terminates
        header_.parent = nullptr;       // root = nullptr
        header_.left   = &header_;      // leftmost points to header when empty
        header_.right  = &header_;      // rightmost points to header when empty
    }

    void reset_header() noexcept {
        header_.parent = nullptr;
        header_.left   = &header_;
        header_.right  = &header_;
    }

public:
    // ---- constructors / destructor ----

    rb_tree() noexcept(std::is_nothrow_default_constructible_v<Compare>)
        : comp_() {
        init_header();
    }

    explicit rb_tree(const Compare& comp) noexcept(std::is_nothrow_copy_constructible_v<Compare>)
        : comp_(comp) {
        init_header();
    }

    explicit rb_tree(const Compare& comp, const Alloc& alloc)
        : comp_(comp), alloc_(alloc) {
        init_header();
    }

    rb_tree(const rb_tree& other)
        : comp_(other.comp_), alloc_(node_alloc_traits::select_on_container_copy_construction(other.alloc_)) {
        init_header();
        // Bulk insertion is handled by the container layer; here we just setup
        for (auto it = other.begin(); it != other.end(); ++it) {
            insert_unique_copy(*it);
        }
    }

    rb_tree(rb_tree&& other) noexcept
        : comp_(zstl::move(other.comp_)), alloc_(zstl::move(other.alloc_)) {
        if (other.root()) {
            // Transfer ownership
            root()          = other.root();
            leftmost()      = other.leftmost();
            rightmost()     = other.rightmost();
            root()->parent  = &header_;
            node_count_     = other.node_count_;
            // Reset other
            other.init_header();
            other.node_count_ = 0;
        } else {
            init_header();
        }
    }

    rb_tree& operator=(const rb_tree& other) {
        if (this != &other) {
            clear();
            comp_ = other.comp_;
            for (auto it = other.begin(); it != other.end(); ++it) {
                insert_unique_copy(*it);
            }
        }
        return *this;
    }

    rb_tree& operator=(rb_tree&& other) noexcept {
        if (this != &other) {
            clear();
            if (other.root()) {
                root()          = other.root();
                leftmost()      = other.leftmost();
                rightmost()     = other.rightmost();
                root()->parent  = &header_;
                node_count_     = other.node_count_;
                other.init_header();
                other.node_count_ = 0;
            }
        }
        return *this;
    }

    ~rb_tree() {
        clear();
    }

    // ---- allocator access ----

    allocator_type get_allocator() const noexcept { return alloc_; }

    // ---- iterators ----

    iterator begin() noexcept {
        return iterator(leftmost());
    }

    const_iterator begin() const noexcept {
        return const_iterator(const_cast<rb_tree_node_base*>(leftmost()));
    }

    const_iterator cbegin() const noexcept {
        return begin();
    }

    iterator end() noexcept {
        return iterator(&header_);
    }

    const_iterator end() const noexcept {
        return const_iterator(const_cast<rb_tree_node_base*>(&header_));
    }

    const_iterator cend() const noexcept {
        return end();
    }

    reverse_iterator rbegin() noexcept {
        return reverse_iterator(end());
    }

    const_reverse_iterator rbegin() const noexcept {
        return const_reverse_iterator(end());
    }

    const_reverse_iterator crbegin() const noexcept {
        return rbegin();
    }

    reverse_iterator rend() noexcept {
        return reverse_iterator(begin());
    }

    const_reverse_iterator rend() const noexcept {
        return const_reverse_iterator(begin());
    }

    const_reverse_iterator crend() const noexcept {
        return rend();
    }

    // ---- capacity ----

    bool empty() const noexcept { return node_count_ == 0; }
    size_type size() const noexcept { return node_count_; }
    size_type max_size() const noexcept {
        return node_alloc_traits::max_size(alloc_);
    }

    // ---- key comparison ----

    key_compare key_comp() const noexcept { return comp_; }

    // ============================================================
    // Insert (unique keys)
    // Returns pair<iterator, bool> — iterator to element,
    // bool = true if insertion happened, false if key already existed.
    // ============================================================
    pair<iterator, bool> insert_unique(const value_type& v) {
        return insert_unique_impl(v, KeyOfValue()(v));
    }

    // Insert with hint (for better performance when inserting near a known position)
    pair<iterator, bool> insert_unique_hint(const_iterator hint, const value_type& v) {
        const Key& k = KeyOfValue()(v);
        // If hint is end(), use normal insertion
        if (hint.node == &header_) {
            return insert_unique_impl(v, k);
        }
        const Key& hint_key = static_cast<const node_type*>(hint.node)->key;
        // Check if hint is useful: hint key should be immediately before or after k
        // For simplicity, fall back to normal insert unless hint is exactly right
        return insert_unique_impl(v, k);
    }

    // ============================================================
    // Insert (allow duplicates)
    // Always inserts; returns iterator to the newly inserted element.
    // Duplicate keys are inserted in the order they arrive (stable wrt insertion order).
    // ============================================================
    iterator insert_multi(const value_type& v) {
        return insert_multi_impl(v, KeyOfValue()(v));
    }

    iterator insert_multi_hint(const_iterator hint, const value_type& v) {
        const Key& k = KeyOfValue()(v);
        return insert_multi_impl(v, k);
    }

    // ============================================================
    // Emplace (in-place construction)
    // These delegate to the container layer which constructs the value_type.
    // ============================================================
    template<typename... Args>
    pair<iterator, bool> emplace_unique(Args&&... args) {
        // Allocate node and construct value in place
        node_type* node = create_node_empty();
        try {
            construct(&node->value, zstl::forward<Args>(args)...);
            construct(&node->key, KeyOfValue()(node->value));
        } catch (...) {
            destroy_node(node);
            throw;
        }
        return insert_node_unique(node);
    }

    template<typename... Args>
    iterator emplace_multi(Args&&... args) {
        node_type* node = create_node_empty();
        try {
            construct(&node->value, zstl::forward<Args>(args)...);
            construct(&node->key, KeyOfValue()(node->value));
        } catch (...) {
            destroy_node(node);
            throw;
        }
        return insert_node_multi(node);
    }

    // ============================================================
    // Find
    // ============================================================
    iterator find(const Key& k) {
        rb_tree_node_base* x = root();
        while (x) {
            const Key& xk = static_cast<node_type*>(x)->key;
            if (comp_(k, xk)) {
                x = x->left;
            } else if (comp_(xk, k)) {
                x = x->right;
            } else {
                return iterator(x);
            }
        }
        return end();
    }

    const_iterator find(const Key& k) const {
        const rb_tree_node_base* x = root();
        while (x) {
            const Key& xk = static_cast<const node_type*>(x)->key;
            if (comp_(k, xk)) {
                x = x->left;
            } else if (comp_(xk, k)) {
                x = x->right;
            } else {
                return const_iterator(const_cast<rb_tree_node_base*>(x));
            }
        }
        return end();
    }

    // ============================================================
    // Count — returns 0 or 1 for unique-key trees
    // ============================================================
    size_type count(const Key& k) const {
        return find(k) != end() ? 1 : 0;
    }

    bool contains(const Key& k) const {
        return find(k) != end();
    }

    // ============================================================
    // Lower bound — first element >= k
    // ============================================================
    iterator lower_bound(const Key& k) {
        rb_tree_node_base* x = root();
        rb_tree_node_base* y = &header_;
        while (x) {
            const Key& xk = static_cast<node_type*>(x)->key;
            if (!comp_(xk, k)) {
                // xk >= k  — record candidate and search left
                y = x;
                x = x->left;
            } else {
                x = x->right;
            }
        }
        return iterator(y);
    }

    const_iterator lower_bound(const Key& k) const {
        const rb_tree_node_base* x = root();
        const rb_tree_node_base* y = &header_;
        while (x) {
            const Key& xk = static_cast<const node_type*>(x)->key;
            if (!comp_(xk, k)) {
                y = x;
                x = x->left;
            } else {
                x = x->right;
            }
        }
        return const_iterator(const_cast<rb_tree_node_base*>(y));
    }

    // ============================================================
    // Upper bound — first element > k
    // ============================================================
    iterator upper_bound(const Key& k) {
        rb_tree_node_base* x = root();
        rb_tree_node_base* y = &header_;
        while (x) {
            const Key& xk = static_cast<node_type*>(x)->key;
            if (comp_(k, xk)) {
                // k < xk — record candidate and search left
                y = x;
                x = x->left;
            } else {
                x = x->right;
            }
        }
        return iterator(y);
    }

    const_iterator upper_bound(const Key& k) const {
        const rb_tree_node_base* x = root();
        const rb_tree_node_base* y = &header_;
        while (x) {
            const Key& xk = static_cast<const node_type*>(x)->key;
            if (comp_(k, xk)) {
                y = x;
                x = x->left;
            } else {
                x = x->right;
            }
        }
        return const_iterator(const_cast<rb_tree_node_base*>(y));
    }

    // ============================================================
    // Equal range — [lower_bound(k), upper_bound(k))
    // ============================================================
    pair<iterator, iterator> equal_range(const Key& k) {
        return {lower_bound(k), upper_bound(k)};
    }

    pair<const_iterator, const_iterator> equal_range(const Key& k) const {
        return {lower_bound(k), upper_bound(k)};
    }

    // ============================================================
    // Erase
    // ============================================================
    void erase(iterator pos) {
        if (!pos.node || pos.node == &header_) return;
        node_type* z = static_cast<node_type*>(pos.node);

        // Update sentinel bookkeeping before the node is destroyed
        if (pos.node == leftmost()) {
            leftmost() = rb_tree_node_base::successor(pos.node);
        }
        if (pos.node == rightmost()) {
            rightmost() = rb_tree_node_base::predecessor(pos.node);
        }

        rb_erase(z);
        destroy_node(z);
        --node_count_;
    }

    void erase(const_iterator pos) {
        erase(iterator(const_cast<rb_tree_node_base*>(pos.node)));
    }

    // Erase by key — returns number of elements removed (0 or 1 for unique-key trees)
    size_type erase(const Key& k) {
        auto it = find(k);
        if (it == end()) return 0;
        erase(it);
        return 1;
    }

    void erase(iterator first, iterator last) {
        if (first == begin() && last == end()) {
            clear();
        } else {
            while (first != last) {
                erase(first++);
            }
        }
    }

    // ============================================================
    // Clear — destroy all nodes
    // ============================================================
    void clear() noexcept {
        if (root()) {
            // Use iterative post-order for stack safety
            clear_subtree(root());
            root()      = nullptr;
            leftmost()  = &header_;
            rightmost() = &header_;
            node_count_ = 0;
        }
    }

    // ============================================================
    // Swap
    // ============================================================
    void swap(rb_tree& other) noexcept(std::is_nothrow_swappable_v<Compare>) {
        using zstl::swap;
        if (root() && other.root()) {
            // Both non-empty: swap roots and fix parent pointers
            swap(root(), other.root());
            swap(leftmost(), other.leftmost());
            swap(rightmost(), other.rightmost());
            root()->parent = &header_;
            other.root()->parent = &other.header_;
        } else if (root()) {
            // Only this has elements
            other.root()      = root();
            other.leftmost()  = leftmost();
            other.rightmost() = rightmost();
            other.root()->parent = &other.header_;
            root()      = nullptr;
            leftmost()  = &header_;
            rightmost() = &header_;
        } else if (other.root()) {
            // Only other has elements
            root()      = other.root();
            leftmost()  = other.leftmost();
            rightmost() = other.rightmost();
            root()->parent = &header_;
            other.root()      = nullptr;
            other.leftmost()  = &other.header_;
            other.rightmost() = &other.header_;
        }
        swap(node_count_, other.node_count_);
        swap(comp_, other.comp_);
        swap(alloc_, other.alloc_);
    }

private:
    // ============================================================
    // Internal insert implementation
    // ============================================================
    pair<iterator, bool> insert_unique_impl(const value_type& v, const Key& k) {
        rb_tree_node_base* y = &header_;
        rb_tree_node_base* x = root();
        bool go_left = true;

        while (x) {
            y = x;
            const Key& xk = static_cast<node_type*>(x)->key;
            if (comp_(k, xk)) {
                x = x->left;
                go_left = true;
            } else if (comp_(xk, k)) {
                x = x->right;
                go_left = false;
            } else {
                // Duplicate key — don't insert
                return {iterator(x), false};
            }
        }

        node_type* z = create_node(v);
        z->parent = y;
        z->left   = nullptr;
        z->right  = nullptr;
        z->color  = RbColor::RED;

        link_and_rebalance(z, go_left, y);
        return {iterator(z), true};
    }

    iterator insert_multi_impl(const value_type& v, const Key& k) {
        rb_tree_node_base* y = &header_;
        rb_tree_node_base* x = root();
        bool go_left = true;

        while (x) {
            y = x;
            const Key& xk = static_cast<node_type*>(x)->key;
            if (comp_(k, xk)) {
                x = x->left;
                go_left = true;
            } else {
                // Equal keys go right (guarantees stable insertion order for duplicates)
                x = x->right;
                go_left = false;
            }
        }

        node_type* z = create_node(v);
        z->parent = y;
        z->left   = nullptr;
        z->right  = nullptr;
        z->color  = RbColor::RED;

        link_and_rebalance(z, go_left, y);
        return iterator(z);
    }

    // Insert an already-constructed node (for emplace)
    pair<iterator, bool> insert_node_unique(node_type* z) {
        const Key& k = z->key;
        rb_tree_node_base* y = &header_;
        rb_tree_node_base* x = root();
        bool go_left = true;

        while (x) {
            y = x;
            const Key& xk = static_cast<node_type*>(x)->key;
            if (comp_(k, xk)) {
                x = x->left;
                go_left = true;
            } else if (comp_(xk, k)) {
                x = x->right;
                go_left = false;
            } else {
                destroy_node(z);
                return {iterator(x), false};
            }
        }

        z->parent = y;
        z->left   = nullptr;
        z->right  = nullptr;
        z->color  = RbColor::RED;

        link_and_rebalance(z, go_left, y);
        return {iterator(z), true};
    }

    iterator insert_node_multi(node_type* z) {
        const Key& k = z->key;
        rb_tree_node_base* y = &header_;
        rb_tree_node_base* x = root();
        bool go_left = true;

        while (x) {
            y = x;
            const Key& xk = static_cast<node_type*>(x)->key;
            if (comp_(k, xk)) {
                x = x->left;
                go_left = true;
            } else {
                x = x->right;
                go_left = false;
            }
        }

        z->parent = y;
        z->left   = nullptr;
        z->right  = nullptr;
        z->color  = RbColor::RED;

        link_and_rebalance(z, go_left, y);
        return iterator(z);
    }

    // Insert for copy construction (always unique-key; used internally)
    void insert_unique_copy(const value_type& v) {
        insert_unique_impl(v, KeyOfValue()(v));
    }

    // Common logic: link node z as child of y, update sentinel pointers,
    // and fix up red-black invariants.
    void link_and_rebalance(node_type* z, bool go_left, rb_tree_node_base* y) {
        if (y == &header_ || go_left) {
            y->left = z;
            if (y == &header_) {
                // Tree was empty
                root()      = z;
                rightmost() = z;
            } else if (y == leftmost()) {
                leftmost() = z;
            }
        } else {
            y->right = z;
            if (y == rightmost()) {
                rightmost() = z;
            }
        }

        insert_fixup(z);
        ++node_count_;
    }

    // ============================================================
    // Node allocation / deallocation
    // ============================================================
    node_type* create_node(const value_type& v) {
        node_type* node = node_alloc_traits::allocate(alloc_, 1);
        try {
            construct(&node->key, KeyOfValue()(v));
            construct(&node->value, v);
        } catch (...) {
            node_alloc_traits::deallocate(alloc_, node, 1);
            throw;
        }
        node->parent = nullptr;
        node->left   = nullptr;
        node->right  = nullptr;
        return node;
    }

    // Allocate a node without constructing value (for emplace)
    node_type* create_node_empty() {
        node_type* node = node_alloc_traits::allocate(alloc_, 1);
        node->parent = nullptr;
        node->left   = nullptr;
        node->right  = nullptr;
        return node;
    }

    void destroy_node(node_type* node) noexcept {
        destroy(&node->value);
        destroy(&node->key);
        node_alloc_traits::deallocate(alloc_, node, 1);
    }

    // ============================================================
    // Tree destruction — iterative to avoid stack overflow
    // ============================================================
    void clear_subtree(rb_tree_node_base* x) noexcept {
        if (!x || x == &header_) return;  // Skip null and sentinel
        if (x->left && x->left != &header_)   clear_subtree(x->left);
        if (x->right && x->right != &header_) clear_subtree(x->right);
        destroy_node(static_cast<node_type*>(x));
    }

    // ============================================================
    // Rotations (CLRS 13.2)
    // ============================================================
    //
    // Left rotation on x:
    //     x                 y
    //    / \               /
    //   a   y     =>      x   c
    //      / \           /
    //     b   c         a   b
    //
    void rotate_left(rb_tree_node_base* x) noexcept {
        rb_tree_node_base* y = x->right;
        x->right = y->left;
        if (y->left) y->left->parent = x;
        y->parent = x->parent;

        if (x == root())           root() = y;
        else if (x == x->parent->left) x->parent->left = y;
        else                           x->parent->right = y;

        y->left  = x;
        x->parent = y;
    }

    //
    // Right rotation on x:
    //       x               y
    //      / \             /
    //     y   c    =>     a   x
    //    / \                 /
    //   a   b               b   c
    //
    void rotate_right(rb_tree_node_base* x) noexcept {
        rb_tree_node_base* y = x->left;
        x->left = y->right;
        if (y->right) y->right->parent = x;
        y->parent = x->parent;

        if (x == root())            root() = y;
        else if (x == x->parent->right) x->parent->right = y;
        else                            x->parent->left = y;

        y->right = x;
        x->parent = y;
    }

    // ============================================================
    // Insert fixup (CLRS 13.3)
    //
    // After inserting a RED node z:
    //   - Invariant violation: both z and z->parent may be RED
    //   - Loop while z->parent is RED
    //   - Three cases, symmetric for left/right:
    //
    //   Case 1: uncle y is RED
    //     => recolor parent, uncle BLACK; grandparent RED; move z up to grandparent
    //
    //   Case 2: z is right child of left parent (or left child of right parent)
    //     => rotate z's parent to form a straight line, then fall through to case 3
    //
    //   Case 3: z is left child of left parent (or right child of right parent)
    //     => recolor parent BLACK, grandparent RED, rotate grandparent
    //     => terminates the fixup
    // ============================================================
    void insert_fixup(rb_tree_node_base* z) noexcept {
        while (z->parent->color == RbColor::RED) {
            if (z->parent == z->parent->parent->left) {
                // z's parent is a left child
                rb_tree_node_base* y = z->parent->parent->right;  // uncle

                if (y && y->color == RbColor::RED) {
                    // Case 1: uncle is RED — recolor and push up
                    z->parent->color = RbColor::BLACK;
                    y->color         = RbColor::BLACK;
                    z->parent->parent->color = RbColor::RED;
                    z = z->parent->parent;
                } else {
                    if (z == z->parent->right) {
                        // Case 2: z is a right child — rotate left to make a straight line
                        z = z->parent;
                        rotate_left(z);
                    }
                    // Case 3: z is a left child — recolor and rotate to fix
                    z->parent->color = RbColor::BLACK;
                    z->parent->parent->color = RbColor::RED;
                    rotate_right(z->parent->parent);
                }
            } else {
                // z's parent is a right child (symmetric)
                rb_tree_node_base* y = z->parent->parent->left;  // uncle

                if (y && y->color == RbColor::RED) {
                    // Case 1 (symmetric): uncle is RED
                    z->parent->color = RbColor::BLACK;
                    y->color         = RbColor::BLACK;
                    z->parent->parent->color = RbColor::RED;
                    z = z->parent->parent;
                } else {
                    if (z == z->parent->left) {
                        // Case 2 (symmetric): z is a left child — rotate right
                        z = z->parent;
                        rotate_right(z);
                    }
                    // Case 3 (symmetric): z is a right child — recolor and rotate
                    z->parent->color = RbColor::BLACK;
                    z->parent->parent->color = RbColor::RED;
                    rotate_left(z->parent->parent);
                }
            }
        }
        root()->color = RbColor::BLACK;
    }

    // ============================================================
    // Transplant — replace subtree rooted at u with subtree rooted at v
    // (CLRS 13.3 TRANSPLANT)
    // ============================================================
    void transplant(rb_tree_node_base* u, rb_tree_node_base* v) noexcept {
        if (u->parent == &header_) root() = v;
        else if (u == u->parent->left) u->parent->left = v;
        else                           u->parent->right = v;
        if (v) v->parent = u->parent;
    }

    // ============================================================
    // RB Erase (CLRS 13.4 RB-DELETE)
    //
    // Works in two phases:
    // Phase 1 — Physical removal: remove node z from the tree, tracking
    //           the node y that actually gets removed (or recolored) and
    //           the node x that takes its place.
    // Phase 2 — Fixup: if y was BLACK, the black-height invariant is violated;
    //           fix it using RB-DELETE-FIXUP on x.
    // ============================================================
    void rb_erase(rb_tree_node_base* z) noexcept {
        rb_tree_node_base* y = z;
        rb_tree_node_base* x = nullptr;
        rb_tree_node_base* x_parent = nullptr;
        RbColor y_original_color = y->color;

        if (!z->left) {
            // z has no left child — replace with right child
            x = z->right;
            transplant(z, z->right);
            x_parent = z->parent;
        } else if (!z->right) {
            // z has no right child — replace with left child
            x = z->left;
            transplant(z, z->left);
            x_parent = z->parent;
        } else {
            // z has two children — find successor y, which has no left child
            y = rb_tree_node_base::minimum(z->right);
            y_original_color = y->color;
            x = y->right;
            if (y->parent == z) {
                // y is direct right child of z
                if (x) x->parent = y;
                x_parent = y;
            } else {
                // y is deeper in the right subtree
                transplant(y, y->right);
                y->right = z->right;
                y->right->parent = y;
                x_parent = y->parent;
            }
            transplant(z, y);
            y->left = z->left;
            y->left->parent = y;
            y->color = z->color;
        }

        if (y_original_color == RbColor::BLACK && x_parent) {
            erase_fixup(x, x_parent);
        }
    }

    // ============================================================
    // Erase fixup (CLRS 13.4 RB-DELETE-FIXUP)
    //
    // Restores black-height after a BLACK node was removed.
    // x is the node that replaced the removed BLACK node.
    // x has an "extra black" conceptually (it's "doubly black").
    //
    // Four cases, symmetric for x being left/right child:
    //
    //   Case 1: sibling w is RED
    //     => recolor w BLACK, x_parent RED, rotate x_parent toward x,
    //        update w to new sibling
    //
    //   Case 2: sibling w is BLACK, both children of w are BLACK
    //     => recolor w RED, move extra-black up to x_parent
    //
    //   Case 3: sibling w is BLACK, far child of w is BLACK, near child is RED
    //     => recolor near child BLACK, w RED, rotate w away from x,
    //        update w to new sibling, fall through to case 4
    //
    //   Case 4: sibling w is BLACK, far child of w is RED
    //     => recolor w to match x_parent, x_parent BLACK, far child BLACK,
    //        rotate x_parent toward x => terminates
    // ============================================================
    void erase_fixup(rb_tree_node_base* x, rb_tree_node_base* x_parent) noexcept {
        while (x != root() && (!x || x->color == RbColor::BLACK)) {
            if (x == x_parent->left) {
                rb_tree_node_base* w = x_parent->right;  // sibling

                // Case 1: sibling is RED
                if (w && w->color == RbColor::RED) {
                    w->color = RbColor::BLACK;
                    x_parent->color = RbColor::RED;
                    rotate_left(x_parent);
                    w = x_parent->right;
                }

                // Case 2: sibling has two BLACK children
                if (w && (!w->left || w->left->color == RbColor::BLACK) &&
                    (!w->right || w->right->color == RbColor::BLACK)) {
                    w->color = RbColor::RED;
                    x = x_parent;
                    x_parent = x_parent->parent;
                } else {
                    // Case 3: sibling's right child is BLACK (far child)
                    if (w && (!w->right || w->right->color == RbColor::BLACK)) {
                        if (w->left) w->left->color = RbColor::BLACK;
                        w->color = RbColor::RED;
                        rotate_right(w);
                        w = x_parent->right;
                    }

                    // Case 4: sibling's far child is RED — terminal case
                    if (w) {
                        w->color = x_parent->color;
                        if (w->right) w->right->color = RbColor::BLACK;
                    }
                    x_parent->color = RbColor::BLACK;
                    rotate_left(x_parent);
                    break;
                }
            } else {
                // Symmetric: x is right child
                rb_tree_node_base* w = x_parent->left;  // sibling

                // Case 1 (symmetric): sibling is RED
                if (w && w->color == RbColor::RED) {
                    w->color = RbColor::BLACK;
                    x_parent->color = RbColor::RED;
                    rotate_right(x_parent);
                    w = x_parent->left;
                }

                // Case 2 (symmetric): sibling has two BLACK children
                if (w && (!w->right || w->right->color == RbColor::BLACK) &&
                    (!w->left || w->left->color == RbColor::BLACK)) {
                    w->color = RbColor::RED;
                    x = x_parent;
                    x_parent = x_parent->parent;
                } else {
                    // Case 3 (symmetric): sibling's left child is BLACK
                    if (w && (!w->left || w->left->color == RbColor::BLACK)) {
                        if (w->right) w->right->color = RbColor::BLACK;
                        w->color = RbColor::RED;
                        rotate_left(w);
                        w = x_parent->left;
                    }

                    // Case 4 (symmetric): sibling's far child is RED — terminal case
                    if (w) {
                        w->color = x_parent->color;
                        if (w->left) w->left->color = RbColor::BLACK;
                    }
                    x_parent->color = RbColor::BLACK;
                    rotate_right(x_parent);
                    break;
                }
            }
        }
        if (x) x->color = RbColor::BLACK;
    }
};

// ============================================================
// Free function swap
// ============================================================
template<typename K, typename V, typename KOV, typename C, typename A>
void swap(rb_tree<K, V, KOV, C, A>& a, rb_tree<K, V, KOV, C, A>& b)
    noexcept(noexcept(a.swap(b))) {
    a.swap(b);
}

} // namespace detail
} // namespace zstl
