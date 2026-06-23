// zstl unordered_map — hash table backed unordered associative container
//
// An unordered associative container that contains key-value pairs with unique keys.
// Search, insertion, and removal have average constant-time complexity.
// Uses Robin Hood open-addressing hash table internally.
//
// Template params:
//   Key      — key type
//   T        — mapped type
//   Hash     — hash functor (default: detail::hash<Key>)
//   KeyEqual — key equality functor (default: equal_to<Key>)
//   Alloc    — allocator
#pragma once

#include <stdexcept>
#include <initializer_list>

#include "zstl/containers/detail/hashtable.h"
#include "zstl/memory/utility.h"
#include "zstl/memory/construct.h"

namespace zstl {

// Forward declaration for merge
template<typename Key, typename T, typename Hash, typename KeyEqual, typename Alloc>
class unordered_multimap;

template<typename Key, typename T,
         typename Hash = detail::hash<Key>,
         typename KeyEqual = equal_to<Key>,
         typename Alloc = default_alloc<char>>
class unordered_map {
public:
    // ---- type definitions ----
    using key_type        = Key;
    using mapped_type     = T;
    using value_type      = pair<const Key, T>;
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
        const Key& operator()(const value_type& v) const noexcept { return v.first; }
    };

    using table_type = detail::hashtable<Key, value_type, key_of_value, Hash, KeyEqual, Alloc>;
    table_type table_;

public:
    using iterator       = typename table_type::iterator;
    using const_iterator = typename table_type::const_iterator;

    // Local iterator (for bucket iteration — simplified to forward range)
    using local_iterator       = iterator;
    using const_local_iterator = const_iterator;

    // ---- constructors ----

    unordered_map() = default;

    explicit unordered_map(size_type bucket_count,
                           const Hash& hash = Hash(),
                           const KeyEqual& equal = KeyEqual())
        : table_(bucket_count, hash, equal) {}

    template<typename InputIt>
    unordered_map(InputIt first, InputIt last,
                  size_type bucket_count = 0,
                  const Hash& hash = Hash(),
                  const KeyEqual& equal = KeyEqual())
        : table_(bucket_count, hash, equal) {
        insert(first, last);
    }

    unordered_map(const unordered_map& other) = default;
    unordered_map(unordered_map&& other) noexcept = default;

    unordered_map(std::initializer_list<value_type> init,
                  size_type bucket_count = 0,
                  const Hash& hash = Hash(),
                  const KeyEqual& equal = KeyEqual())
        : table_(bucket_count, hash, equal) {
        insert(init.begin(), init.end());
    }

    unordered_map& operator=(const unordered_map& other) = default;
    unordered_map& operator=(unordered_map&& other) noexcept = default;

    unordered_map& operator=(std::initializer_list<value_type> ilist) {
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

    // ---- element access ----

    T& operator[](const Key& key) {
        auto it = find(key);
        if (it != end()) {
            return (*it).second;
        }
        auto result = table_.insert_unique(value_type(key, T()));
        return (*result.first).second;
    }

    T& operator[](Key&& key) {
        auto it = find(key);
        if (it != end()) {
            return (*it).second;
        }
        auto result = table_.insert_unique(value_type(zstl::move(key), T()));
        return (*result.first).second;
    }

    T& at(const Key& key) {
        auto it = find(key);
        if (it == end()) throw std::out_of_range("zstl::unordered_map::at: key not found");
        return (*it).second;
    }

    const T& at(const Key& key) const {
        auto it = find(key);
        if (it == end()) throw std::out_of_range("zstl::unordered_map::at: key not found");
        return (*it).second;
    }

    // ---- modifiers ----

    // Insert
    pair<iterator, bool> insert(const value_type& value) {
        return table_.insert_unique(value);
    }

    template<typename P>
    auto insert(P&& value)
        -> std::enable_if_t<std::is_constructible_v<value_type, P&&>,
                            pair<iterator, bool>> {
        return insert(value_type(zstl::forward<P>(value)));
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

    // Emplace
    template<typename... Args>
    pair<iterator, bool> emplace(Args&&... args) {
        return table_.emplace_unique(zstl::forward<Args>(args)...);
    }

    // Try emplace
    template<typename... Args>
    pair<iterator, bool> try_emplace(const Key& k, Args&&... args) {
        auto it = find(k);
        if (it != end()) return {it, false};
        return emplace(piecewise_construct,
                       std::forward_as_tuple(k),
                       std::forward_as_tuple(zstl::forward<Args>(args)...));
    }

    template<typename... Args>
    pair<iterator, bool> try_emplace(Key&& k, Args&&... args) {
        auto it = find(k);
        if (it != end()) return {it, false};
        return emplace(piecewise_construct,
                       std::forward_as_tuple(zstl::move(k)),
                       std::forward_as_tuple(zstl::forward<Args>(args)...));
    }

    // Insert or assign
    template<typename M>
    pair<iterator, bool> insert_or_assign(const Key& k, M&& obj) {
        auto it = find(k);
        if (it != end()) {
            (*it).second = zstl::forward<M>(obj);
            return {it, false};
        }
        return insert(value_type(k, zstl::forward<M>(obj)));
    }

    template<typename M>
    pair<iterator, bool> insert_or_assign(Key&& k, M&& obj) {
        auto it = find(k);
        if (it != end()) {
            (*it).second = zstl::forward<M>(obj);
            return {it, false};
        }
        return insert(value_type(zstl::move(k), zstl::forward<M>(obj)));
    }

    // Erase
    iterator erase(const_iterator pos) {
        // Compute next before erasing
        auto next = pos;
        ++next;
        table_.erase(pos);
        return next;
    }

    size_type erase(const Key& key) {
        return table_.erase(key);
    }

    void clear() noexcept { table_.clear(); }

    // ---- observers ----

    hasher hash_function() const noexcept { return table_.hash_function(); }
    key_equal key_eq() const noexcept { return table_.key_eq(); }

    // ---- lookup ----

    iterator find(const Key& key) { return table_.find(key); }
    const_iterator find(const Key& key) const { return table_.find(key); }

    size_type count(const Key& key) const { return table_.count(key); }
    bool contains(const Key& key) const { return table_.contains(key); }

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

    template<typename H2, typename P2>
    void merge(unordered_map<Key, T, H2, P2, Alloc>& source) {
        auto it = source.begin();
        while (it != source.end()) {
            if (find((*it).first) == end()) {
                insert(*it);
                it = source.erase(it);
            } else {
                ++it;
            }
        }
    }

    template<typename H2, typename P2>
    void merge(unordered_multimap<Key, T, H2, P2, Alloc>& source) {
        auto it = source.begin();
        while (it != source.end()) {
            if (find((*it).first) == end()) {
                insert(*it);
                it = source.erase(it);
            } else {
                ++it;
            }
        }
    }

    // ---- emplace_hint / insert with hint ----

    template<typename... Args>
    iterator emplace_hint(const_iterator /*hint*/, Args&&... args) {
        return emplace(zstl::forward<Args>(args)...).first;
    }

    iterator insert(const_iterator /*hint*/, const value_type& value) {
        return insert(value).first;
    }

    template<typename P>
    auto insert(const_iterator hint, P&& value)
        -> std::enable_if_t<std::is_constructible_v<value_type, P&&>, iterator> {
        return insert(hint, value_type(zstl::forward<P>(value)));
    }

    // Range erase
    iterator erase(const_iterator first, const_iterator last) {
        while (first != last) {
            first = erase(first);
        }
        return iterator(const_cast<value_type*>(&*last));
    }

    // ---- swap ----

    void swap(unordered_map& other) noexcept { table_.swap(other.table_); }
};

// ---- comparison operators ----

template<typename K, typename T, typename H, typename KE, typename A>
bool operator==(const unordered_map<K, T, H, KE, A>& a,
                const unordered_map<K, T, H, KE, A>& b) {
    if (a.size() != b.size()) return false;
    for (const auto& elem : a) {
        auto it = b.find(elem.first);
        if (it == b.end() || !(it->second == elem.second)) return false;
    }
    return true;
}

template<typename K, typename T, typename H, typename KE, typename A>
bool operator!=(const unordered_map<K, T, H, KE, A>& a,
                const unordered_map<K, T, H, KE, A>& b) {
    return !(a == b);
}

// ---- free function swap ----

template<typename K, typename T, typename H, typename KE, typename A>
void swap(unordered_map<K, T, H, KE, A>& a,
          unordered_map<K, T, H, KE, A>& b) noexcept(noexcept(a.swap(b))) {
    a.swap(b);
}

} // namespace zstl
