// zstl unordered_set — hash table backed unordered unique-key set
//
// An unordered associative container that contains a set of unique keys.
// Search, insertion, and removal have average constant-time complexity.
// Uses Robin Hood open-addressing hash table internally.
//
// Design notes:
//   - Robin Hood hashing bounds probe sequence length to ~log2(N)
//   - 64-bit hash caching in each slot avoids re-hashing on probes
//   - Tombstone reclamation during insertion keeps table clean
//   - Default max load factor 0.8; rehash doubles capacity
//   - Power-of-2 table sizes for fast modulo via bitmask
//
// Template params:
//   Key      — key type (also serves as value_type)
//   Hash     — hash functor (default: detail::hash<Key>)
//   KeyEqual — key equality functor (default: equal_to<Key>)
//   Alloc    — allocator (default: default_alloc<char>)
#pragma once

#include <stdexcept>
#include <initializer_list>

#include "zstl/containers/detail/hashtable.h"
#include "zstl/memory/utility.h"
#include "zstl/memory/construct.h"

namespace zstl {

// Forward declaration for merge
template<typename Key, typename Hash, typename KeyEqual, typename Alloc>
class unordered_multiset;

template<typename Key,
         typename Hash = detail::hash<Key>,
         typename KeyEqual = equal_to<Key>,
         typename Alloc = default_alloc<char>>
class unordered_set {
public:
    // ---- type definitions ----
    using key_type        = Key;
    using value_type      = Key;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;
    using hasher          = Hash;
    using key_equal       = KeyEqual;
    using allocator_type  = Alloc;

    using reference       = value_type&;
    using const_reference = const value_type&;
    using pointer         = value_type*;
    using const_pointer   = const value_type*;

private:
    // Key-of-value extractor — identity for sets
    struct key_of_value {
        const Key& operator()(const Key& k) const noexcept { return k; }
    };

    using table_type = detail::hashtable<Key, value_type, key_of_value, Hash, KeyEqual, Alloc>;
    table_type table_;

public:
    using iterator               = typename table_type::iterator;
    using const_iterator         = typename table_type::const_iterator;
    using local_iterator         = iterator;
    using const_local_iterator   = const_iterator;

    // ---- constructors ----

    unordered_set() = default;

    explicit unordered_set(size_type bucket_count,
                           const Hash& hash = Hash(),
                           const KeyEqual& equal = KeyEqual(),
                           const Alloc& alloc = Alloc())
        : table_(bucket_count, hash, equal, alloc) {}

    template<typename InputIt>
    unordered_set(InputIt first, InputIt last,
                  size_type bucket_count = 0,
                  const Hash& hash = Hash(),
                  const KeyEqual& equal = KeyEqual(),
                  const Alloc& alloc = Alloc())
        : table_(bucket_count, hash, equal, alloc) {
        insert(first, last);
    }

    unordered_set(const unordered_set& other) = default;
    unordered_set(unordered_set&& other) noexcept = default;

    unordered_set(std::initializer_list<value_type> init,
                  size_type bucket_count = 0,
                  const Hash& hash = Hash(),
                  const KeyEqual& equal = KeyEqual(),
                  const Alloc& alloc = Alloc())
        : table_(bucket_count, hash, equal, alloc) {
        insert(init.begin(), init.end());
    }

    unordered_set& operator=(const unordered_set& other) = default;
    unordered_set& operator=(unordered_set&& other) noexcept = default;

    unordered_set& operator=(std::initializer_list<value_type> ilist) {
        clear();
        insert(ilist.begin(), ilist.end());
        return *this;
    }

    // ---- allocator ----

    allocator_type get_allocator() const noexcept { return table_.get_allocator(); }

    // ---- iterators ----

    iterator begin() noexcept { return table_.begin(); }
    const_iterator begin() const noexcept { return table_.begin(); }
    const_iterator cbegin() const noexcept { return table_.cbegin(); }

    iterator end() noexcept { return table_.end(); }
    const_iterator end() const noexcept { return table_.end(); }
    const_iterator cend() const noexcept { return table_.cend(); }

    // ---- capacity ----

    bool empty() const noexcept { return table_.empty(); }
    size_type size() const noexcept { return table_.size(); }
    size_type max_size() const noexcept { return table_.max_size(); }

    // ---- modifiers ----

    // Insert — unique keys only; returns iterator and success flag
    pair<iterator, bool> insert(const value_type& value) {
        return table_.insert_unique(value);
    }

    // Insert with hint — hint is advisory for finding insertion point quickly
    // In hash table, hint is ignored since bucket is determined by hash
    iterator insert(const_iterator hint, const value_type& value) {
        return table_.insert_unique(value).first;
    }

    // Move-aware insert — construct value_type from forwarding reference
    template<typename P>
    auto insert(P&& value)
        -> std::enable_if_t<std::is_constructible_v<value_type, P&&>,
                            pair<iterator, bool>> {
        return table_.insert_unique(value_type(zstl::forward<P>(value)));
    }

    // Range insert
    template<typename InputIt>
    void insert(InputIt first, InputIt last) {
        for (; first != last; ++first) {
            insert(*first);
        }
    }

    // Initializer list insert
    void insert(std::initializer_list<value_type> ilist) {
        insert(ilist.begin(), ilist.end());
    }

    // Emplace — construct element in place, unique keys only
    template<typename... Args>
    pair<iterator, bool> emplace(Args&&... args) {
        return table_.emplace_unique(zstl::forward<Args>(args)...);
    }

    // Emplace with hint
    template<typename... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args) {
        return table_.emplace_unique(zstl::forward<Args>(args)...).first;
    }

    // Try emplace — insert if key doesn't exist, otherwise do nothing
    template<typename... Args>
    pair<iterator, bool> try_emplace(const Key& k, Args&&... args) {
        auto it = find(k);
        if (it != end()) return {it, false};
        return emplace(zstl::forward<Args>(args)...);
    }

    template<typename... Args>
    pair<iterator, bool> try_emplace(Key&& k, Args&&... args) {
        auto it = find(k);
        if (it != end()) return {it, false};
        return emplace(zstl::move(k), zstl::forward<Args>(args)...);
    }

    // Erase — element pointed to by iterator; returns next iterator
    iterator erase(const_iterator pos) {
        auto next = pos;
        ++next;
        table_.erase(pos);
        return next;
    }

    // Erase range [first, last)
    iterator erase(const_iterator first, const_iterator last) {
        while (first != last) {
            first = erase(first);
        }
        return iterator(const_cast<value_type*>(&*last));
    }

    // Erase by key — returns number of elements removed (0 or 1)
    size_type erase(const Key& key) {
        return table_.erase(key);
    }

    // Remove all elements
    void clear() noexcept { table_.clear(); }

    // ---- observers ----

    hasher hash_function() const noexcept { return table_.hash_function(); }
    key_equal key_eq() const noexcept { return table_.key_eq(); }

    // ---- lookup ----

    // Find element by key; returns end() if not found
    iterator find(const Key& key) { return table_.find(key); }
    const_iterator find(const Key& key) const { return table_.find(key); }

    // Count elements with key (0 or 1 for unique-key containers)
    size_type count(const Key& key) const { return table_.count(key); }

    // Check if key exists in the set
    bool contains(const Key& key) const { return table_.contains(key); }

    // Equal range — range of elements matching key (single element or empty)
    pair<iterator, iterator> equal_range(const Key& key) {
        return table_.equal_range(key);
    }

    pair<const_iterator, const_iterator> equal_range(const Key& key) const {
        return table_.equal_range(key);
    }

    // ---- bucket interface ----

    // Number of buckets in the hash table
    size_type bucket_count() const noexcept { return table_.bucket_count(); }

    // Maximum possible number of buckets
    size_type max_bucket_count() const noexcept { return table_.max_bucket_count(); }

    // Number of elements in a specific bucket
    size_type bucket_size(size_type n) const { return table_.bucket_size(n); }

    // Bucket index for a given key
    size_type bucket(const Key& key) const { return table_.bucket(key); }

    // Iterator to the beginning of a specific bucket
    local_iterator begin(size_type n) { return table_.begin(n); }
    const_local_iterator begin(size_type n) const { return table_.begin(n); }
    const_local_iterator cbegin(size_type n) const { return table_.cbegin(n); }

    // Iterator to the end of a specific bucket
    local_iterator end(size_type n) { return table_.end(n); }
    const_local_iterator end(size_type n) const { return table_.end(n); }
    const_local_iterator cend(size_type n) const { return table_.cend(n); }

    // ---- hash policy ----

    // Current load factor = size / bucket_count
    float load_factor() const noexcept { return table_.load_factor(); }

    // Maximum load factor before rehash (default 0.8)
    float max_load_factor() const noexcept { return table_.max_load_factor(); }
    void max_load_factor(float ml) { table_.max_load_factor(ml); }

    // Rehash to at least the specified number of buckets
    void rehash(size_type count) { table_.rehash(count); }

    // Reserve space for at least count elements (pre-rehashes if needed)
    void reserve(size_type count) { table_.reserve(count); }

    // ---- merge — splice elements from another container ----
    // Elements from source that don't exist in this container are moved;
    // elements that already exist remain in source.

    template<typename H2, typename KE2>
    void merge(unordered_set<Key, H2, KE2, Alloc>& source) {
        auto it = source.begin();
        while (it != source.end()) {
            if (find(*it) == end()) {
                insert(*it);
                it = source.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Merge from a multi-set (allow extracting elements from a multiset)
    template<typename H2, typename KE2>
    void merge(unordered_multiset<Key, H2, KE2, Alloc>& source) {
        auto it = source.begin();
        while (it != source.end()) {
            if (find(*it) == end()) {
                insert(*it);
                it = source.erase(it);
            } else {
                ++it;
            }
        }
    }

    // ---- swap ----

    void swap(unordered_set& other) noexcept { table_.swap(other.table_); }
};

// ---- comparison operators ----
// Equality: same elements regardless of order
// Two unordered_sets are equal if they contain the same set of elements.

template<typename K, typename H, typename KE, typename A>
bool operator==(const unordered_set<K, H, KE, A>& a,
                const unordered_set<K, H, KE, A>& b) {
    if (a.size() != b.size()) return false;
    for (const auto& elem : a) {
        if (!b.contains(elem)) return false;
    }
    return true;
}

template<typename K, typename H, typename KE, typename A>
bool operator!=(const unordered_set<K, H, KE, A>& a,
                const unordered_set<K, H, KE, A>& b) {
    return !(a == b);
}

// ---- free function swap ----
// Specialization of swap for ADL-based swapping

template<typename K, typename H, typename KE, typename A>
void swap(unordered_set<K, H, KE, A>& a,
          unordered_set<K, H, KE, A>& b) noexcept(noexcept(a.swap(b))) {
    a.swap(b);
}

} // namespace zstl
