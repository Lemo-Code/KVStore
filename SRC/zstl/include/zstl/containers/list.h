// zstl list — doubly-linked circular list with sentinel node
//
// Implementation:
//   - Sentinel node anchors a circular doubly-linked list.
//   - empty() is checked via sentinel_.next == &sentinel_ (or size_ == 0).
//   - O(1) push/pop at both ends, O(1) splice/insert/erase at known position.
//   - O(1) size tracking via size_ member.
//   - Sort uses bottom-up merge sort: O(n log n) comparisons, O(log n) auxiliary
//     storage, stable.
//
// Iterator invalidation:
//   - Insert never invalidates iterators.
//   - Erase invalidates only iterators to the erased element.
//   - Splice invalidates iterators to the spliced elements (they now belong
//     to the destination list).
#pragma once

#include <cstddef>
#include <initializer_list>
#include "zstl/memory/allocator.h"
#include "zstl/memory/construct.h"
#include "zstl/memory/utility.h"
#include "zstl/containers/detail/list_node.h"
#include "zstl/containers/detail/list_iterator.h"

namespace zstl {

template<typename T, typename Alloc = default_alloc<detail::list_node<T>>>
class list {
public:
    // ---- types ----
    using value_type      = T;
    using allocator_type  = Alloc;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;
    using reference       = T&;
    using const_reference = const T&;
    using pointer         = T*;
    using const_pointer   = const T*;

    using iterator       = detail::list_iterator<T>;
    using const_iterator = detail::list_iterator<const T>;

    using reverse_iterator       = zstl::reverse_iterator<iterator>;
    using const_reverse_iterator = zstl::reverse_iterator<const_iterator>;

    using node_type  = detail::list_node<T>;
    using base_type  = detail::list_node_base;

private:
    base_type sentinel_;           // dummy node, anchors the circular list
    size_type size_ = 0;
    [[no_unique_address]] allocator_type alloc_;

    // ---- node allocation helpers ----
    node_type* create_node() {
        node_type* node = std::allocator_traits<Alloc>::allocate(alloc_, 1);
        construct(node);
        return node;
    }

    template<typename... Args>
    node_type* create_node_emplace(Args&&... args) {
        node_type* node = std::allocator_traits<Alloc>::allocate(alloc_, 1);
        construct(node, zstl::forward<Args>(args)...);
        return node;
    }

    void destroy_node(node_type* node) noexcept {
        destroy(node);
        std::allocator_traits<Alloc>::deallocate(alloc_, node, 1);
    }

    // Initialize empty sentinel
    void init_sentinel() noexcept {
        sentinel_.next = &sentinel_;
        sentinel_.prev = &sentinel_;
    }

public:
    // ============================================================
    // Constructors
    // ============================================================
    list() noexcept { init_sentinel(); }

    explicit list(size_type count, const T& value = T()) : list() {
        for (size_type i = 0; i < count; ++i) {
            push_back(value);
        }
    }

    template<typename InputIterator,
             typename = decltype(*std::declval<InputIterator&>())>
    list(InputIterator first, InputIterator last) : list() {
        for (; first != last; ++first) {
            push_back(*first);
        }
    }

    list(const list& other) : list() {
        for (const auto& v : other) {
            push_back(v);
        }
    }

    list(list&& other) noexcept : list() {
        swap(other);
    }

    list(std::initializer_list<T> il) : list() {
        for (const auto& v : il) {
            push_back(v);
        }
    }

    ~list() { clear(); }

    // ============================================================
    // Assignment
    // ============================================================
    list& operator=(const list& other) {
        if (this != &other) {
            // Strong exception guarantee via copy-swap
            list tmp(other);
            swap(tmp);
        }
        return *this;
    }

    list& operator=(list&& other) noexcept {
        if (this != &other) {
            clear();
            swap(other);
        }
        return *this;
    }

    list& operator=(std::initializer_list<T> il) {
        assign(il);
        return *this;
    }

    // ============================================================
    // assign
    // ============================================================
    void assign(size_type count, const T& value) {
        clear();
        for (size_type i = 0; i < count; ++i) {
            push_back(value);
        }
    }

    template<typename InputIterator,
             typename = decltype(*std::declval<InputIterator&>())>
    void assign(InputIterator first, InputIterator last) {
        clear();
        for (; first != last; ++first) {
            push_back(*first);
        }
    }

    void assign(std::initializer_list<T> il) {
        clear();
        for (const auto& v : il) {
            push_back(v);
        }
    }

    // ============================================================
    // Element access
    // ============================================================
    reference front() noexcept { return node_type::value(sentinel_.next); }
    const_reference front() const noexcept { return node_type::value(sentinel_.next); }
    reference back() noexcept { return node_type::value(sentinel_.prev); }
    const_reference back() const noexcept { return node_type::value(sentinel_.prev); }

    // ============================================================
    // Iterators
    // ============================================================
    iterator begin() noexcept { return iterator(sentinel_.next); }
    const_iterator begin() const noexcept { return const_iterator(const_cast<base_type*>(sentinel_.next)); }
    const_iterator cbegin() const noexcept { return const_iterator(const_cast<base_type*>(sentinel_.next)); }
    iterator end() noexcept { return iterator(&sentinel_); }
    const_iterator end() const noexcept { return const_iterator(const_cast<base_type*>(&sentinel_)); }
    const_iterator cend() const noexcept { return const_iterator(const_cast<base_type*>(&sentinel_)); }

    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

    // ============================================================
    // Capacity
    // ============================================================
    bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }
    size_type max_size() const noexcept { return static_cast<size_type>(-1) / sizeof(node_type); }

    // ============================================================
    // push_front / push_back
    // ============================================================
    void push_front(const T& value) {
        node_type* node = create_node_emplace(value);
        node->hook(sentinel_.next);
        ++size_;
    }

    void push_front(T&& value) {
        node_type* node = create_node_emplace(zstl::move(value));
        node->hook(sentinel_.next);
        ++size_;
    }

    void push_back(const T& value) {
        node_type* node = create_node_emplace(value);
        node->hook(&sentinel_);
        ++size_;
    }

    void push_back(T&& value) {
        node_type* node = create_node_emplace(zstl::move(value));
        node->hook(&sentinel_);
        ++size_;
    }

    // ============================================================
    // emplace_front / emplace_back
    // ============================================================
    template<typename... Args>
    reference emplace_front(Args&&... args) {
        node_type* node = create_node_emplace(zstl::forward<Args>(args)...);
        node->hook(sentinel_.next);
        ++size_;
        return node->data;
    }

    template<typename... Args>
    reference emplace_back(Args&&... args) {
        node_type* node = create_node_emplace(zstl::forward<Args>(args)...);
        node->hook(&sentinel_);
        ++size_;
        return node->data;
    }

    // ============================================================
    // pop_front / pop_back
    // ============================================================
    void pop_front() {
        base_type* node = sentinel_.next;
        node->unhook();
        destroy_node(static_cast<node_type*>(node));
        --size_;
    }

    void pop_back() {
        base_type* node = sentinel_.prev;
        node->unhook();
        destroy_node(static_cast<node_type*>(node));
        --size_;
    }

    // ============================================================
    // insert — insert value before pos
    // ============================================================
    iterator insert(const_iterator pos, const T& value) {
        return emplace(pos, value);
    }

    iterator insert(const_iterator pos, T&& value) {
        return emplace(pos, zstl::move(value));
    }

    iterator insert(const_iterator pos, size_type count, const T& value) {
        if (count == 0) return iterator(const_cast<base_type*>(pos.base()));
        iterator first_inserted = emplace(pos, value);
        for (size_type i = 1; i < count; ++i) {
            emplace(pos, value);
        }
        return first_inserted;
    }

    template<typename InputIterator,
             typename = decltype(*std::declval<InputIterator&>())>
    iterator insert(const_iterator pos, InputIterator first, InputIterator last) {
        if (first == last) return iterator(const_cast<base_type*>(pos.base()));
        iterator first_inserted = emplace(pos, *first);
        ++first;
        for (; first != last; ++first) {
            emplace(pos, *first);
        }
        return first_inserted;
    }

    iterator insert(const_iterator pos, std::initializer_list<T> il) {
        return insert(pos, il.begin(), il.end());
    }

    // ============================================================
    // emplace — construct element before pos
    // ============================================================
    template<typename... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        node_type* node = create_node_emplace(zstl::forward<Args>(args)...);
        node->hook(const_cast<base_type*>(pos.base()));
        ++size_;
        return iterator(static_cast<base_type*>(node));
    }

    // ============================================================
    // erase — erase element at pos, return next iterator
    // ============================================================
    iterator erase(const_iterator pos) {
        base_type* node = const_cast<base_type*>(pos.base());
        iterator next(node->next);
        node->unhook();
        destroy_node(static_cast<node_type*>(node));
        --size_;
        return next;
    }

    iterator erase(const_iterator first, const_iterator last) {
        while (first != last) {
            first = erase(first);
        }
        return iterator(const_cast<base_type*>(last.base()));
    }

    // ============================================================
    // resize
    // ============================================================
    void resize(size_type count) {
        resize(count, T());
    }

    void resize(size_type count, const T& value) {
        if (count < size_) {
            // Erase extra elements from the end
            while (size_ > count) {
                pop_back();
            }
        } else if (count > size_) {
            while (size_ < count) {
                push_back(value);
            }
        }
    }

    // ============================================================
    // clear — destroy all elements
    // ============================================================
    void clear() noexcept {
        base_type* cur = sentinel_.next;
        while (cur != &sentinel_) {
            base_type* next = cur->next;
            destroy_node(static_cast<node_type*>(cur));
            cur = next;
        }
        init_sentinel();
        size_ = 0;
    }

    // ============================================================
    // swap
    // ============================================================
    void swap(list& other) noexcept {
        if (size_ == 0 && other.size_ == 0) return;

        if (size_ == 0) {
            // Move all from other to this
            base_type::transfer(other.sentinel_.next, &other.sentinel_, &sentinel_);
            size_ = other.size_;
            other.init_sentinel();
            other.size_ = 0;
        } else if (other.size_ == 0) {
            // Move all from this to other
            base_type::transfer(sentinel_.next, &sentinel_, &other.sentinel_);
            other.size_ = size_;
            init_sentinel();
            size_ = 0;
        } else {
            // Both non-empty: swap via transfer
            // Transfer other's nodes to this before end()
            base_type::transfer(other.sentinel_.next, &other.sentinel_, &sentinel_);
            // Transfer our old nodes to other before other's end()
            base_type::transfer(sentinel_.next, &sentinel_, &other.sentinel_);
            zstl::swap(size_, other.size_);
        }
    }

    // ============================================================
    // splice — transfer elements from other list
    // ============================================================

    // Splice all elements from other before pos
    void splice(const_iterator pos, list& other) {
        if (other.empty()) return;
        base_type::transfer(other.sentinel_.next, &other.sentinel_,
                            const_cast<base_type*>(pos.base()));
        size_ += other.size_;
        other.init_sentinel();
        other.size_ = 0;
    }

    void splice(const_iterator pos, list&& other) {
        splice(pos, other);
    }

    // Splice single element from other before pos
    void splice(const_iterator pos, list& other, const_iterator it) {
        base_type* node = const_cast<base_type*>(it.base());
        if (node == pos.base() || node == pos.base()->next) return;
        node->unhook();
        node->hook(const_cast<base_type*>(pos.base()));
        --other.size_;
        ++size_;
    }

    void splice(const_iterator pos, list&& other, const_iterator it) {
        splice(pos, other, it);
    }

    // Splice range [first, last) from other before pos
    void splice(const_iterator pos, list& other,
                const_iterator first, const_iterator last) {
        if (first == last) return;
        base_type* f = const_cast<base_type*>(first.base());
        base_type* l = const_cast<base_type*>(last.base());
        size_type n = 0;
        for (base_type* cur = f; cur != l; cur = cur->next) ++n;
        base_type::transfer(f, l, const_cast<base_type*>(pos.base()));
        other.size_ -= n;
        size_ += n;
    }

    void splice(const_iterator pos, list&& other,
                const_iterator first, const_iterator last) {
        splice(pos, other, first, last);
    }

    // ============================================================
    // remove / remove_if
    // ============================================================
    void remove(const T& value) {
        remove_if([&value](const T& v) { return v == value; });
    }

    template<typename Predicate>
    void remove_if(Predicate pred) {
        base_type* cur = sentinel_.next;
        while (cur != &sentinel_) {
            base_type* next = cur->next;
            if (pred(node_type::value(cur))) {
                cur->unhook();
                destroy_node(static_cast<node_type*>(cur));
                --size_;
            }
            cur = next;
        }
    }

    // ============================================================
    // unique — remove consecutive duplicates
    // ============================================================
    void unique() {
        unique(equal_to<void>());
    }

    template<typename BinaryPredicate>
    void unique(BinaryPredicate pred) {
        if (size_ <= 1) return;
        base_type* cur = sentinel_.next->next;
        while (cur != &sentinel_) {
            base_type* next = cur->next;
            if (pred(node_type::value(cur->prev), node_type::value(cur))) {
                cur->unhook();
                destroy_node(static_cast<node_type*>(cur));
                --size_;
            }
            cur = next;
        }
    }

    // ============================================================
    // merge — merge two sorted lists
    // Both lists must be sorted according to comp.
    // ============================================================
    void merge(list& other) {
        merge(other, less<void>());
    }

    void merge(list&& other) {
        merge(other);
    }

    template<typename Compare>
    void merge(list& other, Compare comp) {
        if (this == &other || other.empty()) return;
        if (empty()) {
            swap(other);
            return;
        }

        base_type* cur1 = sentinel_.next;
        base_type* cur2 = other.sentinel_.next;
        base_type* const end1 = &sentinel_;
        base_type* const end2 = &other.sentinel_;

        while (cur1 != end1 && cur2 != end2) {
            if (comp(node_type::value(cur2), node_type::value(cur1))) {
                // Move cur2 before cur1
                base_type* next2 = cur2->next;
                cur2->unhook();
                cur2->hook(cur1);
                --other.size_;
                ++size_;
                cur2 = next2;
            } else {
                cur1 = cur1->next;
            }
        }

        // Transfer remaining elements from other
        if (cur2 != end2) {
            base_type::transfer(cur2, end2, end1);
            size_ += other.size_;
            other.init_sentinel();
            other.size_ = 0;
        }
    }

    template<typename Compare>
    void merge(list&& other, Compare comp) {
        merge(other, comp);
    }

    // ============================================================
    // sort — bottom-up merge sort
    // O(n log n) comparisons, O(log n) auxiliary storage, stable.
    // ============================================================
    void sort() {
        sort(less<void>());
    }

    template<typename Compare>
    void sort(Compare comp) {
        if (size_ <= 1) return;

        // Bottom-up merge sort using an array of lists as merge bins
        // Bin i holds a sorted list of up to 2^i elements.
        list carry;
        list bin[64];  // enough for > 2^64 elements
        int fill = 0;

        while (!empty()) {
            // Take one element from this list
            base_type* node = sentinel_.next;
            node->unhook();
            --size_;
            carry.sentinel_.next = node;
            carry.sentinel_.prev = node;
            node->next = &carry.sentinel_;
            node->prev = &carry.sentinel_;
            carry.size_ = 1;

            int i = 0;
            while (i < fill && !bin[i].empty()) {
                bin[i].merge(carry, comp);
                carry.swap(bin[i]);
                ++i;
            }
            carry.swap(bin[i]);
            if (i == fill) ++fill;
        }

        // Merge all bins together
        for (int i = 1; i < fill; ++i) {
            bin[i].merge(bin[i - 1], comp);
        }
        swap(bin[fill - 1]);
    }

    // ============================================================
    // reverse
    // ============================================================
    void reverse() noexcept {
        if (size_ <= 1) return;
        base_type::reverse(&sentinel_);
    }

    // ============================================================
    // Comparison operators
    // ============================================================
    friend bool operator==(const list& a, const list& b) {
        if (a.size_ != b.size_) return false;
        auto ia = a.begin();
        auto ib = b.begin();
        for (; ia != a.end(); ++ia, ++ib) {
            if (!(*ia == *ib)) return false;
        }
        return true;
    }

    friend bool operator!=(const list& a, const list& b) {
        return !(a == b);
    }

    friend bool operator<(const list& a, const list& b) {
        auto ia = a.begin();
        auto ib = b.begin();
        for (; ia != a.end() && ib != b.end(); ++ia, ++ib) {
            if (*ia < *ib) return true;
            if (*ib < *ia) return false;
        }
        return a.size_ < b.size_;
    }

    friend bool operator>(const list& a, const list& b) {
        return b < a;
    }

    friend bool operator<=(const list& a, const list& b) {
        return !(b < a);
    }

    friend bool operator>=(const list& a, const list& b) {
        return !(a < b);
    }
};

// ============================================================
// Non-member swap
// ============================================================
template<typename T, typename Alloc>
void swap(list<T, Alloc>& a, list<T, Alloc>& b) noexcept {
    a.swap(b);
}

} // namespace zstl
