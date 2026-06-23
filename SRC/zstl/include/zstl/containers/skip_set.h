// zstl skip_set — skip list backed ordered set
//
// A sorted associative container that contains a set of unique keys.
// Backed by a probabilistic skip list for O(log n) expected operations.
// Provides rank() and at_rank() for ZSet-style positional queries.
//
// Template params:
//   Key     — key type (also serves as value_type)
//   Compare — strict weak ordering on Key (default: less<Key>)
//   Alloc   — allocator
#pragma once

#include <initializer_list>

#include "zstl/containers/detail/skip_list.h"
#include "zstl/memory/utility.h"

namespace zstl {

template<typename Key,
         typename Compare = less<Key>,
         typename Alloc = default_alloc<char>>
class skip_set {
public:
    // ---- type definitions ----
    using key_type        = Key;
    using value_type      = Key;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;
    using key_compare     = Compare;
    using value_compare   = Compare;
    using allocator_type  = Alloc;

    using reference       = value_type&;
    using const_reference = const value_type&;
    using pointer         = value_type*;
    using const_pointer   = const value_type*;

private:
    struct key_of_value {
        const Key& operator()(const Key& k) const noexcept { return k; }
    };

    using list_type = detail::skip_list<Key, value_type, key_of_value, Compare, Alloc>;
    list_type list_;

public:
    using iterator       = typename list_type::iterator;
    using const_iterator = typename list_type::const_iterator;

    // ---- constructors ----

    skip_set() = default;

    explicit skip_set(const Compare& comp) : list_(comp) {}

    template<typename InputIt>
    skip_set(InputIt first, InputIt last, const Compare& comp = Compare())
        : list_(comp) {
        insert(first, last);
    }

    skip_set(const skip_set& other) = default;
    skip_set(skip_set&& other) noexcept = default;

    skip_set(std::initializer_list<value_type> init,
             const Compare& comp = Compare())
        : list_(comp) {
        insert(init.begin(), init.end());
    }

    skip_set& operator=(const skip_set& other) = default;
    skip_set& operator=(skip_set&& other) noexcept = default;

    skip_set& operator=(std::initializer_list<value_type> ilist) {
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

    // ---- modifiers ----

    pair<iterator, bool> insert(const value_type& value) {
        return list_.insert_unique(value);
    }

    template<typename P>
    auto insert(P&& value)
        -> std::enable_if_t<std::is_constructible_v<value_type, P&&>,
                            pair<iterator, bool>> {
        return list_.insert_unique(value_type(zstl::forward<P>(value)));
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
    value_compare value_comp() const noexcept { return key_comp(); }

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

    Key* at_rank(size_type r) {
        return reinterpret_cast<Key*>(list_.at_rank(r));
    }

    const Key* at_rank(size_type r) const {
        return reinterpret_cast<const Key*>(list_.at_rank(r));
    }

    // ---- extreme element access ----

    Key* first() { return reinterpret_cast<Key*>(list_.first()); }
    const Key* first() const { return reinterpret_cast<const Key*>(list_.first()); }
    Key* last()  { return reinterpret_cast<Key*>(list_.last()); }
    const Key* last() const { return reinterpret_cast<const Key*>(list_.last()); }

    // Range insert/erase helpers for multi-value operations

    template<typename InputIt>
    void insert_range(InputIt first, InputIt last) {
        insert(first, last);
    }

    size_type erase_range(const Key& first_key, const Key& last_key) {
        size_type count = 0;
        auto it = lower_bound(first_key);
        auto last = lower_bound(last_key);
        while (it != last) {
            it = erase(it);
            ++count;
        }
        return count;
    }

    // Conditional insert for value management
    template<typename Predicate>
    void erase_if(Predicate pred) {
        auto it = begin();
        while (it != end()) {
            if (pred(*it)) {
                it = erase(it);
            } else {
                ++it;
            }
        }
    }

    // ---- merge ----

    template<typename C2>
    void merge(skip_set<Key, C2, Alloc>& source) {
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

    void swap(skip_set& other) noexcept { list_.swap(other.list_); }
};

// ---- comparison operators ----

template<typename K, typename C, typename A>
bool operator==(const skip_set<K, C, A>& a, const skip_set<K, C, A>& b) {
    if (a.size() != b.size()) return false;
    auto ait = a.begin();
    auto bit = b.begin();
    while (ait != a.end()) {
        if (!(*ait == *bit)) return false;
        ++ait; ++bit;
    }
    return true;
}

template<typename K, typename C, typename A>
bool operator!=(const skip_set<K, C, A>& a, const skip_set<K, C, A>& b) {
    return !(a == b);
}

template<typename K, typename C, typename A>
bool operator<(const skip_set<K, C, A>& a, const skip_set<K, C, A>& b) {
    auto ait = a.begin(), aend = a.end();
    auto bit = b.begin(), bend = b.end();
    while (ait != aend && bit != bend) {
        if (*ait < *bit) return true;
        if (*bit < *ait) return false;
        ++ait; ++bit;
    }
    return ait == aend && bit != bend;
}

template<typename K, typename C, typename A>
bool operator<=(const skip_set<K, C, A>& a, const skip_set<K, C, A>& b) {
    return !(b < a);
}

template<typename K, typename C, typename A>
bool operator>(const skip_set<K, C, A>& a, const skip_set<K, C, A>& b) {
    return b < a;
}

template<typename K, typename C, typename A>
bool operator>=(const skip_set<K, C, A>& a, const skip_set<K, C, A>& b) {
    return !(a < b);
}

// ---- free function swap ----

template<typename K, typename C, typename A>
void swap(skip_set<K, C, A>& a, skip_set<K, C, A>& b) noexcept(noexcept(a.swap(b))) {
    a.swap(b);
}

} // namespace zstl
