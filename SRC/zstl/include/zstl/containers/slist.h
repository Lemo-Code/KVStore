// zstl slist — singly-linked list (forward_list equivalent)
//
// Singly-linked list with forward-only iteration. Uses a sentinel head node
// (before_begin) to simplify insert_after / erase_after operations.
//
// Complexity:
//   - push_front, pop_front, insert_after, erase_after: O(1)
//   - size(): O(1) (tracked member)
//   - sort(): O(n log n) using bottom-up merge sort (same as list)
//   - reverse(): O(n)
//
// Iterator invalidation:
//   - insert_after never invalidates any iterators.
//   - erase_after invalidates only iterators to the erased element.
//   - Operations on the list do not affect iterators to other elements.
#pragma once

#include <cstddef>
#include <initializer_list>
#include "zstl/memory/allocator.h"
#include "zstl/memory/construct.h"
#include "zstl/memory/utility.h"
#include "zstl/containers/detail/slist_node.h"

namespace zstl {

// ============================================================
// slist_iterator — forward iterator for slist
// ============================================================
template<typename T, typename Ref, typename Ptr>
struct slist_iterator {
    using iterator_category = forward_iterator_tag;
    using value_type        = T;
    using difference_type   = ptrdiff_t;
    using pointer           = Ptr;
    using reference         = Ref;
    using node_type         = detail::slist_node_base;

    node_type* node;

    slist_iterator() noexcept : node(nullptr) {}
    explicit slist_iterator(node_type* n) noexcept : node(n) {}

    // Converting: iterator -> const_iterator
    template<typename U,
             typename = enable_if_t<std::is_convertible<U*, Ptr>::value>>
    slist_iterator(const slist_iterator<U, U&, U*>& other) noexcept
        : node(other.node) {}

    reference operator*() const noexcept {
        return detail::slist_node<T>::value(node);
    }
    pointer operator->() const noexcept {
        return &(operator*());
    }

    slist_iterator& operator++() noexcept {
        node = node->next;
        return *this;
    }
    slist_iterator operator++(int) noexcept {
        slist_iterator tmp = *this;
        node = node->next;
        return tmp;
    }

    bool operator==(const slist_iterator& o) const noexcept { return node == o.node; }
    bool operator!=(const slist_iterator& o) const noexcept { return node != o.node; }
};

template<typename T, typename Alloc = default_alloc<detail::slist_node<T>>>
class slist {
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
    using iterator        = slist_iterator<T, T&, T*>;
    using const_iterator  = slist_iterator<T, const T&, const T*>;
    using node_type       = detail::slist_node<T>;
    using base_type       = detail::slist_node_base;

private:
    base_type head_;        // Sentinal head (before_begin)
    size_type size_ = 0;
    [[no_unique_address]] allocator_type alloc_;

    // ---- helpers ----
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

public:
    // ============================================================
    // Constructors
    // ============================================================
    slist() noexcept = default;

    explicit slist(size_type count, const T& value = T()) {
        // Build in reverse to keep order
        for (size_type i = count; i > 0; --i) {
            push_front(value);
        }
    }

    template<typename InputIterator,
             typename = enable_if_t<detail::is_input_iterator_v<InputIterator>>>
    slist(InputIterator first, InputIterator last) {
        // Build in reverse to maintain order
        slist tmp;
        for (; first != last; ++first) {
            tmp.push_front(*first);
        }
        tmp.reverse();
        swap(tmp);
    }

    slist(const slist& other) {
        // Build forward by inserting after the tail
        const_iterator src = other.begin();
        if (src == other.end()) return;
        base_type* prev = &head_;
        for (; src != other.end(); ++src) {
            node_type* node = create_node_emplace(*src);
            detail::slist_insert_after(prev, node);
            prev = node;
        }
        size_ = other.size_;
    }

    slist(slist&& other) noexcept
        : head_(other.head_), size_(other.size_) {
        other.head_.next = nullptr;
        other.size_ = 0;
    }

    slist(std::initializer_list<T> il) {
        auto it = il.begin();
        if (it == il.end()) return;
        base_type* prev = &head_;
        for (; it != il.end(); ++it) {
            node_type* node = create_node_emplace(*it);
            detail::slist_insert_after(prev, node);
            prev = node;
        }
        size_ = il.size();
    }

    ~slist() { clear(); }

    // ============================================================
    // Assignment
    // ============================================================
    slist& operator=(const slist& other) {
        if (this != &other) {
            slist tmp(other);
            swap(tmp);
        }
        return *this;
    }

    slist& operator=(slist&& other) noexcept {
        if (this != &other) {
            clear();
            head_.next = other.head_.next;
            size_ = other.size_;
            other.head_.next = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    slist& operator=(std::initializer_list<T> il) {
        assign(il);
        return *this;
    }

    // ============================================================
    // assign
    // ============================================================
    void assign(size_type count, const T& value) {
        clear();
        base_type* prev = &head_;
        for (size_type i = 0; i < count; ++i) {
            node_type* node = create_node_emplace(value);
            detail::slist_insert_after(prev, node);
            prev = node;
        }
        size_ = count;
    }

    template<typename InputIterator,
             typename = enable_if_t<detail::is_input_iterator_v<InputIterator>>>
    void assign(InputIterator first, InputIterator last) {
        clear();
        base_type* prev = &head_;
        for (; first != last; ++first) {
            node_type* node = create_node_emplace(*first);
            detail::slist_insert_after(prev, node);
            prev = node;
        }
        size_ = 0;
        for (base_type* cur = head_.next; cur; cur = cur->next) ++size_;
    }

    void assign(std::initializer_list<T> il) {
        clear();
        base_type* prev = &head_;
        for (const auto& v : il) {
            node_type* node = create_node_emplace(v);
            detail::slist_insert_after(prev, node);
            prev = node;
        }
        size_ = il.size();
    }

    // ============================================================
    // Element access
    // ============================================================
    reference front() noexcept { return node_type::value(head_.next); }
    const_reference front() const noexcept { return node_type::value(head_.next); }

    // ============================================================
    // Iterators
    // ============================================================
    iterator before_begin() noexcept { return iterator(&head_); }
    const_iterator before_begin() const noexcept { return const_iterator(const_cast<base_type*>(&head_)); }
    const_iterator cbefore_begin() const noexcept { return const_iterator(const_cast<base_type*>(&head_)); }

    iterator begin() noexcept { return iterator(head_.next); }
    const_iterator begin() const noexcept { return const_iterator(head_.next); }
    const_iterator cbegin() const noexcept { return const_iterator(head_.next); }
    iterator end() noexcept { return iterator(nullptr); }
    const_iterator end() const noexcept { return const_iterator(nullptr); }
    const_iterator cend() const noexcept { return const_iterator(nullptr); }

    // ============================================================
    // Capacity
    // ============================================================
    bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }  // O(1)
    size_type max_size() const noexcept { return static_cast<size_type>(-1) / sizeof(node_type); }

    // ============================================================
    // push_front / pop_front
    // ============================================================
    void push_front(const T& value) {
        emplace_front(value);
    }

    void push_front(T&& value) {
        emplace_front(zstl::move(value));
    }

    template<typename... Args>
    reference emplace_front(Args&&... args) {
        node_type* node = create_node_emplace(zstl::forward<Args>(args)...);
        detail::slist_insert_after(&head_, node);
        ++size_;
        return node->data;
    }

    void pop_front() {
        base_type* node = head_.next;
        head_.next = node->next;
        destroy_node(static_cast<node_type*>(node));
        --size_;
    }

    // ============================================================
    // insert_after / emplace_after
    // ============================================================
    iterator insert_after(const_iterator pos, const T& value) {
        return emplace_after(pos, value);
    }

    iterator insert_after(const_iterator pos, T&& value) {
        return emplace_after(pos, zstl::move(value));
    }

    iterator insert_after(const_iterator pos, size_type count, const T& value) {
        if (count == 0) return iterator(const_cast<base_type*>(pos.node));
        base_type* prev = const_cast<base_type*>(pos.node);
        for (size_type i = 0; i < count; ++i) {
            node_type* node = create_node_emplace(value);
            detail::slist_insert_after(prev, node);
            prev = node;
            ++size_;
        }
        return iterator(prev);
    }

    template<typename InputIterator,
             typename = enable_if_t<detail::is_input_iterator_v<InputIterator>>>
    iterator insert_after(const_iterator pos, InputIterator first, InputIterator last) {
        if (first == last) return iterator(const_cast<base_type*>(pos.node));
        base_type* prev = const_cast<base_type*>(pos.node);
        for (; first != last; ++first) {
            node_type* node = create_node_emplace(*first);
            detail::slist_insert_after(prev, node);
            prev = node;
            ++size_;
        }
        return iterator(prev);
    }

    iterator insert_after(const_iterator pos, std::initializer_list<T> il) {
        return insert_after(pos, il.begin(), il.end());
    }

    template<typename... Args>
    iterator emplace_after(const_iterator pos, Args&&... args) {
        node_type* node = create_node_emplace(zstl::forward<Args>(args)...);
        detail::slist_insert_after(const_cast<base_type*>(pos.node), node);
        ++size_;
        return iterator(node);
    }

    // ============================================================
    // erase_after
    // ============================================================
    iterator erase_after(const_iterator pos) {
        base_type* prev = const_cast<base_type*>(pos.node);
        base_type* node = prev->next;
        if (node == nullptr) return end();
        prev->next = node->next;
        destroy_node(static_cast<node_type*>(node));
        --size_;
        return iterator(prev->next);
    }

    iterator erase_after(const_iterator first, const_iterator last) {
        while (first.node->next != last.node) {
            erase_after(first);
        }
        return iterator(const_cast<base_type*>(last.node));
    }

    // ============================================================
    // resize
    // ============================================================
    void resize(size_type count) {
        resize(count, T());
    }

    void resize(size_type count, const T& value) {
        base_type* prev = &head_;
        size_type i = 0;

        // Walk to the end
        while (prev->next && i < count) {
            prev = prev->next;
            ++i;
        }

        if (i < count) {
            // Add elements
            while (i < count) {
                node_type* node = create_node_emplace(value);
                detail::slist_insert_after(prev, node);
                prev = node;
                ++i;
                ++size_;
            }
        } else {
            // Erase extra elements
            base_type* to_erase = prev->next;
            prev->next = nullptr;
            while (to_erase) {
                base_type* next = to_erase->next;
                destroy_node(static_cast<node_type*>(to_erase));
                --size_;
                to_erase = next;
            }
        }
    }

    // ============================================================
    // clear
    // ============================================================
    void clear() noexcept {
        base_type* cur = head_.next;
        while (cur) {
            base_type* next = cur->next;
            destroy_node(static_cast<node_type*>(cur));
            cur = next;
        }
        head_.next = nullptr;
        size_ = 0;
    }

    // ============================================================
    // swap
    // ============================================================
    void swap(slist& other) noexcept {
        zstl::swap(head_.next, other.head_.next);
        zstl::swap(size_, other.size_);
    }

    // ============================================================
    // splice_after — transfer elements after pos from other
    // ============================================================
    void splice_after(const_iterator pos, slist& other) {
        if (other.empty()) return;
        detail::slist_splice_after(const_cast<base_type*>(pos.node), &other.head_);
        size_ += other.size_;
        other.size_ = 0;
    }

    void splice_after(const_iterator pos, slist&& other) {
        splice_after(pos, other);
    }

    // Splice single element after it from other to after pos
    void splice_after(const_iterator pos, slist& other, const_iterator it) {
        base_type* target = const_cast<base_type*>(it.node)->next;
        if (target == nullptr) return;
        // Remove from other
        const_cast<base_type*>(it.node)->next = target->next;
        --other.size_;
        // Insert after pos
        detail::slist_insert_after(const_cast<base_type*>(pos.node), target);
        ++size_;
    }

    void splice_after(const_iterator pos, slist&& other, const_iterator it) {
        splice_after(pos, other, it);
    }

    // Splice range (before_last is the position *before* the last element to splice)
    void splice_after(const_iterator pos, slist& other,
                      const_iterator before_first, const_iterator before_last) {
        base_type* bf = const_cast<base_type*>(before_first.node);
        base_type* bl = const_cast<base_type*>(before_last.node);
        if (bf->next == bl->next) return;

        // Count and remove
        size_type n = 0;
        base_type* cur = bf->next;
        base_type* end = bl->next;
        while (cur != end) { ++n; cur = cur->next; }

        base_type* first = bf->next;
        bf->next = bl->next;
        bl->next = const_cast<base_type*>(pos.node)->next;
        const_cast<base_type*>(pos.node)->next = first;

        other.size_ -= n;
        size_ += n;
    }

    void splice_after(const_iterator pos, slist&& other,
                      const_iterator before_first, const_iterator before_last) {
        splice_after(pos, other, before_first, before_last);
    }

    // ============================================================
    // remove / remove_if
    // ============================================================
    void remove(const T& value) {
        remove_if([&value](const T& v) { return v == value; });
    }

    template<typename Predicate>
    void remove_if(Predicate pred) {
        base_type* prev = &head_;
        while (prev->next) {
            base_type* cur = prev->next;
            if (pred(node_type::value(cur))) {
                prev->next = cur->next;
                destroy_node(static_cast<node_type*>(cur));
                --size_;
            } else {
                prev = cur;
            }
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
        base_type* prev = head_.next;
        while (prev && prev->next) {
            base_type* cur = prev->next;
            if (pred(node_type::value(prev), node_type::value(cur))) {
                prev->next = cur->next;
                destroy_node(static_cast<node_type*>(cur));
                --size_;
            } else {
                prev = cur;
            }
        }
    }

    // ============================================================
    // merge — merge two sorted lists
    // ============================================================
    void merge(slist& other) {
        merge(other, less<void>());
    }

    void merge(slist&& other) {
        merge(other);
    }

    template<typename Compare>
    void merge(slist& other, Compare comp) {
        if (this == &other || other.empty()) return;
        if (empty()) {
            swap(other);
            return;
        }

        base_type* prev1 = &head_;
        base_type* cur1 = head_.next;
        base_type* cur2 = other.head_.next;

        while (cur1 && cur2) {
            if (comp(node_type::value(cur2), node_type::value(cur1))) {
                // Move cur2 after prev1
                base_type* next2 = cur2->next;
                cur2->next = cur1;
                prev1->next = cur2;
                prev1 = cur2;
                cur2 = next2;
                --other.size_;
                ++size_;
            } else {
                prev1 = cur1;
                cur1 = cur1->next;
            }
        }

        // Attach remaining elements from other
        if (cur2) {
            prev1->next = cur2;
            size_ += other.size_;
            other.head_.next = nullptr;
            other.size_ = 0;
        }
    }

    template<typename Compare>
    void merge(slist&& other, Compare comp) {
        merge(other, comp);
    }

    // ============================================================
    // sort — bottom-up merge sort (same algorithm as list)
    // ============================================================
    void sort() {
        sort(less<void>());
    }

    template<typename Compare>
    void sort(Compare comp) {
        if (size_ <= 1) return;

        slist carry;
        slist bin[64];
        int fill = 0;

        while (!empty()) {
            // Take one element
            base_type* node = head_.next;
            head_.next = node->next;
            --size_;
            node->next = nullptr;
            carry.head_.next = node;
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
        head_.next = detail::slist_reverse(head_.next);
    }

    // ============================================================
    // Comparison operators
    // ============================================================
    friend bool operator==(const slist& a, const slist& b) {
        if (a.size_ != b.size_) return false;
        auto ia = a.begin();
        auto ib = b.begin();
        for (; ia != a.end(); ++ia, ++ib) {
            if (!(*ia == *ib)) return false;
        }
        return true;
    }

    friend bool operator!=(const slist& a, const slist& b) {
        return !(a == b);
    }

    friend bool operator<(const slist& a, const slist& b) {
        auto ia = a.begin();
        auto ib = b.begin();
        for (; ia != a.end() && ib != b.end(); ++ia, ++ib) {
            if (*ia < *ib) return true;
            if (*ib < *ia) return false;
        }
        return a.size_ < b.size_;
    }

    friend bool operator>(const slist& a, const slist& b) {
        return b < a;
    }

    friend bool operator<=(const slist& a, const slist& b) {
        return !(b < a);
    }

    friend bool operator>=(const slist& a, const slist& b) {
        return !(a < b);
    }
};

// ============================================================
// Non-member swap
// ============================================================
template<typename T, typename Alloc>
void swap(slist<T, Alloc>& a, slist<T, Alloc>& b) noexcept {
    a.swap(b);
}

} // namespace zstl
