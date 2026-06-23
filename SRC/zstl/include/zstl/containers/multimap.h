// zstl multimap — ordered map with duplicate keys, backed by red-black tree
//
// A sorted associative container that contains key-value pairs.
// Multiple elements with the same key are allowed and are stored
// in stable insertion order.
//
// Template params:
//   Key     — key type
//   T       — mapped type
//   Compare — strict weak ordering on Key (default: less<Key>)
//   Alloc   — allocator
#pragma once

#include <initializer_list>

#include "zstl/containers/detail/rb_tree.h"
#include "zstl/memory/utility.h"

namespace zstl {

template<typename Key, typename T, typename Compare = less<Key>,
         typename Alloc = default_alloc<detail::rb_tree_node<Key, pair<const Key, T>>>>
class multimap {
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

    using tree_type = detail::rb_tree<Key, value_type, key_of_value, Compare, Alloc>;
    tree_type tree_;

public:
    using iterator               = typename tree_type::iterator;
    using const_iterator         = typename tree_type::const_iterator;
    using reverse_iterator       = typename tree_type::reverse_iterator;
    using const_reverse_iterator = typename tree_type::const_reverse_iterator;

    // ---- value_compare ----
    class value_compare {
        friend class multimap;
    protected:
        Compare comp;
        value_compare(Compare c) : comp(c) {}
    public:
        bool operator()(const value_type& a, const value_type& b) const {
            return comp(a.first, b.first);
        }
    };

    // ---- constructors ----

    multimap() = default;

    explicit multimap(const Compare& comp, const Alloc& alloc = Alloc())
        : tree_(comp, alloc) {}

    template<typename InputIt>
    multimap(InputIt first, InputIt last,
             const Compare& comp = Compare(),
             const Alloc& alloc = Alloc())
        : tree_(comp, alloc) {
        insert(first, last);
    }

    multimap(const multimap& other) = default;
    multimap(multimap&& other) noexcept = default;

    multimap(std::initializer_list<value_type> init,
             const Compare& comp = Compare(),
             const Alloc& alloc = Alloc())
        : tree_(comp, alloc) {
        insert(init.begin(), init.end());
    }

    multimap& operator=(const multimap& other) = default;
    multimap& operator=(multimap&& other) noexcept = default;

    multimap& operator=(std::initializer_list<value_type> ilist) {
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

    // ---- modifiers ----

    iterator insert(const value_type& value) {
        return tree_.insert_multi(value);
    }

    iterator insert(const_iterator hint, const value_type& value) {
        return tree_.insert_multi_hint(hint, value);
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
    iterator emplace(Args&&... args) {
        return tree_.emplace_multi(zstl::forward<Args>(args)...);
    }

    template<typename... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args) {
        return tree_.emplace_multi(zstl::forward<Args>(args)...);
    }

    // Erase
    iterator erase(const_iterator pos) {
        auto it = iterator(const_cast<detail::rb_tree_node_base*>(pos.node));
        auto next = zstl::next(it);
        tree_.erase(pos);
        return next;
    }

    iterator erase(const_iterator first, const_iterator last) {
        while (first != last) {
            first = erase(first);
        }
        return iterator(const_cast<detail::rb_tree_node_base*>(last.node));
    }

    size_type erase(const Key& key) {
        // Erase all elements with the given key
        size_type count = 0;
        auto it = find(key);
        while (it != end() && !tree_.key_comp()(key, (*it).first) && !tree_.key_comp()((*it).first, key)) {
            it = erase(it);
            ++count;
        }
        return count;
    }

    void clear() noexcept { tree_.clear(); }

    // ---- observers ----

    key_compare key_comp() const noexcept { return tree_.key_comp(); }
    value_compare value_comp() const noexcept { return value_compare(tree_.key_comp()); }

    // ---- lookup ----

    iterator find(const Key& key) { return tree_.find(key); }
    const_iterator find(const Key& key) const { return tree_.find(key); }

    size_type count(const Key& key) const {
        // For multimap, count can be > 1
        size_type n = 0;
        auto it = find(key);
        while (it != end() && !tree_.key_comp()(key, (*it).first) && !tree_.key_comp()((*it).first, key)) {
            ++n;
            ++it;
        }
        return n;
    }

    bool contains(const Key& key) const { return find(key) != end(); }

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

    template<typename C2>
    void merge(multimap<Key, T, C2, Alloc>& source) {
        auto it = source.begin();
        while (it != source.end()) {
            auto next = zstl::next(it);
            insert(*it);
            source.erase(it);
            it = next;
        }
    }

    // ---- swap ----

    void swap(multimap& other) noexcept { tree_.swap(other.tree_); }
};

// ---- comparison operators ----

template<typename K, typename T, typename C, typename A>
bool operator==(const multimap<K, T, C, A>& a, const multimap<K, T, C, A>& b) {
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
bool operator!=(const multimap<K, T, C, A>& a, const multimap<K, T, C, A>& b) {
    return !(a == b);
}

// ---- free function swap ----

template<typename K, typename T, typename C, typename A>
void swap(multimap<K, T, C, A>& a, multimap<K, T, C, A>& b) noexcept(noexcept(a.swap(b))) {
    a.swap(b);
}

} // namespace zstl
