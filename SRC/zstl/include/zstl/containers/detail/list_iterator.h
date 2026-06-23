// zstl list_iterator — bidirectional iterator for zstl::list
#pragma once

#include <cstddef>
#include <iterator>
#include <type_traits>

#include "list_node.h"

namespace zstl {
namespace detail {

// ============================================================
// list_iterator<T> — bidirectional iterator over zstl::list
//
// Stores a list_node_base* pointer for type erasure, enabling
// implicit conversion from list_iterator<T> to list_iterator<const T>.
//
// Complexity: O(1) for all operations.
// Invalidation: iterator remains valid as long as the referenced
//   node is not erased from the list.
// ============================================================
template<typename T>
class list_iterator {
public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type        = T;
    using difference_type   = ptrdiff_t;
    using pointer           = T*;
    using reference         = T&;

    using base_ptr  = list_node_base*;
    using node_type = list_node<T>;

    // ---- constructors ----
    list_iterator() noexcept : node_(nullptr) {}

    explicit list_iterator(base_ptr n) noexcept : node_(n) {}

    // Converting constructor: iterator -> const_iterator
    // Enabled only when U is convertible to T (non-const -> const)
    template<typename U,
             typename = typename std::enable_if<
                 std::is_convertible<U*, T*>::value>::type>
    list_iterator(const list_iterator<U>& other) noexcept
        : node_(other.base()) {}

    // ---- dereference ----
    reference operator*() const noexcept {
        return list_node<T>::value(node_);
    }

    pointer operator->() const noexcept {
        return &(operator*());
    }

    // ---- increment / decrement ----
    list_iterator& operator++() noexcept {
        node_ = node_->next;
        return *this;
    }

    list_iterator operator++(int) noexcept {
        list_iterator tmp = *this;
        node_ = node_->next;
        return tmp;
    }

    list_iterator& operator--() noexcept {
        node_ = node_->prev;
        return *this;
    }

    list_iterator operator--(int) noexcept {
        list_iterator tmp = *this;
        node_ = node_->prev;
        return tmp;
    }

    // ---- comparison ----
    bool operator==(const list_iterator& other) const noexcept {
        return node_ == other.node_;
    }

    bool operator!=(const list_iterator& other) const noexcept {
        return node_ != other.node_;
    }

    // ---- access underlying base pointer ----
    base_ptr base() const noexcept { return node_; }

private:
    base_ptr node_;
};

} // namespace detail
} // namespace zstl
