// zstl bmap — ordered map backed by B+ tree
//
// A sorted associative container that contains key-value pairs with unique keys.
// Backed by a B+ tree for cache-friendly range scans and logarithmic operations.
// B+ tree leaves form a linked list enabling O(1) inter-leaf iteration,
// making range queries and full scans significantly faster than RB-tree.
//
// Key differences from std::map:
//   - B+ tree backing (better cache locality, range-scan optimized)
//   - Configurable branching factor (default 64)
//   - Leaf-linked-list iteration for O(1) next-leaf access
//
// Template params:
//   Key     — key type
//   T       — mapped type
//   Compare — strict weak ordering on Key (default: less<Key>)
//   Alloc   — allocator
//   BFactor — branching factor (default: 64, must be >= 4)
#pragma once

#include <stdexcept>
#include <initializer_list>

#include "zstl/containers/detail/bplus_tree.h"
#include "zstl/memory/utility.h"
#include "zstl/memory/construct.h"

namespace zstl {

template<typename Key, typename T,
         typename Compare = less<Key>,
         typename Alloc = default_alloc<char>,
         size_t BFactor = 64>
class bmap {
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

    // B+ tree branching factor
    static constexpr size_t kBranchFactor = BFactor >= 4 ? BFactor : 4;

private:
    struct key_of_value {
        const Key& operator()(const value_type& v) const noexcept { return v.first; }
    };

    using tree_type = detail::bplus_tree<Key, value_type, key_of_value, Compare, BFactor, Alloc>;
    tree_type tree_;

public:
    using iterator               = typename tree_type::iterator;
    using const_iterator         = typename tree_type::const_iterator;
    using reverse_iterator       = typename tree_type::reverse_iterator;
    using const_reverse_iterator = typename tree_type::const_reverse_iterator;

    // ---- value_compare ----
    class value_compare {
        friend class bmap;
    protected:
        Compare comp;
        value_compare(Compare c) : comp(c) {}
    public:
        bool operator()(const value_type& a, const value_type& b) const {
            return comp(a.first, b.first);
        }
    };

    // ---- constructors ----

    bmap() = default;

    explicit bmap(const Compare& comp, const Alloc& alloc = Alloc())
        : tree_(comp, alloc) {}

    template<typename InputIt>
    bmap(InputIt first, InputIt last,
         const Compare& comp = Compare(),
         const Alloc& alloc = Alloc())
        : tree_(comp, alloc) {
        insert(first, last);
    }

    bmap(const bmap& other) = default;
    bmap(bmap&& other) noexcept = default;

    bmap(std::initializer_list<value_type> init,
         const Compare& comp = Compare(),
         const Alloc& alloc = Alloc())
        : tree_(comp, alloc) {
        insert(init.begin(), init.end());
    }

    bmap& operator=(const bmap& other) = default;
    bmap& operator=(bmap&& other) noexcept = default;

    bmap& operator=(std::initializer_list<value_type> ilist) {
        clear();
        insert(ilist.begin(), ilist.end());
        return *this;
    }

    // ---- allocator ----

    allocator_type get_allocator() const noexcept { return tree_.get_allocator(); }

    // ---- iterators ----

    iterator begin() noexcept { return tree_.begin(); }
    const_iterator begin() const noexcept { return tree_.begin(); }
    const_iterator cbegin() const noexcept { return tree_.cbegin(); }

    iterator end() noexcept { return tree_.end(); }
    const_iterator end() const noexcept { return tree_.end(); }
    const_iterator cend() const noexcept { return tree_.cend(); }

    reverse_iterator rbegin() noexcept { return tree_.rbegin(); }
    const_reverse_iterator rbegin() const noexcept { return tree_.rbegin(); }
    const_reverse_iterator crbegin() const noexcept { return tree_.crbegin(); }

    reverse_iterator rend() noexcept { return tree_.rend(); }
    const_reverse_iterator rend() const noexcept { return tree_.rend(); }
    const_reverse_iterator crend() const noexcept { return tree_.crend(); }

    // ---- capacity ----

    bool empty() const noexcept { return tree_.empty(); }
    size_type size() const noexcept { return tree_.size(); }
    size_type max_size() const noexcept { return tree_.max_size(); }

    // ---- element access ----

    T& operator[](const Key& key) {
        auto it = tree_.find(key);
        if (it != end()) {
            return (*it).second;
        }
        auto result = tree_.insert_unique(value_type(key, T()));
        return (*result.first).second;
    }

    T& operator[](Key&& key) {
        auto it = tree_.find(key);
        if (it != end()) {
            return (*it).second;
        }
        auto result = tree_.insert_unique(value_type(zstl::move(key), T()));
        return (*result.first).second;
    }

    T& at(const Key& key) {
        auto it = tree_.find(key);
        if (it == end()) throw std::out_of_range("zstl::bmap::at: key not found");
        return (*it).second;
    }

    const T& at(const Key& key) const {
        auto it = tree_.find(key);
        if (it == end()) throw std::out_of_range("zstl::bmap::at: key not found");
        return (*it).second;
    }

    // ---- modifiers ----

    pair<iterator, bool> insert(const value_type& value) {
        return tree_.insert_unique(value);
    }

    template<typename P>
    auto insert(P&& value)
        -> std::enable_if_t<std::is_constructible_v<value_type, P&&>,
                            pair<iterator, bool>> {
        return tree_.insert_unique(value_type(zstl::forward<P>(value)));
    }

    iterator insert(const_iterator hint, const value_type& value) {
        return tree_.insert_unique(value).first;
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
        return tree_.emplace_unique(zstl::forward<Args>(args)...);
    }

    template<typename... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args) {
        return tree_.emplace_unique(zstl::forward<Args>(args)...).first;
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
        auto next = pos;
        ++next;
        tree_.erase(pos);
        return next;
    }

    iterator erase(const_iterator first, const_iterator last) {
        while (first != last) {
            first = erase(first);
        }
        return iterator(const_cast<value_type*>(&*last));
    }

    size_type erase(const Key& key) {
        return tree_.erase(key);
    }

    void clear() noexcept { tree_.clear(); }

    // ---- observers ----

    key_compare key_comp() const noexcept { return tree_.key_comp(); }
    value_compare value_comp() const noexcept { return value_compare(tree_.key_comp()); }

    // ---- lookup ----

    iterator find(const Key& key) { return tree_.find(key); }
    const_iterator find(const Key& key) const { return tree_.find(key); }

    size_type count(const Key& key) const { return tree_.count(key); }
    bool contains(const Key& key) const { return tree_.contains(key); }

    iterator lower_bound(const Key& key) { return tree_.lower_bound(key); }
    const_iterator lower_bound(const Key& key) const { return tree_.lower_bound(key); }

    iterator upper_bound(const Key& key) { return tree_.upper_bound(key); }
    const_iterator upper_bound(const Key& key) const { return tree_.upper_bound(key); }

    pair<iterator, iterator> equal_range(const Key& key) {
        return tree_.equal_range(key);
    }

    pair<const_iterator, const_iterator> equal_range(const Key& key) const {
        return tree_.equal_range(key);
    }

    // ---- merge ----

    template<typename C2, size_t BF2>
    void merge(bmap<Key, T, C2, Alloc, BF2>& source) {
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

    void swap(bmap& other) noexcept { tree_.swap(other.tree_); }
};

// ---- comparison operators ----

template<typename K, typename T, typename C, typename A, size_t BF>
bool operator==(const bmap<K, T, C, A, BF>& a, const bmap<K, T, C, A, BF>& b) {
    if (a.size() != b.size()) return false;
    auto ait = a.begin();
    auto bit = b.begin();
    while (ait != a.end()) {
        if (!(*ait == *bit)) return false;
        ++ait; ++bit;
    }
    return true;
}

template<typename K, typename T, typename C, typename A, size_t BF>
bool operator!=(const bmap<K, T, C, A, BF>& a, const bmap<K, T, C, A, BF>& b) {
    return !(a == b);
}

template<typename K, typename T, typename C, typename A, size_t BF>
bool operator<(const bmap<K, T, C, A, BF>& a, const bmap<K, T, C, A, BF>& b) {
    auto ait = a.begin(), aend = a.end();
    auto bit = b.begin(), bend = b.end();
    while (ait != aend && bit != bend) {
        if (*ait < *bit) return true;
        if (*bit < *ait) return false;
        ++ait; ++bit;
    }
    return ait == aend && bit != bend;
}

template<typename K, typename T, typename C, typename A, size_t BF>
bool operator<=(const bmap<K, T, C, A, BF>& a, const bmap<K, T, C, A, BF>& b) {
    return !(b < a);
}

template<typename K, typename T, typename C, typename A, size_t BF>
bool operator>(const bmap<K, T, C, A, BF>& a, const bmap<K, T, C, A, BF>& b) {
    return b < a;
}

template<typename K, typename T, typename C, typename A, size_t BF>
bool operator>=(const bmap<K, T, C, A, BF>& a, const bmap<K, T, C, A, BF>& b) {
    return !(a < b);
}

// ---- free function swap ----

template<typename K, typename T, typename C, typename A, size_t BF>
void swap(bmap<K, T, C, A, BF>& a,
          bmap<K, T, C, A, BF>& b) noexcept(noexcept(a.swap(b))) {
    a.swap(b);
}

} // namespace zstl
