// zstl queue — FIFO container adaptor, default underlying container: deque
#pragma once

#include "zstl/containers/deque.h"
#include "zstl/memory/utility.h"

namespace zstl {

// ============================================================
// queue<T, Container>
//
// Adapts a sequence container to provide a first-in, first-out
// (FIFO) interface.  The default container is zstl::deque<T>.
//
// The underlying container must support:
//   front(), back(), push_back(), pop_front(), empty(), size(),
//   emplace_back()
// ============================================================
template<typename T, typename Container = deque<T>>
class queue {
public:
    // ---- container types ----
    using container_type  = Container;
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

public:
    // ============================================================
    // Constructors
    // ============================================================

    // Default constructor — creates an empty queue
    queue() noexcept(std::is_nothrow_default_constructible_v<Container>) = default;

    // Construct with a specific container (copy)
    explicit queue(const Container& cont)
        noexcept(std::is_nothrow_copy_constructible_v<Container>)
        : c_(cont) {}

    // Construct with a specific container (move)
    explicit queue(Container&& cont)
        noexcept(std::is_nothrow_move_constructible_v<Container>)
        : c_(zstl::move(cont)) {}

    // Copy constructor
    queue(const queue& other)
        noexcept(std::is_nothrow_copy_constructible_v<Container>) = default;

    // Move constructor
    queue(queue&& other)
        noexcept(std::is_nothrow_move_constructible_v<Container>) = default;

    // Destructor
    ~queue() = default;

    // ============================================================
    // Assignment
    // ============================================================

    queue& operator=(const queue& other)
        noexcept(std::is_nothrow_copy_assignable_v<Container>) = default;

    queue& operator=(queue&& other)
        noexcept(std::is_nothrow_move_assignable_v<Container>) = default;

    // ============================================================
    // Element access
    // ============================================================

    // Returns a reference to the front element (oldest)
    reference front() {
        return c_.front();
    }

    const_reference front() const {
        return c_.front();
    }

    // Returns a reference to the back element (newest)
    reference back() {
        return c_.back();
    }

    const_reference back() const {
        return c_.back();
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

    // Push a copy of value to the back
    void push(const T& value) {
        c_.push_back(value);
    }

    // Push value to the back (move)
    void push(T&& value) {
        c_.push_back(zstl::move(value));
    }

    // Remove the front element
    void pop() {
        c_.pop_front();
    }

    // Construct element in-place at the back
    template<typename... Args>
    decltype(auto) emplace(Args&&... args) {
        return c_.emplace_back(zstl::forward<Args>(args)...);
    }

    // Swap contents with another queue
    void swap(queue& other) noexcept(noexcept(zstl::swap(c_, other.c_))) {
        zstl::swap(c_, other.c_);
    }

    // ============================================================
    // Underlying container access
    // ============================================================

    Container _get_container() const & {
        return c_;
    }

    Container _get_container() && {
        return zstl::move(c_);
    }

protected:
    Container& _container() noexcept { return c_; }
    const Container& _container() const noexcept { return c_; }
};

// ============================================================
// Non-member swap
// ============================================================

template<typename T, typename Container>
void swap(queue<T, Container>& a, queue<T, Container>& b)
    noexcept(noexcept(a.swap(b))) {
    a.swap(b);
}

// ============================================================
// Comparison operators
// ============================================================

template<typename T, typename Container>
bool operator==(const queue<T, Container>& a, const queue<T, Container>& b) {
    return a._get_container() == b._get_container();
}

template<typename T, typename Container>
bool operator!=(const queue<T, Container>& a, const queue<T, Container>& b) {
    return a._get_container() != b._get_container();
}

template<typename T, typename Container>
bool operator<(const queue<T, Container>& a, const queue<T, Container>& b) {
    return a._get_container() < b._get_container();
}

template<typename T, typename Container>
bool operator>(const queue<T, Container>& a, const queue<T, Container>& b) {
    return a._get_container() > b._get_container();
}

template<typename T, typename Container>
bool operator<=(const queue<T, Container>& a, const queue<T, Container>& b) {
    return a._get_container() <= b._get_container();
}

template<typename T, typename Container>
bool operator>=(const queue<T, Container>& a, const queue<T, Container>& b) {
    return a._get_container() >= b._get_container();
}

} // namespace zstl
