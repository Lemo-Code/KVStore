// zstl skip_map — skip list backed ordered map
//
// A sorted associative container that contains key-value pairs with unique keys.
// Backed by a probabilistic skip list for O(log n) expected operations.
// Naturally balanced without rotations — ideal for concurrent access patterns.
// Also provides rank() and at_rank() for ZSet-style operations.
//
// Template params:
//   Key     — key type
//   T       — mapped type
//   Compare — strict weak ordering on Key (default: less<Key>)
//   Alloc   — allocator
#pragma once

#include <stdexcept>
#include <initializer_list>

#include "zstl/containers/detail/skip_list.h"
#include "zstl/memory/utility.h"

namespace zstl {

template<typename Key, typename T,
         typename Compare = less<Key>,
         typename Alloc = default_alloc<char>>
class skip_map {
public:
    // ---- type definitions ----
    using key_type        = Key;
    using mapped_type     = T;
    using value_type      = pair<const Key, T>;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;
    using key_compare     = Compare;
    using allocator_type  = Alloc;

    using reference       = value_type&;
    using const_reference = const value_type&;
    using pointer         = value_type*;
    using const_pointer   = const value_type*;

private:
    struct key_of_value {
        const Key& operator()(const value_type& v) const noexcept { return v.first; }
    };

    using list_type = detail::skip_list<Key, value_type, key_of_value, Compare, Alloc>;
    list_type list_;

public:
    using iterator       = typename list_type::iterator;
    using const_iterator = typename list_type::const_iterator;

    // ---- value_compare ----
    class value_compare {
        friend class skip_map;
    protected:
        Compare comp;
        value_compare(Compare c) : comp(c) {}
    public:
        bool operator()(const value_type& a, const value_type& b) const {
            return comp(a.first, b.first);
        }
    };

    // ---- constructors ----

    skip_map() = default;

    explicit skip_map(const Compare& comp) : list_(comp) {}

    template<typename InputIt>
    skip_map(InputIt first, InputIt last, const Compare& comp = Compare())
        : list_(comp) {
        insert(first, last);
    }

    skip_map(const skip_map& other) = default;
    skip_map(skip_map&& other) noexcept = default;

    skip_map(std::initializer_list<value_type> init,
             const Compare& comp = Compare())
        : list_(comp) {
        insert(init.begin(), init.end());
    }

    skip_map& operator=(const skip_map& other) = default;
    skip_map& operator=(skip_map&& other) noexcept = default;

    skip_map& operator=(std::initializer_list<value_type> ilist) {
        clear();
        insert(ilist.begin(), ilist.end());
        return *this;
    }

    // ---- iterators ----

    iterator begin() noexcept { return list_.begin(); }
    const_iterator begin() const noexcept { return list_.begin(); }
    const_iterator cbegin() const noexcept { return list_.cbegin(); }

    iterator end() noexcept { return list_.end(); }
    const_iterator end() const noexcept { return list_.end(); }
    const_iterator cend() const noexcept { return list_.cend(); }

    // ---- capacity ----

    bool empty() const noexcept { return list_.empty(); }
    size_type size() const noexcept { return list_.size(); }
    size_type max_size() const noexcept { return list_.max_size(); }

    // ---- element access ----

    T& operator[](const Key& key) {
        auto it = find(key);
        if (it != end()) return (*it).second;
        auto [new_it, inserted] = list_.insert_unique(value_type(key, T()));
        return (*new_it).second;
    }

    T& operator[](Key&& key) {
        auto it = find(key);
        if (it != end()) return (*it).second;
        auto result = list_.insert_unique(value_type(zstl::move(key), T()));
        return (*result.first).second;
    }

    T& at(const Key& key) {
        auto it = find(key);
        if (it == end()) throw std::out_of_range("zstl::skip_map::at: key not found");
        return (*it).second;
    }

    const T& at(const Key& key) const {
        auto it = find(key);
        if (it == end()) throw std::out_of_range("zstl::skip_map::at: key not found");
        return (*it).second;
    }

    // ---- modifiers ----

    pair<iterator, bool> insert(const value_type& value) {
        return list_.insert_unique(value);
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

    template<typename... Args>
    pair<iterator, bool> emplace(Args&&... args) {
        return list_.emplace_unique(zstl::forward<Args>(args)...);
    }

    template<typename... Args>
    pair<iterator, bool> try_emplace(const Key& k, Args&&... args) {
        auto it = find(k);
        if (it != end()) return {it, false};
        return emplace(piecewise_construct,
                       std::forward_as_tuple(k),
                       std::forward_as_tuple(zstl::forward<Args>(args)...));
    }

    template<typename M>
    pair<iterator, bool> insert_or_assign(const Key& k, M&& obj) {
        auto it = find(k);
        if (it != end()) {
            (*it).second = zstl::forward<M>(obj);
            return {it, false};
        }
        return insert(value_type(k, zstl::forward<M>(obj)));
    }

    // Insert with hint
    iterator insert(const_iterator /*hint*/, const value_type& value) {
        return insert(value).first;
    }

    // Emplace hint
    template<typename... Args>
    iterator emplace_hint(const_iterator /*hint*/, Args&&... args) {
        return emplace(zstl::forward<Args>(args)...).first;
    }

    // Erase
    size_type erase(const Key& key) { return list_.erase(key); }
    void erase(iterator pos) { list_.erase(pos); }
    void erase(const_iterator pos) { list_.erase(pos); }

    iterator erase(const_iterator first, const_iterator last) {
        while (first != last) {
            first = erase(first);
        }
        return iterator(const_cast<value_type*>(&*last));
    }

    void clear() noexcept { list_.clear(); }

    // ---- observers ----

    key_compare key_comp() const noexcept { return list_.key_comp(); }
    value_compare value_comp() const noexcept { return value_compare(list_.key_comp()); }

    // ---- lookup ----

    iterator find(const Key& key) { return list_.find(key); }
    const_iterator find(const Key& key) const { return list_.find(key); }

    size_type count(const Key& key) const { return list_.count(key); }
    bool contains(const Key& key) const { return list_.contains(key); }

    iterator lower_bound(const Key& key) { return list_.lower_bound(key); }
    const_iterator lower_bound(const Key& key) const { return list_.lower_bound(key); }

    iterator upper_bound(const Key& key) { return list_.upper_bound(key); }
    const_iterator upper_bound(const Key& key) const { return list_.upper_bound(key); }

    pair<iterator, iterator> equal_range(const Key& key) {
        return list_.equal_range(key);
    }

    pair<const_iterator, const_iterator> equal_range(const Key& key) const {
        return list_.equal_range(key);
    }

    // ---- ZSet-style rank operations ----

    size_type rank(const Key& key) const { return list_.rank(key); }

    value_type* at_rank(size_type r) {
        return reinterpret_cast<value_type*>(list_.at_rank(r));
    }

    const value_type* at_rank(size_type r) const {
        return reinterpret_cast<const value_type*>(list_.at_rank(r));
    }

    // ---- extreme element access ----

    value_type* first() { return reinterpret_cast<value_type*>(list_.first()); }
    const value_type* first() const { return reinterpret_cast<const value_type*>(list_.first()); }
    value_type* last()  { return reinterpret_cast<value_type*>(list_.last()); }
    const value_type* last() const { return reinterpret_cast<const value_type*>(list_.last()); }

    // ---- merge ----

    template<typename C2>
    void merge(skip_map<Key, T, C2, Alloc>& source) {
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

    // ---- swap ----

    void swap(skip_map& other) noexcept { list_.swap(other.list_); }
};

// ---- comparison operators ----

template<typename K, typename T, typename C, typename A>
bool operator==(const skip_map<K, T, C, A>& a, const skip_map<K, T, C, A>& b) {
    if (a.size() != b.size()) return false;
    auto ait = a.begin();
    auto bit = b.begin();
    while (ait != a.end()) {
        if (!(*ait == *bit)) return false;
        ++ait; ++bit;
    }
    return true;
}

template<typename K, typename T, typename C, typename A>
bool operator!=(const skip_map<K, T, C, A>& a, const skip_map<K, T, C, A>& b) {
    return !(a == b);
}

template<typename K, typename T, typename C, typename A>
bool operator<(const skip_map<K, T, C, A>& a, const skip_map<K, T, C, A>& b) {
    auto ait = a.begin(), aend = a.end();
    auto bit = b.begin(), bend = b.end();
    while (ait != aend && bit != bend) {
        if (*ait < *bit) return true;
        if (*bit < *ait) return false;
        ++ait; ++bit;
    }
    return ait == aend && bit != bend;
}

template<typename K, typename T, typename C, typename A>
bool operator<=(const skip_map<K, T, C, A>& a, const skip_map<K, T, C, A>& b) {
    return !(b < a);
}

template<typename K, typename T, typename C, typename A>
bool operator>(const skip_map<K, T, C, A>& a, const skip_map<K, T, C, A>& b) {
    return b < a;
}

template<typename K, typename T, typename C, typename A>
bool operator>=(const skip_map<K, T, C, A>& a, const skip_map<K, T, C, A>& b) {
    return !(a < b);
}

// ---- free function swap ----

template<typename K, typename T, typename C, typename A>
void swap(skip_map<K, T, C, A>& a, skip_map<K, T, C, A>& b) noexcept(noexcept(a.swap(b))) {
    a.swap(b);
}

} // namespace zstl
