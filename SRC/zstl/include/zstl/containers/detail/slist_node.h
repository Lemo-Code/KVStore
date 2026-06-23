// zstl slist_node — singly-linked list node base and data node for zstl::slist
#pragma once

#include <cstddef>
#include <utility>

namespace zstl {
namespace detail {

// ============================================================
// slist_node_base — non-template base for singly-linked list nodes
// Contains only a next pointer. Used by slist_iterator for
// type erasure.
// ============================================================
struct slist_node_base {
    slist_node_base* next;

    slist_node_base() noexcept : next(nullptr) {}
};

// ============================================================
// slist_insert_after — insert `n` immediately after `pos`
// O(1), non-member for use on slist_node_base*
// ============================================================
inline void slist_insert_after(slist_node_base* pos, slist_node_base* n) noexcept {
    n->next = pos->next;
    pos->next = n;
}

// ============================================================
// slist_reverse — reverse a singly-linked list
// Returns the new head (old tail).
// O(n) time, O(1) space.
// ============================================================
inline slist_node_base* slist_reverse(slist_node_base* head) noexcept {
    slist_node_base* prev = nullptr;
    slist_node_base* cur = head;
    while (cur) {
        slist_node_base* next = cur->next;
        cur->next = prev;
        prev = cur;
        cur = next;
    }
    return prev;
}

// ============================================================
// slist_count — count nodes from head (excluding head if it's a sentinel)
// O(n). For debugging/validation only.
// ============================================================
inline size_t slist_count(const slist_node_base* head) noexcept {
    size_t n = 0;
    while (head) {
        ++n;
        head = head->next;
    }
    return n;
}

// ============================================================
// slist_splice_after — splice elements after `pos` from another list
// Transfers all elements after `other_head` to after `pos`.
// `other_head` is a sentinel (its data is not transferred).
// O(1).
// ============================================================
inline void slist_splice_after(slist_node_base* pos,
                                slist_node_base* other_head) noexcept {
    if (other_head->next == nullptr) return;  // Nothing to transfer
    slist_node_base* first = other_head->next;
    // Find last node of other list
    slist_node_base* last = first;
    while (last->next) {
        last = last->next;
    }
    last->next = pos->next;
    pos->next = first;
    other_head->next = nullptr;
}

// ============================================================
// slist_node<T> — templated node that holds a value of type T
// Inherits from slist_node_base to allow type-erased operations.
// ============================================================
template<typename T>
struct slist_node : public slist_node_base {
    T data;

    slist_node() : slist_node_base() {}

    template<typename... Args>
    explicit slist_node(Args&&... args)
        : slist_node_base()
        , data(std::forward<Args>(args)...) {}

    // Access T& from a slist_node_base* (static downcast)
    static T& value(slist_node_base* n) noexcept {
        return static_cast<slist_node<T>*>(n)->data;
    }

    static const T& value(const slist_node_base* n) noexcept {
        return static_cast<const slist_node<T>*>(n)->data;
    }

    // Convert slist_node_base* to slist_node<T>*
    static slist_node<T>* from_base(slist_node_base* n) noexcept {
        return static_cast<slist_node<T>*>(n);
    }

    static const slist_node<T>* from_base(const slist_node_base* n) noexcept {
        return static_cast<const slist_node<T>*>(n);
    }
};

} // namespace detail
} // namespace zstl
