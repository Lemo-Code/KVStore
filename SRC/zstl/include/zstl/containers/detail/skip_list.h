// zstl skip list — probabilistic ordered data structure
// Used by: skip_map, skip_set, and zedis ZSet
//
// Design:
//   - Probabilistic skip list with configurable max level (default 32)
//   - Each node has a random number of forward pointers (0 to max_level)
//   - Promotion probability = 1/4 (each level has 1/4 chance of being promoted)
//   - Expected O(log n) for insert, find, erase
//   - Sentinel header node with max_level forward pointers
//   - Level 0 forms a sorted singly-linked list for iteration
//   - Upper levels provide skip pointers for logarithmic search
//
// Template params:
//   Key        — key type
//   Value      — value type
//   KeyOfValue — functor to extract Key from Value
//   Compare    — strict weak ordering on Key
//   Alloc      — allocator type
#pragma once

#include <cstdint>
#include <cstddef>
#include <random>
#include <type_traits>

#include "zstl/memory/utility.h"
#include "zstl/memory/allocator.h"
#include "zstl/memory/construct.h"
#include "zstl/memory/type_traits.h"
#include "zstl/iterators/iterator_traits.h"

namespace zstl {
namespace detail {

// ============================================================
// Skip list node
// Nodes have variable-sized forward pointer arrays.
// Memory layout: [Node header][key][value][forward[0..level]]
// The forward pointer array is allocated inline after the Node.
// ============================================================
template<typename Key, typename Value>
struct skip_list_node {
    Key   key;
    Value value;
    int   level;  // Number of forward pointers minus 1 (0-based)
    // skip_list_node* forward[level + 1] follows in memory

    // Access forward pointer at index i (0 to level)
    skip_list_node* forward(int i) const {
        return forward_array()[i];
    }

    void set_forward(int i, skip_list_node* node) {
        forward_array()[i] = node;
    }

    skip_list_node** forward_array() const {
        return const_cast<skip_list_node**>(
            reinterpret_cast<skip_list_node* const*>(
                reinterpret_cast<const char*>(this) + sizeof(skip_list_node)));
    }

    // Total size of this node including its forward array
    static size_t node_size(int lvl) {
        return sizeof(skip_list_node) + (lvl + 1) * sizeof(skip_list_node*);
    }
};

// ============================================================
// Skip list iterator — forward only (level 0 traversal)
// ============================================================
template<typename Key, typename Value, typename Ref, typename Ptr>
struct skip_list_iterator {
    using iterator_category = forward_iterator_tag;
    using value_type        = Value;
    using difference_type   = ptrdiff_t;
    using pointer           = Ptr;
    using reference         = Ref;
    using node_type         = skip_list_node<Key, Value>;

    node_type* node;

    skip_list_iterator() noexcept : node(nullptr) {}
    explicit skip_list_iterator(node_type* n) noexcept : node(n) {}

    // Conversion from iterator to const_iterator
    template<typename R, typename P,
             typename = std::enable_if_t<std::is_convertible_v<R, Ref> && std::is_convertible_v<P, Ptr>>>
    skip_list_iterator(const skip_list_iterator<Key, Value, R, P>& other) noexcept
        : node(other.node) {}

    reference operator*() const { return node->value; }
    pointer   operator->() const { return &node->value; }

    skip_list_iterator& operator++() noexcept {
        node = node->forward(0);
        return *this;
    }

    skip_list_iterator operator++(int) noexcept {
        skip_list_iterator tmp = *this;
        node = node->forward(0);
        return tmp;
    }

    bool operator==(const skip_list_iterator& other) const noexcept { return node == other.node; }
    bool operator!=(const skip_list_iterator& other) const noexcept { return node != other.node; }
};

// ============================================================
// Skip list
// ============================================================
template<typename Key, typename Value, typename KeyOfValue,
         typename Compare = less<Key>,
         typename Alloc = default_alloc<char>>
class skip_list {
public:
    // ---- type definitions ----
    using key_type        = Key;
    using value_type      = Value;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;
    using key_compare     = Compare;
    using allocator_type  = Alloc;

    using node_type = skip_list_node<Key, Value>;

    using iterator       = skip_list_iterator<Key, Value, Value&, Value*>;
    using const_iterator = skip_list_iterator<Key, Value, const Value&, const Value*>;

    static constexpr int    kMaxLevel            = 32;
    static constexpr double kPromotionProbability = 0.25;

private:
    using node_alloc       = typename std::allocator_traits<Alloc>::template rebind_alloc<char>;
    using node_alloc_traits = std::allocator_traits<node_alloc>;

    node_type*    header_     = nullptr;  // Sentinel node (has kMaxLevel forward pointers)
    int           max_level_  = 0;        // Maximum level currently in use (0-indexed)
    size_type     size_       = 0;
    [[no_unique_address]] Compare     comp_;
    [[no_unique_address]] node_alloc  alloc_;

    // Random number generation
    std::mt19937                          rng_;
    std::uniform_real_distribution<double> dist_;

public:
    // ---- constructors / destructor ----

    skip_list()
        : rng_(std::random_device{}()), dist_(0.0, 1.0) {
        init_header();
    }

    explicit skip_list(const Compare& comp)
        : comp_(comp), rng_(std::random_device{}()), dist_(0.0, 1.0) {
        init_header();
    }

    skip_list(const skip_list& other)
        : comp_(other.comp_), rng_(std::random_device{}()), dist_(0.0, 1.0) {
        init_header();
        for (auto it = other.begin(); it != other.end(); ++it) {
            insert_multi(*it);
        }
    }

    skip_list(skip_list&& other) noexcept
        : header_(other.header_), max_level_(other.max_level_),
          size_(other.size_), comp_(zstl::move(other.comp_)),
          rng_(zstl::move(other.rng_)), dist_(zstl::move(other.dist_)) {
        other.header_    = nullptr;
        other.max_level_ = 0;
        other.size_      = 0;
    }

    skip_list& operator=(const skip_list& other) {
        if (this != &other) {
            clear();
            comp_ = other.comp_;
            for (auto it = other.begin(); it != other.end(); ++it) {
                insert_multi(*it);
            }
        }
        return *this;
    }

    skip_list& operator=(skip_list&& other) noexcept {
        if (this != &other) {
            clear();
            destroy_header();
            header_    = other.header_;
            max_level_ = other.max_level_;
            size_      = other.size_;
            comp_      = zstl::move(other.comp_);
            rng_       = zstl::move(other.rng_);
            dist_      = zstl::move(other.dist_);
            other.header_    = nullptr;
            other.max_level_ = 0;
            other.size_      = 0;
        }
        return *this;
    }

    ~skip_list() {
        clear();
        destroy_header();
    }

    // ---- iterators ----

    iterator begin() noexcept {
        return iterator(header_ ? header_->forward(0) : nullptr);
    }

    const_iterator begin() const noexcept {
        return const_iterator(header_ ? header_->forward(0) : nullptr);
    }

    const_iterator cbegin() const noexcept { return begin(); }

    iterator end() noexcept { return iterator(nullptr); }
    const_iterator end() const noexcept { return const_iterator(nullptr); }
    const_iterator cend() const noexcept { return end(); }

    // ---- capacity ----

    size_type size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    size_type max_size() const noexcept { return static_cast<size_t>(-1); }

    // ---- key comparison ----

    key_compare key_comp() const noexcept { return comp_; }

    // ============================================================
    // Insert (unique keys)
    // ============================================================
    pair<iterator, bool> insert_unique(const value_type& v) {
        const Key& k = KeyOfValue()(v);
        node_type* update[kMaxLevel + 1];
        node_type* x = search_with_update(k, update);

        // Check for duplicate at level 0
        if (x && is_key_equal(k, x->key)) {
            return {iterator(x), false};
        }

        int lvl = random_level();
        node_type* n = create_node(k, v, lvl);
        link_node(n, update, lvl);
        return {iterator(n), true};
    }

    // ============================================================
    // Insert (allow duplicates)
    // ============================================================
    iterator insert_multi(const value_type& v) {
        const Key& k = KeyOfValue()(v);
        node_type* update[kMaxLevel + 1];
        search_with_update(k, update);

        int lvl = random_level();
        node_type* n = create_node(k, v, lvl);
        link_node(n, update, lvl);
        return iterator(n);
    }

    // ============================================================
    // Emplace (unique)
    // ============================================================
    template<typename... Args>
    pair<iterator, bool> emplace_unique(Args&&... args) {
        value_type v(zstl::forward<Args>(args)...);
        return insert_unique(zstl::move(v));
    }

    template<typename... Args>
    iterator emplace_multi(Args&&... args) {
        value_type v(zstl::forward<Args>(args)...);
        return insert_multi(zstl::move(v));
    }

    // ============================================================
    // Find
    // ============================================================
    iterator find(const Key& k) {
        node_type* x = search(k);
        if (x && is_key_equal(k, x->key)) {
            return iterator(x);
        }
        return end();
    }

    const_iterator find(const Key& k) const {
        node_type* x = const_cast<skip_list*>(this)->search(k);
        if (x && const_cast<skip_list*>(this)->is_key_equal(k, x->key)) {
            return const_iterator(x);
        }
        return end();
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
        node_type* update[kMaxLevel + 1];
        node_type* x = search_with_update(k, update);
        // x is the node right after where k would be inserted
        // i.e., the first node with key >= k
        // But our search returns the node BEFORE the target
        // Actually: search_with_update returns header_->forward(0) after the search,
        // but we track the predecessor at each level. The forward(0) from the
        // update[0] predecessor is the first node >= k.
        x = update[0]->forward(0);
        return iterator(x);
    }

    const_iterator lower_bound(const Key& k) const {
        return const_cast<skip_list*>(this)->lower_bound(k);
    }

    // ============================================================
    // Upper bound — first element > k
    // ============================================================
    iterator upper_bound(const Key& k) {
        iterator it = lower_bound(k);
        // Skip past equal keys
        while (it != end() && is_key_equal(k, (*it))) {
            ++it;
        }
        return it;
    }

    const_iterator upper_bound(const Key& k) const {
        return const_cast<skip_list*>(this)->upper_bound(k);
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
    // Erase
    // ============================================================
    size_type erase(const Key& k) {
        node_type* update[kMaxLevel + 1];
        node_type* x = search_with_update(k, update);

        if (!x || !is_key_equal(k, x->key)) {
            return 0;
        }

        unlink_node(x, update);
        destroy_node(x);
        return 1;
    }

    void erase(iterator pos) {
        if (!pos.node) return;
        // Need to rebuild update array — walk from header
        node_type* update[kMaxLevel + 1];
        search_with_update(pos.node->key, update);
        if (update[0]->forward(0) == pos.node) {
            unlink_node(pos.node, update);
            destroy_node(pos.node);
        }
    }

    void erase(const_iterator pos) {
        erase(iterator(const_cast<node_type*>(pos.node)));
    }

    // ============================================================
    // Rank — count elements strictly less than k
    // For exact rank, we need span widths. This implementation
    // uses a simple O(n) scan for accuracy. For a production ZSet,
    // span widths at each level should be maintained.
    // ============================================================
    size_type rank(const Key& k) const {
        size_type r = 0;
        node_type* x = header_->forward(0);
        while (x && comp_(x->key, k)) {
            ++r;
            x = x->forward(0);
        }
        return r;
    }

    // ============================================================
    // Element at rank (0-based) — O(n) scan
    // ============================================================
    Value* at_rank(size_type r) {
        node_type* x = header_->forward(0);
        while (x && r > 0) {
            x = x->forward(0);
            --r;
        }
        return x ? &x->value : nullptr;
    }

    const Value* at_rank(size_type r) const {
        return const_cast<skip_list*>(this)->at_rank(r);
    }

    // ============================================================
    // First / Last
    // ============================================================
    Value* first() noexcept {
        node_type* x = header_->forward(0);
        return x ? &x->value : nullptr;
    }

    const Value* first() const noexcept {
        node_type* x = header_->forward(0);
        return x ? &x->value : nullptr;
    }

    Value* last() noexcept {
        node_type* x = header_;
        for (int i = max_level_; i >= 0; --i) {
            while (x->forward(i)) {
                x = x->forward(i);
            }
        }
        return (x != header_) ? &x->value : nullptr;
    }

    const Value* last() const noexcept {
        return const_cast<skip_list*>(this)->last();
    }

    // ============================================================
    // Clear
    // ============================================================
    void clear() noexcept {
        node_type* x = header_->forward(0);
        while (x) {
            node_type* next = x->forward(0);
            destroy_node(x);
            x = next;
        }
        for (int i = 0; i <= kMaxLevel; ++i) {
            header_->set_forward(i, nullptr);
        }
        max_level_ = 0;
        size_ = 0;
    }

    // ============================================================
    // Swap
    // ============================================================
    void swap(skip_list& other) noexcept {
        zstl::swap(header_,    other.header_);
        zstl::swap(max_level_, other.max_level_);
        zstl::swap(size_,      other.size_);
        zstl::swap(comp_,      other.comp_);
        std::swap(rng_,       other.rng_);
        std::swap(dist_,      other.dist_);
    }

private:
    // ---- internal helpers ----

    void init_header() {
        // Allocate header with kMaxLevel forward pointers
        size_t sz = sizeof(node_type) + (kMaxLevel + 1) * sizeof(node_type*);
        char* mem = node_alloc_traits::allocate(alloc_, sz);
        header_ = reinterpret_cast<node_type*>(mem);
        // Initialize header (key and value are unused)
        header_->level = kMaxLevel;
        for (int i = 0; i <= kMaxLevel; ++i) {
            header_->set_forward(i, nullptr);
        }
    }

    void destroy_header() noexcept {
        if (header_) {
            size_t sz = sizeof(node_type) + (kMaxLevel + 1) * sizeof(node_type*);
            node_alloc_traits::deallocate(alloc_, reinterpret_cast<char*>(header_), sz);
            header_ = nullptr;
        }
    }

    node_type* create_node(const Key& k, const value_type& v, int lvl) {
        size_t sz = node_type::node_size(lvl);
        char* mem = node_alloc_traits::allocate(alloc_, sz);
        node_type* node = reinterpret_cast<node_type*>(mem);
        construct(&node->key, k);
        construct(&node->value, v);
        node->level = lvl;
        for (int i = 0; i <= lvl; ++i) {
            node->set_forward(i, nullptr);
        }
        return node;
    }

    void destroy_node(node_type* node) noexcept {
        size_t sz = node_type::node_size(node->level);
        destroy(&node->value);
        destroy(&node->key);
        node_alloc_traits::deallocate(alloc_, reinterpret_cast<char*>(node), sz);
    }

    bool is_key_equal(const Key& a, const Key& b) const {
        return !comp_(a, b) && !comp_(b, a);
    }

    int random_level() {
        int lvl = 0;
        while (dist_(rng_) < kPromotionProbability && lvl < kMaxLevel) {
            ++lvl;
        }
        return lvl;
    }

    // ============================================================
    // Search — return node with key >= k at level 0, or nullptr
    // ============================================================
    node_type* search(const Key& k) const {
        node_type* x = header_;
        for (int i = max_level_; i >= 0; --i) {
            while (x->forward(i) && comp_(x->forward(i)->key, k)) {
                x = x->forward(i);
            }
        }
        return x->forward(0);
    }

    // ============================================================
    // Search with update — also records predecessor at each level.
    // Returns the node at level 0 after the insertion point.
    // update[i] = the rightmost node at level i whose key < k
    // (the node whose forward[i] needs to be updated on insertion)
    // ============================================================
    node_type* search_with_update(const Key& k, node_type* update[]) const {
        node_type* x = header_;
        for (int i = max_level_; i >= 0; --i) {
            while (x->forward(i) && comp_(x->forward(i)->key, k)) {
                x = x->forward(i);
            }
            update[i] = x;
        }
        return x->forward(0);
    }

    // ============================================================
    // Link a node into the skip list at all levels up to its level
    // ============================================================
    void link_node(node_type* n, node_type* update[], int lvl) {
        if (lvl > max_level_) {
            for (int i = max_level_ + 1; i <= lvl; ++i) {
                update[i] = header_;
            }
            max_level_ = lvl;
        }

        for (int i = 0; i <= lvl; ++i) {
            n->set_forward(i, update[i]->forward(i));
            update[i]->set_forward(i, n);
        }
        ++size_;
    }

    // ============================================================
    // Unlink a node from all levels
    // ============================================================
    void unlink_node(node_type* n, node_type* update[]) {
        for (int i = 0; i <= max_level_; ++i) {
            if (update[i]->forward(i) == n) {
                update[i]->set_forward(i, n->forward(i));
            }
        }
        --size_;

        // Adjust max_level_ down if necessary
        while (max_level_ > 0 && header_->forward(max_level_) == nullptr) {
            --max_level_;
        }
    }
};

// ============================================================
// Free function swap
// ============================================================
template<typename K, typename V, typename KOV, typename C, typename A>
void swap(skip_list<K, V, KOV, C, A>& a, skip_list<K, V, KOV, C, A>& b)
    noexcept(noexcept(a.swap(b))) {
    a.swap(b);
}

} // namespace detail
} // namespace zstl
