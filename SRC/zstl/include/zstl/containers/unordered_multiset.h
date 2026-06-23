// zstl unordered_multiset — hash table backed unordered multi-key set
//
// An unordered associative container that contains a set of keys with
// potentially duplicate elements. Search, insertion, and removal have
// average constant-time complexity.
// Uses Robin Hood open-addressing hash table internally with insert_multi.
//
// Template params:
//   Key      — key type (also serves as value_type)
//   Hash     — hash functor (default: detail::hash<Key>)
//   KeyEqual — key equality functor (default: equal_to<Key>)
//   Alloc    — allocator
#pragma once

#include <initializer_list>

#include "zstl/containers/detail/hashtable.h"
#include "zstl/memory/utility.h"
#include "zstl/memory/construct.h"

namespace zstl {

template<typename Key,
         typename Hash = detail::hash<Key>,
         typename KeyEqual = equal_to<Key>,
         typename Alloc = default_alloc<char>>
class unordered_multiset {
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

    unordered_multiset() = default;

    explicit unordered_multiset(size_type bucket_count,
                                 const Hash& hash = Hash(),
                                 const KeyEqual& equal = KeyEqual())
        : table_(bucket_count, hash, equal) {}

    template<typename InputIt>
    unordered_multiset(InputIt first, InputIt last,
                       size_type bucket_count = 0,
                       const Hash& hash = Hash(),
                       const KeyEqual& equal = KeyEqual())
        : table_(bucket_count, hash, equal) {
        insert(first, last);
    }

    unordered_multiset(const unordered_multiset& other) = default;
    unordered_multiset(unordered_multiset&& other) noexcept = default;

    unordered_multiset(std::initializer_list<value_type> init,
                       size_type bucket_count = 0,
                       const Hash& hash = Hash(),
                       const KeyEqual& equal = KeyEqual())
        : table_(bucket_count, hash, equal) {
        insert(init.begin(), init.end());
    }

    unordered_multiset& operator=(const unordered_multiset& other) = default;
    unordered_multiset& operator=(unordered_multiset&& other) noexcept = default;

    unordered_multiset& operator=(std::initializer_list<value_type> ilist) {
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

    // Insert — duplicate keys allowed (always succeeds)
    iterator insert(const value_type& value) {
        return table_.insert_multi(value);
    }

    iterator insert(const_iterator hint, const value_type& value) {
        return table_.insert_multi(value);
    }

    template<typename InputIt>
    void insert(InputIt first, InputIt last) {
        for (; first != last; ++first) {
            insert(*first);
        }
    }

    void insert(std::initializer_list<value_type> ilist) {
        insert(ilist.begin(), ilist.end());
    }

    // Emplace — always inserts (duplicate keys allowed)
    template<typename... Args>
    iterator emplace(Args&&... args) {
        return table_.emplace_multi(zstl::forward<Args>(args)...);
    }

    template<typename... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args) {
        return table_.emplace_multi(zstl::forward<Args>(args)...);
    }

    // Erase
    iterator erase(const_iterator pos) {
        auto next = pos;
        ++next;
        table_.erase(pos);
        return next;
    }

    iterator erase(const_iterator first, const_iterator last) {
        while (first != last) {
            first = erase(first);
        }
        return iterator(const_cast<value_type*>(&*last));
    }

    size_type erase(const Key& key) {
        // Erase all elements with the given key
        return table_.erase_multi(key);
    }

    void clear() noexcept { table_.clear(); }

    // ---- observers ----

    hasher hash_function() const noexcept { return table_.hash_function(); }
    key_equal key_eq() const noexcept { return table_.key_eq(); }

    // ---- lookup ----

    iterator find(const Key& key) { return table_.find(key); }
    const_iterator find(const Key& key) const { return table_.find(key); }

    size_type count(const Key& key) const {
        // Count all elements matching the key
        size_type n = 0;
        auto range = equal_range(key);
        for (auto it = range.first; it != range.second; ++it) {
            ++n;
        }
        return n;
    }

    bool contains(const Key& key) const { return find(key) != end(); }

    pair<iterator, iterator> equal_range(const Key& key) {
        return table_.equal_range(key);
    }

    pair<const_iterator, const_iterator> equal_range(const Key& key) const {
        return table_.equal_range(key);
    }

    // ---- bucket interface ----

    size_type bucket_count() const noexcept { return table_.bucket_count(); }
    size_type max_bucket_count() const noexcept { return table_.max_bucket_count(); }
    size_type bucket_size(size_type n) const { return table_.bucket_size(n); }
    size_type bucket(const Key& key) const { return table_.bucket(key); }

    // ---- hash policy ----

    float load_factor() const noexcept { return table_.load_factor(); }
    float max_load_factor() const noexcept { return table_.max_load_factor(); }
    void max_load_factor(float ml) { table_.max_load_factor(ml); }
    void rehash(size_type count) { table_.rehash(count); }
    void reserve(size_type count) { table_.reserve(count); }

    // ---- merge ----

    template<typename H2, typename KE2>
    void merge(unordered_multiset<Key, H2, KE2, Alloc>& source) {
        auto it = source.begin();
        while (it != source.end()) {
            insert(*it);
            it = source.erase(it);
        }
    }

    // ---- swap ----

    void swap(unordered_multiset& other) noexcept { table_.swap(other.table_); }
};

// ---- comparison operators ----
// Equality: same multiset of elements

template<typename K, typename H, typename KE, typename A>
bool operator==(const unordered_multiset<K, H, KE, A>& a,
                const unordered_multiset<K, H, KE, A>& b) {
    if (a.size() != b.size()) return false;
    // Since elements are duplicated, compare by counting occurrences
    auto a_copy = a;
    for (const auto& elem : b) {
        auto it = a_copy.find(elem);
        if (it == a_copy.end()) return false;
        a_copy.erase(it);
    }
    return a_copy.empty();
}

template<typename K, typename H, typename KE, typename A>
bool operator!=(const unordered_multiset<K, H, KE, A>& a,
                const unordered_multiset<K, H, KE, A>& b) {
    return !(a == b);
}

// ---- free function swap ----

template<typename K, typename H, typename KE, typename A>
void swap(unordered_multiset<K, H, KE, A>& a,
          unordered_multiset<K, H, KE, A>& b) noexcept(noexcept(a.swap(b))) {
    a.swap(b);
}

} // namespace zstl
