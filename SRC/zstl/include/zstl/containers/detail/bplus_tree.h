// zstl B+ tree — ordered associative container with range-scan optimization
// Used by: bmap, bset
//
// Design:
//   - Configurable branching factor BFactor (default 64)
//   - Internal nodes store keys + child pointers; keys[i] is the maximum
//     key in the subtree rooted at children[i]
//   - Leaf nodes store key-value pairs in sorted order
//   - Leaves form a singly-linked list for O(1) iteration between leaves
//   - Insert: split full nodes on the way down, then insert into leaf;
//     if leaf overflows, split and propagate key to parent
//   - Erase: remove from leaf; if underflow, borrow from sibling or merge
//   - All nodes are dynamically allocated; the tree owns all memory
//   - Iterators traverse the leaf linked list for O(1) next-leaf access
//
// Template params:
//   Key        — key type
//   Value      — value type
//   KeyOfValue — functor to extract Key from Value
//   Compare    — strict weak ordering on Key
//   BFactor    — maximum number of keys per node (must be >= 4)
//   Alloc      — allocator type
#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "zstl/memory/utility.h"
#include "zstl/memory/allocator.h"
#include "zstl/memory/construct.h"
#include "zstl/memory/type_traits.h"
#include "zstl/iterators/iterator_traits.h"

namespace zstl {
namespace detail {

// ============================================================
// B+ tree
// ============================================================
template<typename Key, typename Value, typename KeyOfValue,
         typename Compare = less<Key>,
         size_t BFactor = 64,
         typename Alloc = default_alloc<char>>
class bplus_tree {
public:
    // ---- type definitions ----
    using key_type        = Key;
    using value_type      = Value;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;
    using key_compare     = Compare;
    using allocator_type  = Alloc;

    static constexpr size_t kMaxKeys = BFactor >= 4 ? BFactor : 4;
    static constexpr size_t kMinKeysInternal = kMaxKeys / 2;  // Min keys for internal nodes (except root)
    static constexpr size_t kMinKeysLeaf     = (kMaxKeys + 1) / 2; // Slightly more for leaves

private:
    // ---- node types ----
    enum class NodeType : uint8_t { INTERNAL = 0, LEAF = 1 };

    struct Node {
        NodeType type;
        size_t   num_keys;    // Number of keys currently stored
        Key      keys[kMaxKeys];
        Node*    parent;

        // Leaf-specific
        Node*    next_leaf;   // Next leaf in the linked list

        // Data storage — union of child pointers (internal) or values (leaf)
        union {
            Node*  children[kMaxKeys + 1];  // Internal: children[0..num_keys]
            Value  values[kMaxKeys];         // Leaf: values[0..num_keys-1]
        };

        Node(NodeType t) noexcept
            : type(t), num_keys(0), parent(nullptr), next_leaf(nullptr) {
            // Zero-initialize all child pointers (safe even for leaf — same starting bytes)
            for (size_t i = 0; i <= kMaxKeys; ++i) {
                children[i] = nullptr;
            }
        }

        bool is_leaf()     const noexcept { return type == NodeType::LEAF; }
        bool is_internal() const noexcept { return type == NodeType::INTERNAL; }
        bool is_full()     const noexcept { return num_keys >= kMaxKeys; }
        bool is_underflow() const noexcept {
            if (is_leaf()) return num_keys < kMinKeysLeaf && parent != nullptr;
            return num_keys < kMinKeysInternal && parent != nullptr;
        }

        // Find the child index to descend for a given key
        // Returns the smallest i such that key <= keys[i], or num_keys if key > all keys
        size_t child_index_for(const Key& k, const Compare& comp) const noexcept {
            size_t i = 0;
            while (i < num_keys && !comp(k, keys[i])) {
                ++i;
            }
            return i;
        }
    };

    // ---- allocator for nodes ----
    using node_alloc = typename std::allocator_traits<Alloc>::template rebind_alloc<Node>;
    using node_alloc_traits = std::allocator_traits<node_alloc>;

    // ---- data members ----
    Node*          root_       = nullptr;
    Node*          first_leaf_ = nullptr;
    size_type      size_       = 0;
    [[no_unique_address]] Compare     comp_;
    [[no_unique_address]] node_alloc  alloc_;

public:
    // ---- forward declarations ----
    template<typename Ref, typename Ptr>
    struct bplus_iterator;

    using iterator       = bplus_iterator<Value&, Value*>;
    using const_iterator = bplus_iterator<const Value&, const Value*>;
    // Reverse iterators: B+ tree is forward-only; reverse_iterator delegates to
    // bidirectional adaptor. For now, aliased to the forward iterator type for
    // API compatibility with bmap/bset. Full reverse iteration requires a
    // doubly-linked leaf list or parent pointers.
    using reverse_iterator       = iterator;
    using const_reverse_iterator = const_iterator;
    // ---- iterator ----
    template<typename Ref, typename Ptr>
    struct bplus_iterator {
        using iterator_category = forward_iterator_tag;
        using value_type        = Value;
        using difference_type   = ptrdiff_t;
        using pointer           = Ptr;
        using reference         = Ref;

        Node*   leaf;
        size_t  pos;  // Position within the leaf

        bplus_iterator() noexcept : leaf(nullptr), pos(0) {}
        bplus_iterator(Node* l, size_t p) noexcept : leaf(l), pos(p) {}

        reference operator*() const {
            return leaf->values[pos];
        }

        pointer operator->() const {
            return &leaf->values[pos];
        }

        bplus_iterator& operator++() noexcept {
            ++pos;
            if (pos >= leaf->num_keys) {
                leaf = leaf->next_leaf;
                pos  = 0;
            }
            return *this;
        }

        bplus_iterator operator++(int) noexcept {
            bplus_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const bplus_iterator& other) const noexcept {
            return leaf == other.leaf && pos == other.pos;
        }

        bool operator!=(const bplus_iterator& other) const noexcept {
            return !(*this == other);
        }

        // Allow conversion from iterator to const_iterator
        template<typename R, typename P,
                 typename = std::enable_if_t<std::is_convertible_v<R, Ref> && std::is_convertible_v<P, Ptr>>>
        bplus_iterator(const bplus_iterator<R, P>& other) noexcept
            : leaf(other.leaf), pos(other.pos) {}
    };

    // ---- constructors / destructor ----

    bplus_tree() noexcept(std::is_nothrow_default_constructible_v<Compare>)
        : comp_() {
        init_empty();
    }

    explicit bplus_tree(const Compare& comp) noexcept(std::is_nothrow_copy_constructible_v<Compare>)
        : comp_(comp) {
        init_empty();
    }

    bplus_tree(const Compare& comp, const Alloc& alloc)
        : comp_(comp), alloc_(alloc) {
        init_empty();
    }

    bplus_tree(const bplus_tree& other)
        : comp_(other.comp_) {
        init_empty();
        for (auto it = other.begin(); it != other.end(); ++it) {
            insert_unique(*it);
        }
    }

    bplus_tree(bplus_tree&& other) noexcept
        : root_(other.root_), first_leaf_(other.first_leaf_),
          size_(other.size_), comp_(zstl::move(other.comp_)) {
        other.root_       = nullptr;
        other.first_leaf_ = nullptr;
        other.size_       = 0;
    }

    bplus_tree& operator=(const bplus_tree& other) {
        if (this != &other) {
            clear();
            comp_ = other.comp_;
            for (auto it = other.begin(); it != other.end(); ++it) {
                insert_unique(*it);
            }
        }
        return *this;
    }

    bplus_tree& operator=(bplus_tree&& other) noexcept {
        if (this != &other) {
            clear();
            root_       = other.root_;
            first_leaf_ = other.first_leaf_;
            size_       = other.size_;
            comp_       = zstl::move(other.comp_);
            other.root_       = nullptr;
            other.first_leaf_ = nullptr;
            other.size_       = 0;
        }
        return *this;
    }

    ~bplus_tree() {
        clear();
    }

    // ---- capacity ----

    size_type size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    size_type max_size() const noexcept { return node_alloc_traits::max_size(alloc_); }

    allocator_type get_allocator() const noexcept { return alloc_; }

    // ---- iterators ----

    iterator begin() noexcept {
        if (!first_leaf_ || first_leaf_->num_keys == 0) return end();
        return {first_leaf_, 0};
    }

    const_iterator begin() const noexcept {
        if (!first_leaf_ || first_leaf_->num_keys == 0) return end();
        return {first_leaf_, 0};
    }

    const_iterator cbegin() const noexcept { return begin(); }

    iterator end() noexcept {
        return {nullptr, 0};
    }

    const_iterator end() const noexcept {
        return {nullptr, 0};
    }

    const_iterator cend() const noexcept { return end(); }

    // Reverse iterators — note: B+ tree is forward-only; these are placeholders
    reverse_iterator rbegin() noexcept { return end(); }
    const_reverse_iterator rbegin() const noexcept { return end(); }
    const_reverse_iterator crbegin() const noexcept { return rbegin(); }

    reverse_iterator rend() noexcept { return begin(); }
    const_reverse_iterator rend() const noexcept { return begin(); }
    const_reverse_iterator crend() const noexcept { return rend(); }

    // ---- key comparison ----

    key_compare key_comp() const noexcept { return comp_; }

    // ============================================================
    // Find
    // ============================================================
    iterator find(const Key& k) {
        auto [leaf, pos] = find_in_leaf(k);
        return (leaf && is_key_equal(k, leaf->keys[pos])) ? iterator{leaf, pos} : end();
    }

    const_iterator find(const Key& k) const {
        return const_cast<bplus_tree*>(this)->find(k);
    }

    // ============================================================
    // Count / Contains
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
        auto [leaf, pos] = find_in_leaf(k);
        if (!leaf) return end();
        if (pos >= leaf->num_keys) {
            // All keys in leaf are < k; go to next leaf
            if (leaf->next_leaf && leaf->next_leaf->num_keys > 0) {
                return {leaf->next_leaf, 0};
            }
            return end();
        }
        return {leaf, pos};
    }

    const_iterator lower_bound(const Key& k) const {
        return const_cast<bplus_tree*>(this)->lower_bound(k);
    }

    // ============================================================
    // Upper bound — first element > k
    // ============================================================
    iterator upper_bound(const Key& k) {
        auto [leaf, pos] = find_in_leaf(k);
        if (!leaf) return end();

        // Scan forward past equal keys
        while (pos < leaf->num_keys && !comp_(k, leaf->keys[pos])) {
            ++pos;
        }

        if (pos >= leaf->num_keys) {
            if (leaf->next_leaf && leaf->next_leaf->num_keys > 0) {
                return {leaf->next_leaf, 0};
            }
            return end();
        }
        return {leaf, pos};
    }

    const_iterator upper_bound(const Key& k) const {
        return const_cast<bplus_tree*>(this)->upper_bound(k);
    }

    // ============================================================
    // Equal range
    // ============================================================
    pair<iterator, iterator> equal_range(const Key& k) {
        return {lower_bound(k), upper_bound(k)};
    }

    pair<const_iterator, const_iterator> equal_range(const Key& k) const {
        return {lower_bound(k), upper_bound(k)};
    }

    // ============================================================
    // Insert (unique keys)
    // ============================================================
    pair<iterator, bool> insert_unique(const value_type& v) {
        const Key& k = KeyOfValue()(v);

        if (!root_) {
            init_empty();
        }

        // Split root if it's full (preemptive split on the way down)
        if (root_->is_full()) {
            split_root();
        }

        return insert_into_leaf(root_, k, v);
    }

    // ============================================================
    // Insert (allow duplicates)
    // ============================================================
    iterator insert_multi(const value_type& v) {
        const Key& k = KeyOfValue()(v);

        if (!root_) init_empty();
        if (root_->is_full()) split_root();

        return insert_into_leaf_multi(root_, k, v);
    }

    // ============================================================
    // Emplace — construct in place (unique keys)
    // ============================================================
    template<typename... Args>
    pair<iterator, bool> emplace_unique(Args&&... args) {
        value_type v(zstl::forward<Args>(args)...);
        return insert_unique(zstl::move(v));
    }

    // ============================================================
    // Erase
    // ============================================================
    size_type erase(const Key& k) {
        auto [leaf, pos] = find_in_leaf(k);
        if (!leaf || pos >= leaf->num_keys || !is_key_equal(k, leaf->keys[pos])) {
            return 0;
        }
        erase_from_leaf(leaf, pos);
        return 1;
    }

    void erase(iterator pos) {
        if (pos.leaf && pos.pos < pos.leaf->num_keys) {
            erase_from_leaf(pos.leaf, pos.pos);
        }
    }

    void erase(const_iterator pos) {
        erase(iterator(pos.leaf, pos.pos));
    }

    // ============================================================
    // Clear
    // ============================================================
    void clear() noexcept {
        if (root_) {
            clear_recursive(root_);
            root_       = nullptr;
            first_leaf_ = nullptr;
            size_       = 0;
        }
    }

    // ============================================================
    // Swap
    // ============================================================
    void swap(bplus_tree& other) noexcept {
        using zstl::swap;
        swap(root_,       other.root_);
        swap(first_leaf_, other.first_leaf_);
        swap(size_,       other.size_);
        swap(comp_,       other.comp_);
    }

    // ============================================================
    // First / Last — direct access to extremes
    // ============================================================
    Value* first_value() noexcept {
        if (first_leaf_ && first_leaf_->num_keys > 0)
            return &first_leaf_->values[0];
        return nullptr;
    }

    const Value* first_value() const noexcept {
        if (first_leaf_ && first_leaf_->num_keys > 0)
            return &first_leaf_->values[0];
        return nullptr;
    }

    Value* last_value() noexcept {
        if (!root_) return nullptr;
        Node* leaf = root_;
        while (!leaf->is_leaf()) {
            leaf = leaf->children[leaf->num_keys];
        }
        if (leaf->num_keys > 0) return &leaf->values[leaf->num_keys - 1];
        return nullptr;
    }

    const Value* last_value() const noexcept {
        return const_cast<bplus_tree*>(this)->last_value();
    }

private:
    // ---- initialization ----

    void init_empty() {
        root_       = allocate_node(NodeType::LEAF);
        first_leaf_ = root_;
        size_       = 0;
    }

    // ---- node allocation ----

    Node* allocate_node(NodeType type) {
        Node* node = node_alloc_traits::allocate(alloc_, 1);
        construct(node, type);
        return node;
    }

    void deallocate_node(Node* node) noexcept {
        destroy(node);
        node_alloc_traits::deallocate(alloc_, node, 1);
    }

    void clear_recursive(Node* x) noexcept {
        if (!x) return;
        if (x->is_internal()) {
            for (size_t i = 0; i <= x->num_keys; ++i) {
                clear_recursive(x->children[i]);
            }
        } else {
            // Leaf node — destroy values
            for (size_t i = 0; i < x->num_keys; ++i) {
                destroy(&x->values[i]);
                destroy(&x->keys[i]);
            }
        }
        deallocate_node(x);
    }

    // ---- key comparison helpers ----

    bool is_key_equal(const Key& a, const Key& b) const {
        return !comp_(a, b) && !comp_(b, a);
    }

    // ---- find_in_leaf ----

    // Returns {leaf, position}. position is the index where k would be
    // (the first position where keys[pos] >= k, or num_keys if all are < k).
    pair<Node*, size_t> find_in_leaf(const Key& k) const {
        if (!root_) return {nullptr, 0};
        Node* x = root_;
        while (x->is_internal()) {
            size_t i = x->child_index_for(k, comp_);
            x = x->children[i];
        }
        // x is now a leaf — find position
        size_t pos = 0;
        while (pos < x->num_keys && comp_(x->keys[pos], k)) {
            ++pos;
        }
        return {x, pos};
    }

    // ============================================================
    // Split root — create a new root with two children
    // ============================================================
    void split_root() {
        Node* old_root = root_;
        Node* new_root = allocate_node(NodeType::INTERNAL);
        Node* new_child = allocate_node(old_root->type);

        // Split old root into old_root (first half) and new_child (second half)
        size_t mid = old_root->num_keys / 2;
        size_t second_half_size = old_root->num_keys - mid;

        // Move second half to new_child
        new_child->num_keys = second_half_size;
        for (size_t i = 0; i < second_half_size; ++i) {
            new_child->keys[i] = zstl::move(old_root->keys[mid + i]);
            if (old_root->is_leaf()) {
                new_child->values[i] = zstl::move(old_root->values[mid + i]);
            }
        }
        old_root->num_keys = mid;

        // Move children for internal nodes
        if (old_root->is_internal()) {
            for (size_t i = 0; i <= second_half_size; ++i) {
                new_child->children[i] = old_root->children[mid + i];
                if (new_child->children[i]) {
                    new_child->children[i]->parent = new_child;
                }
                old_root->children[mid + i] = nullptr;
            }
        }

        // Update leaf linked list
        if (old_root->is_leaf()) {
            new_child->next_leaf = old_root->next_leaf;
            old_root->next_leaf = new_child;
        }

        // Set up new root
        new_root->num_keys = 1;
        // The separator key: the maximum key of the left child
        if (old_root->is_leaf()) {
            new_root->keys[0] = old_root->keys[old_root->num_keys - 1];
        } else {
            // For internal nodes, use the first key of the right child
            new_root->keys[0] = get_min_key(new_child);
        }
        new_root->children[0] = old_root;
        new_root->children[1] = new_child;
        old_root->parent = new_root;
        new_child->parent = new_root;

        root_ = new_root;
    }

    // Get the minimum key in the subtree rooted at node
    Key get_min_key(Node* node) const {
        while (node->is_internal()) {
            node = node->children[0];
        }
        return node->keys[0];
    }

    // Get the maximum key in the subtree rooted at node
    Key get_max_key(Node* node) const {
        while (node->is_internal()) {
            node = node->children[node->num_keys];
        }
        return node->keys[node->num_keys - 1];
    }

    // ============================================================
    // Split child — splits a full child of parent at child_idx
    // ============================================================
    void split_child(Node* parent, size_t child_idx) {
        Node* child = parent->children[child_idx];
        Node* new_node = allocate_node(child->type);
        size_t mid = child->num_keys / 2;
        size_t second_half_size = child->num_keys - mid;

        // Move second half to new_node
        new_node->num_keys = second_half_size;
        for (size_t i = 0; i < second_half_size; ++i) {
            new_node->keys[i] = zstl::move(child->keys[mid + i]);
            if (child->is_leaf()) {
                new_node->values[i] = zstl::move(child->values[mid + i]);
            }
        }
        child->num_keys = mid;

        if (child->is_internal()) {
            for (size_t i = 0; i <= second_half_size; ++i) {
                new_node->children[i] = child->children[mid + i];
                if (new_node->children[i]) {
                    new_node->children[i]->parent = new_node;
                }
                child->children[mid + i] = nullptr;
            }
        }

        // Update leaf linked list
        if (child->is_leaf()) {
            new_node->next_leaf = child->next_leaf;
            child->next_leaf = new_node;
        }

        // Insert new_node into parent at child_idx + 1
        // Shift parent's keys and children
        for (size_t i = parent->num_keys; i > child_idx; --i) {
            parent->keys[i]     = zstl::move(parent->keys[i - 1]);
            parent->children[i + 1] = parent->children[i];
        }
        parent->children[child_idx + 1] = new_node;
        // The separator key is the max key of the left child
        parent->keys[child_idx] = get_max_key(child);
        ++parent->num_keys;
        new_node->parent = parent;
    }

    // ============================================================
    // Update separator keys from leaf to root
    // After modifying a leaf, parent keys may need updating
    // ============================================================
    void update_separator_keys(Node* leaf) {
        Node* child = leaf;
        Node* parent = leaf->parent;
        while (parent) {
            for (size_t i = 0; i < parent->num_keys; ++i) {
                if (parent->children[i] == child && child->num_keys > 0) {
                    parent->keys[i] = get_max_key(child);
                    break;
                }
            }
            child = parent;
            parent = parent->parent;
        }
    }

    // ============================================================
    // Insert into subtree — preemptive split on the way down
    // ============================================================
    pair<iterator, bool> insert_into_leaf(Node* x, const Key& k, const value_type& v) {
        if (x->is_leaf()) {
            // Insert into leaf in sorted order
            size_t pos = 0;
            while (pos < x->num_keys && comp_(x->keys[pos], k)) {
                ++pos;
            }

            // Check for duplicate
            if (pos < x->num_keys && is_key_equal(k, x->keys[pos])) {
                // Update existing
                x->values[pos] = v;
                return {{x, pos}, false};
            }

            // Shift to make room
            for (size_t i = x->num_keys; i > pos; --i) {
                x->keys[i]   = zstl::move(x->keys[i - 1]);
                x->values[i] = zstl::move(x->values[i - 1]);
            }
            x->keys[pos] = k;
            zstl::construct(&x->values[pos], v);
            ++x->num_keys;
            ++size_;

            // Update separator keys up the tree
            if (pos == x->num_keys - 1) {
                update_separator_keys(x);
            }

            return {{x, pos}, true};
        }

        // Internal node — find child to descend
        size_t child_idx = x->child_index_for(k, comp_);

        // Preemptively split child if full
        if (x->children[child_idx]->is_full()) {
            split_child(x, child_idx);
            // After split, determine which child to use
            if (!comp_(k, x->keys[child_idx]) || comp_(x->keys[child_idx], k)) {
                // k > separator key — go to right child
                // Also, if k == separator, we want the right child for insertion
                ++child_idx;
            }
        }

        return insert_into_leaf(x->children[child_idx], k, v);
    }

    iterator insert_into_leaf_multi(Node* x, const Key& k, const value_type& v) {
        if (x->is_leaf()) {
            size_t pos = 0;
            while (pos < x->num_keys && comp_(x->keys[pos], k)) {
                ++pos;
            }

            // Insert even if duplicate exists
            for (size_t i = x->num_keys; i > pos; --i) {
                x->keys[i]   = zstl::move(x->keys[i - 1]);
                x->values[i] = zstl::move(x->values[i - 1]);
            }
            x->keys[pos] = k;
            zstl::construct(&x->values[pos], v);
            ++x->num_keys;
            ++size_;

            if (pos == x->num_keys - 1) {
                update_separator_keys(x);
            }

            return {x, pos};
        }

        size_t child_idx = x->child_index_for(k, comp_);
        if (x->children[child_idx]->is_full()) {
            split_child(x, child_idx);
            if (!comp_(k, x->keys[child_idx]) || comp_(x->keys[child_idx], k)) {
                ++child_idx;
            }
        }

        return insert_into_leaf_multi(x->children[child_idx], k, v);
    }

    // ============================================================
    // Erase from leaf
    // ============================================================
    void erase_from_leaf(Node* leaf, size_t pos) {
        // Destroy the value
        destroy(&leaf->values[pos]);
        destroy(&leaf->keys[pos]);

        // Shift remaining elements
        for (size_t i = pos; i < leaf->num_keys - 1; ++i) {
            leaf->keys[i]   = zstl::move(leaf->keys[i + 1]);
            leaf->values[i] = zstl::move(leaf->values[i + 1]);
        }
        --leaf->num_keys;
        --size_;

        // Update separator keys
        update_separator_keys(leaf);

        // Handle underflow
        if (leaf->is_underflow()) {
            handle_underflow(leaf);
        }

        // If root becomes empty
        if (root_->num_keys == 0) {
            if (root_->is_internal()) {
                Node* new_root = root_->children[0];
                if (new_root) new_root->parent = nullptr;
                deallocate_node(root_);
                root_ = new_root;
                if (root_->is_leaf()) {
                    first_leaf_ = root_;
                }
            } else if (size_ == 0) {
                // Empty leaf — keep it but it has 0 keys
                first_leaf_ = root_;
            }
        }
    }

    // ============================================================
    // Handle underflow — borrow from sibling or merge
    // ============================================================
    void handle_underflow(Node* node) {
        if (!node->parent) return;  // Root doesn't underflow

        Node* parent = node->parent;

        // Find node's index in parent
        size_t idx = 0;
        while (idx <= parent->num_keys && parent->children[idx] != node) {
            ++idx;
        }

        // Try to borrow from left sibling
        if (idx > 0) {
            Node* left_sib = parent->children[idx - 1];
            if (left_sib->num_keys > kMinKeysLeaf) {
                borrow_from_left(node, left_sib, parent, idx - 1);
                return;
            }
        }

        // Try to borrow from right sibling
        if (idx < parent->num_keys) {
            Node* right_sib = parent->children[idx + 1];
            if (right_sib->num_keys > kMinKeysLeaf) {
                borrow_from_right(node, right_sib, parent, idx);
                return;
            }
        }

        // Merge with a sibling
        if (idx > 0) {
            merge_nodes(parent->children[idx - 1], node, parent, idx - 1);
        } else {
            merge_nodes(node, parent->children[idx + 1], parent, idx);
        }
    }

    // Borrow one element from left sibling
    void borrow_from_left(Node* node, Node* left, Node* parent, size_t sep_idx) {
        // Shift node's elements right to make room
        for (size_t i = node->num_keys; i > 0; --i) {
            node->keys[i]   = zstl::move(node->keys[i - 1]);
            if (node->is_leaf()) node->values[i] = zstl::move(node->values[i - 1]);
            else node->children[i + 1] = node->children[i];
        }
        if (node->is_internal()) {
            node->children[1] = node->children[0];
        }

        // Move last element from left to node
        --left->num_keys;
        node->keys[0] = zstl::move(left->keys[left->num_keys]);
        if (node->is_leaf()) {
            node->values[0] = zstl::move(left->values[left->num_keys]);
        } else {
            node->children[0] = left->children[left->num_keys + 1];
            if (node->children[0]) node->children[0]->parent = node;
        }
        ++node->num_keys;

        // Update parent separator
        parent->keys[sep_idx] = get_max_key(left);
    }

    // Borrow one element from right sibling
    void borrow_from_right(Node* node, Node* right, Node* parent, size_t sep_idx) {
        // Move first element from right to node
        node->keys[node->num_keys] = zstl::move(right->keys[0]);
        if (node->is_leaf()) {
            node->values[node->num_keys] = zstl::move(right->values[0]);
        } else {
            node->children[node->num_keys + 1] = right->children[0];
            if (node->children[node->num_keys + 1])
                node->children[node->num_keys + 1]->parent = node;
        }
        ++node->num_keys;

        // Shift right's elements left
        --right->num_keys;
        for (size_t i = 0; i < right->num_keys; ++i) {
            right->keys[i] = zstl::move(right->keys[i + 1]);
            if (right->is_leaf()) right->values[i] = zstl::move(right->values[i + 1]);
            else right->children[i] = right->children[i + 1];
        }
        if (right->is_internal()) {
            right->children[right->num_keys] = right->children[right->num_keys + 1];
        }

        // Update parent separator
        parent->keys[sep_idx] = get_max_key(node);
    }

    // Merge right into left, then remove right
    void merge_nodes(Node* left, Node* right, Node* parent, size_t sep_idx) {
        // Move all elements from right to left
        for (size_t i = 0; i < right->num_keys; ++i) {
            left->keys[left->num_keys + i] = zstl::move(right->keys[i]);
            if (left->is_leaf()) {
                left->values[left->num_keys + i] = zstl::move(right->values[i]);
            }
        }
        if (left->is_internal()) {
            for (size_t i = 0; i <= right->num_keys; ++i) {
                left->children[left->num_keys + i + 1] = right->children[i];
                if (right->children[i]) {
                    right->children[i]->parent = left;
                }
            }
        }

        left->num_keys += right->num_keys;

        // Update leaf linked list
        if (left->is_leaf()) {
            left->next_leaf = right->next_leaf;
        }

        // Remove right from parent
        for (size_t i = sep_idx; i < parent->num_keys - 1; ++i) {
            parent->keys[i] = zstl::move(parent->keys[i + 1]);
            parent->children[i + 1] = parent->children[i + 2];
        }
        --parent->num_keys;

        deallocate_node(right);

        // Handle parent underflow
        if (parent->is_underflow()) {
            handle_underflow(parent);
        }
    }
};

} // namespace detail
} // namespace zstl
