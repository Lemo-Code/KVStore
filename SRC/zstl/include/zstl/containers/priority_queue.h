// zstl priority_queue — max-heap container adaptor, default: vector + less
#pragma once

#include "zstl/containers/vector.h"
#include "zstl/containers/detail/heap.h"
#include "zstl/memory/utility.h"

namespace zstl {

// ============================================================
// priority_queue<T, Container, Compare>
//
// Adapts a random-access sequence container to provide a
// priority-queue (max-heap) interface.  The top element is
// always the largest according to Compare.
//
// The default container is zstl::vector<T>.
// The default comparator is zstl::less<T>.
//
// Underlying container must support random-access iterators
// plus front(), push_back(), pop_back(), empty(), size().
// ============================================================
template<typename T,
         typename Container = vector<T>,
         typename Compare = less<typename Container::value_type>>
class priority_queue {
public:
    // ---- container types ----
    using container_type  = Container;
    using value_compare   = Compare;
    using value_type      = typename Container::value_type;
    using size_type       = typename Container::size_type;
    using difference_type = typename Container::difference_type;
    using reference       = typename Container::reference;
    using const_reference = typename Container::const_reference;

    // ---- STL-compatible aliases ----
    using iterator               = void;
    using const_iterator         = void;
    using reverse_iterator       = void;
    using const_reverse_iterator = void;

protected:
    Container c_;
    Compare   comp_;

public:
    // ============================================================
    // Constructors
    // ============================================================

    // Default constructor — empty priority queue
    priority_queue()
        noexcept(std::is_nothrow_default_constructible_v<Container> &&
                 std::is_nothrow_default_constructible_v<Compare>)
        : c_(), comp_() {}

    // Construct with a custom comparator
    explicit priority_queue(const Compare& comp)
        noexcept(std::is_nothrow_default_constructible_v<Container> &&
                 std::is_nothrow_copy_constructible_v<Compare>)
        : c_(), comp_(comp) {}

    // Construct with a custom comparator (move)
    explicit priority_queue(Compare&& comp)
        noexcept(std::is_nothrow_default_constructible_v<Container> &&
                 std::is_nothrow_move_constructible_v<Compare>)
        : c_(), comp_(zstl::move(comp)) {}

    // Build from an iterator range (copies elements, heapifies)
    template<typename InputIterator>
    priority_queue(InputIterator first, InputIterator last,
                   const Compare& comp = Compare())
        : c_(first, last), comp_(comp) {
        make_heap(c_.begin(), c_.end(), comp_);
    }

    // Build from an iterator range with move comparator
    template<typename InputIterator>
    priority_queue(InputIterator first, InputIterator last,
                   Compare&& comp)
        : c_(first, last), comp_(zstl::move(comp)) {
        make_heap(c_.begin(), c_.end(), comp_);
    }

    // Construct from existing container (copy)
    explicit priority_queue(const Container& cont,
                            const Compare& comp = Compare())
        : c_(cont), comp_(comp) {
        make_heap(c_.begin(), c_.end(), comp_);
    }

    // Construct from existing container (move)
    explicit priority_queue(Container&& cont,
                            const Compare& comp = Compare())
        : c_(zstl::move(cont)), comp_(comp) {
        make_heap(c_.begin(), c_.end(), comp_);
    }

    // Copy constructor
    priority_queue(const priority_queue& other)
        noexcept(std::is_nothrow_copy_constructible_v<Container> &&
                 std::is_nothrow_copy_constructible_v<Compare>)
        : c_(other.c_), comp_(other.comp_) {}

    // Move constructor
    priority_queue(priority_queue&& other)
        noexcept(std::is_nothrow_move_constructible_v<Container> &&
                 std::is_nothrow_move_constructible_v<Compare>)
        : c_(zstl::move(other.c_)), comp_(zstl::move(other.comp_)) {}

    // Destructor
    ~priority_queue() = default;

    // ============================================================
    // Assignment
    // ============================================================

    priority_queue& operator=(const priority_queue& other)
        noexcept(std::is_nothrow_copy_assignable_v<Container> &&
                 std::is_nothrow_copy_assignable_v<Compare>) {
        if (this != &other) {
            c_   = other.c_;
            comp_ = other.comp_;
        }
        return *this;
    }

    priority_queue& operator=(priority_queue&& other)
        noexcept(std::is_nothrow_move_assignable_v<Container> &&
                 std::is_nothrow_move_assignable_v<Compare>) {
        if (this != &other) {
            c_   = zstl::move(other.c_);
            comp_ = zstl::move(other.comp_);
        }
        return *this;
    }

    // ============================================================
    // Element access
    // ============================================================

    // Returns const reference to the top (largest) element
    const_reference top() const {
        return c_.front();
    }

    // ============================================================
    // Capacity
    // ============================================================

    bool empty() const noexcept {
        return c_.empty();
    }

    size_type size() const noexcept {
        return c_.size();
    }

    // ============================================================
    // Modifiers
    // ============================================================

    // Push a copy to the heap
    void push(const T& value) {
        c_.push_back(value);
        push_heap(c_.begin(), c_.end(), comp_);
    }

    // Push value to the heap (move)
    void push(T&& value) {
        c_.push_back(zstl::move(value));
        push_heap(c_.begin(), c_.end(), comp_);
    }

    // Remove the top element
    void pop() {
        pop_heap(c_.begin(), c_.end(), comp_);
        c_.pop_back();
    }

    // Construct element in-place
    template<typename... Args>
    void emplace(Args&&... args) {
        c_.emplace_back(zstl::forward<Args>(args)...);
        push_heap(c_.begin(), c_.end(), comp_);
    }

    // Swap contents with another priority_queue
    void swap(priority_queue& other)
        noexcept(noexcept(zstl::swap(c_, other.c_)) &&
                 noexcept(zstl::swap(comp_, other.comp_))) {
        zstl::swap(c_, other.c_);
        zstl::swap(comp_, other.comp_);
    }

    // ============================================================
    // Underlying container / comparator access
    // ============================================================

    Container _get_container() const & {
        return c_;
    }

    Container _get_container() && {
        return zstl::move(c_);
    }

    Compare value_comp() const {
        return comp_;
    }

protected:
    Container& _container() noexcept { return c_; }
    const Container& _container() const noexcept { return c_; }
};

// ============================================================
// Non-member swap
// ============================================================

template<typename T, typename Container, typename Compare>
void swap(priority_queue<T, Container, Compare>& a,
          priority_queue<T, Container, Compare>& b)
    noexcept(noexcept(a.swap(b))) {
    a.swap(b);
}

} // namespace zstl
