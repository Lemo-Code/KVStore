// zstl list_node — doubly-linked list node base and data node for zstl::list
#pragma once

#include <cstddef>
#include <utility>

namespace zstl {
namespace detail {

// ============================================================
// list_node_base — non-template base for doubly-linked list nodes
// Contains only prev/next pointers. Used by list_iterator for
// type erasure: the iterator stores list_node_base*.
// O(1) hook/unhook/transfer/reverse operations.
// ============================================================
struct list_node_base {
    list_node_base* next;
    list_node_base* prev;

    // Default: self-loop (sentinel node state)
    list_node_base() noexcept : next(this), prev(this) {}

    // Insert this node immediately before `pos` in the list.
    // After hook: this is the new predecessor of pos.
    // O(1), no allocation.
    void hook(list_node_base* pos) noexcept {
        this->next = pos;
        this->prev = pos->prev;
        pos->prev->next = this;
        pos->prev = this;
    }

    // Remove this node from its list.
    // After unhook: prev and next are linked to each other, skipping this.
    // O(1). The caller is responsible for destroying/deallocating the node.
    void unhook() noexcept {
        this->prev->next = this->next;
        this->next->prev = this->prev;
    }

    // Transfer the range [first, last) to just before `pos`.
    // The range [first, last) is removed from its current list
    // and spliced before `pos`. pos may be in the same list.
    // O(1). Precondition: first != last (caller must check).
    static void transfer(list_node_base* first, list_node_base* last,
                         list_node_base* pos) noexcept {
        if (first == last) return;

        // Save the boundaries of the range being moved
        list_node_base* first_prev = first->prev;
        list_node_base* last_prev = last->prev;

        // Remove [first, last) from its original list
        first_prev->next = last;
        last->prev = first_prev;

        // Splice [first, last) into position before pos
        last_prev->next = pos;
        first->prev = pos->prev;
        pos->prev->next = first;
        pos->prev = last_prev;
    }

    // Reverse the entire circular list anchored at `sentinel`.
    // Swaps prev/next for every node in the cycle.
    // O(n) where n is the number of elements.
    static void reverse(list_node_base* sentinel) noexcept {
        list_node_base* cur = sentinel;
        do {
            list_node_base* tmp = cur->next;
            cur->next = cur->prev;
            cur->prev = tmp;
            cur = tmp;  // move to the old-next (now prev)
        } while (cur != sentinel);
    }

    // Count nodes reachable from this base node (excluding self).
    // O(n). For debugging/validation only.
    size_t count() const noexcept {
        size_t n = 0;
        const list_node_base* cur = this->next;
        while (cur != this) {
            ++n;
            cur = cur->next;
        }
        return n;
    }
};

// ============================================================
// list_node<T> — templated node that holds a value of type T
// Inherits from list_node_base to allow type-erased operations.
// ============================================================
template<typename T>
struct list_node : public list_node_base {
    T data;

    // Default construct the data
    list_node() : list_node_base() {}

    // Perfect-forwarding constructor
    template<typename... Args>
    explicit list_node(Args&&... args)
        : list_node_base()
        , data(std::forward<Args>(args)...) {}

    // Access T& from a list_node_base* (static downcast)
    static T& value(list_node_base* n) noexcept {
        return static_cast<list_node<T>*>(n)->data;
    }

    static const T& value(const list_node_base* n) noexcept {
        return static_cast<const list_node<T>*>(n)->data;
    }

    // Convert list_node_base* to list_node<T>*
    static list_node<T>* from_base(list_node_base* n) noexcept {
        return static_cast<list_node<T>*>(n);
    }

    static const list_node<T>* from_base(const list_node_base* n) noexcept {
        return static_cast<const list_node<T>*>(n);
    }
};

// ============================================================
// list_size — count nodes in a circular list (helper utility)
// O(n). For validation/debugging.
// ============================================================
inline size_t list_size(const list_node_base* sentinel) noexcept {
    size_t n = 0;
    const list_node_base* cur = sentinel->next;
    while (cur != sentinel) {
        ++n;
        cur = cur->next;
    }
    return n;
}

// ============================================================
// list_validate — check invariants of a circular doubly-linked list
// Returns true if the list structure is valid:
//   - Every node's next->prev == node
//   - Every node's prev->next == node
// O(n). For debugging only.
// ============================================================
inline bool list_validate(const list_node_base* sentinel) noexcept {
    const list_node_base* cur = sentinel->next;
    while (cur != sentinel) {
        if (cur->next->prev != cur) return false;
        if (cur->prev->next != cur) return false;
        cur = cur->next;
    }
    return true;
}

} // namespace detail
} // namespace zstl
