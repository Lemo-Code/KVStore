// zstl deque_iterator — random-access iterator for segmented zstl::deque
//
// Deque stores elements in a segmented array: a "map" of pointers to
// fixed-size blocks. The iterator traverses across block boundaries
// transparently, providing O(1) random access.
//
// Member fields:
//   cur   — pointer to current element within the current block
//   first — pointer to start of the current block
//   last  — pointer to one-past-the-end of the current block
//   node  — pointer to the current block's pointer in the map (T**)
//
// Block size: max(1, 512 / sizeof(T)) for typical types;
//   ensures blocks are ~512 bytes.
#pragma once

#include <cstddef>
#include <iterator>
#include <type_traits>

namespace zstl {
namespace detail {

template<typename T, size_t BufSize = 0>
class deque_iterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = T;
    using difference_type   = ptrdiff_t;
    using pointer           = T*;
    using reference         = T&;
    using map_pointer       = T**;

    using self = deque_iterator<T, BufSize>;

    // ---- data members ----
    pointer     cur_;    // current element in current block
    pointer     first_;  // start of current block
    pointer     last_;   // one-past-end of current block
    map_pointer node_;   // pointer into the map (T**)

    // ---- constructors ----
    deque_iterator() noexcept
        : cur_(nullptr), first_(nullptr), last_(nullptr), node_(nullptr) {}

    deque_iterator(pointer cur, map_pointer node) noexcept
        : cur_(cur)
        , first_(*node)
        , last_(*node + block_size())
        , node_(node) {}

    // Implicit conversion: iterator -> const_iterator
    // Enabled only when T is const and U is the matching non-const type.
    template<typename U,
             typename = typename std::enable_if<
                 std::is_const<T>::value &&
                 std::is_same<typename std::remove_const<T>::type, U>::value>::type>
    deque_iterator(const deque_iterator<U, BufSize>& other) noexcept
        : cur_(other.cur_)
        , first_(other.first_)
        , last_(other.last_)
        , node_(const_cast<map_pointer>(other.node_)) {}

    // ---- block management ----
    // Reposition this iterator into a new block (updating first/last).
    void set_node(map_pointer new_node) noexcept {
        node_ = new_node;
        first_ = *new_node;
        last_ = first_ + block_size();
    }

    // ---- computed block size ----
    // Uses BufSize if explicitly provided (non-zero), otherwise
    // computes: max(1, 512 / sizeof(T)) so blocks are ~512 bytes.
    static constexpr size_t block_size() noexcept {
        if constexpr (BufSize > 0) {
            return BufSize;
        } else {
            return sizeof(T) < 512 ? 512 / sizeof(T) : 1;
        }
    }

    // ---- dereference ----
    reference operator*() const noexcept { return *cur_; }
    pointer operator->() const noexcept { return cur_; }

    // ---- distance ----
    difference_type operator-(const self& other) const noexcept {
        return difference_type(block_size()) * (node_ - other.node_ - 1)
               + (cur_ - first_) + (other.last_ - other.cur_);
    }

    // ---- increment / decrement ----
    self& operator++() noexcept {
        ++cur_;
        if (cur_ == last_) {
            set_node(node_ + 1);
            cur_ = first_;
        }
        return *this;
    }

    self operator++(int) noexcept {
        self tmp = *this;
        ++(*this);
        return tmp;
    }

    self& operator--() noexcept {
        if (cur_ == first_) {
            set_node(node_ - 1);
            cur_ = last_;
        }
        --cur_;
        return *this;
    }

    self operator--(int) noexcept {
        self tmp = *this;
        --(*this);
        return tmp;
    }

    // ---- arithmetic ----
    self& operator+=(difference_type n) noexcept {
        if (n != 0) {
            difference_type offset = n + (cur_ - first_);
            if (offset >= 0 && offset < static_cast<difference_type>(block_size())) {
                // Stay in the same block
                cur_ += n;
            } else {
                // Cross block boundaries
                difference_type node_offset;
                if (offset >= 0) {
                    node_offset = offset / static_cast<difference_type>(block_size());
                } else {
                    // Handle negative offset correctly (round toward -inf)
                    node_offset = -((static_cast<difference_type>(block_size()) - 1 - offset)
                                    / static_cast<difference_type>(block_size()));
                }
                set_node(node_ + node_offset);
                cur_ = first_ + (offset - node_offset * static_cast<difference_type>(block_size()));
            }
        }
        return *this;
    }

    self& operator-=(difference_type n) noexcept {
        return *this += -n;
    }

    self operator+(difference_type n) const noexcept {
        self tmp = *this;
        return tmp += n;
    }

    friend self operator+(difference_type n, const self& it) noexcept {
        self tmp = it;
        return tmp += n;
    }

    self operator-(difference_type n) const noexcept {
        self tmp = *this;
        return tmp -= n;
    }

    // ---- subscript ----
    reference operator[](difference_type n) const noexcept {
        return *(*this + n);
    }

    // ---- comparison ----
    bool operator==(const self& other) const noexcept {
        return cur_ == other.cur_;
    }

    bool operator!=(const self& other) const noexcept {
        return !(*this == other);
    }

    bool operator<(const self& other) const noexcept {
        return (node_ == other.node_) ? (cur_ < other.cur_) : (node_ < other.node_);
    }

    bool operator>(const self& other) const noexcept {
        return other < *this;
    }

    bool operator<=(const self& other) const noexcept {
        return !(other < *this);
    }

    bool operator>=(const self& other) const noexcept {
        return !(*this < other);
    }
};

} // namespace detail
} // namespace zstl
