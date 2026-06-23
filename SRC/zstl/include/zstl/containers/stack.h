// zstl stack — LIFO container adaptor, default underlying container: deque
#pragma once

#include "zstl/containers/deque.h"
#include "zstl/memory/utility.h"

namespace zstl {

// ============================================================
// stack<T, Container>
//
// Adapts a sequence container to provide a last-in, first-out
// (LIFO) interface.  The default container is zstl::deque<T>.
//
// Any container that provides back(), push_back(), pop_back(),
// empty(), size() and emplace_back() can serve as the underlying
// container.
// ============================================================
template<typename T, typename Container = deque<T>>
class stack {
public:
    // ---- container types ----
    using container_type  = Container;
    using value_type      = typename Container::value_type;
    using size_type       = typename Container::size_type;
    using difference_type = typename Container::difference_type;
    using reference       = typename Container::reference;
    using const_reference = typename Container::const_reference;

    // ---- STL-compatible aliases (for generic code) ----
    using iterator               = void;  // stack is not iterable
    using const_iterator         = void;
    using reverse_iterator       = void;
    using const_reverse_iterator = void;

protected:
    Container c_;

public:
    // ============================================================
    // Constructors
    // ============================================================

    // Default constructor — creates an empty stack
    stack() noexcept(std::is_nothrow_default_constructible_v<Container>) = default;

    // Construct with a specific container (copy)
    explicit stack(const Container& cont)
        noexcept(std::is_nothrow_copy_constructible_v<Container>)
        : c_(cont) {}

    // Construct with a specific container (move)
    explicit stack(Container&& cont)
        noexcept(std::is_nothrow_move_constructible_v<Container>)
        : c_(zstl::move(cont)) {}

    // Copy constructor
    stack(const stack& other)
        noexcept(std::is_nothrow_copy_constructible_v<Container>) = default;

    // Move constructor
    stack(stack&& other)
        noexcept(std::is_nothrow_move_constructible_v<Container>) = default;

    // Destructor
    ~stack() = default;

    // ============================================================
    // Assignment
    // ============================================================

    stack& operator=(const stack& other)
        noexcept(std::is_nothrow_copy_assignable_v<Container>) = default;

    stack& operator=(stack&& other)
        noexcept(std::is_nothrow_move_assignable_v<Container>) = default;

    // ============================================================
    // Element access
    // ============================================================

    // Returns a reference to the top element (most recently pushed)
    reference top() {
        return c_.back();
    }

    const_reference top() const {
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

    // Push a copy of value onto the stack
    void push(const T& value) {
        c_.push_back(value);
    }

    // Push value onto the stack (move)
    void push(T&& value) {
        c_.push_back(zstl::move(value));
    }

    // Remove the top element
    void pop() {
        c_.pop_back();
    }

    // Construct element in-place at the top
    template<typename... Args>
    decltype(auto) emplace(Args&&... args) {
        return c_.emplace_back(zstl::forward<Args>(args)...);
    }

    // Swap contents with another stack
    void swap(stack& other) noexcept(noexcept(zstl::swap(c_, other.c_))) {
        zstl::swap(c_, other.c_);
    }

    // ============================================================
    // Underlying container access
    // ============================================================

    // Returns a copy of the underlying container (C++17)
    Container _get_container() const & {
        return c_;
    }

    // Returns the underlying container (move)
    Container _get_container() && {
        return zstl::move(c_);
    }

protected:
    // Protected access to underlying container (for derived classes)
    Container& _container() noexcept { return c_; }
    const Container& _container() const noexcept { return c_; }
};

// ============================================================
// Non-member swap
// ============================================================

template<typename T, typename Container>
void swap(stack<T, Container>& a, stack<T, Container>& b)
    noexcept(noexcept(a.swap(b))) {
    a.swap(b);
}

// ============================================================
// Comparison operators
// ============================================================

template<typename T, typename Container>
bool operator==(const stack<T, Container>& a, const stack<T, Container>& b) {
    return a._get_container() == b._get_container();
}

template<typename T, typename Container>
bool operator!=(const stack<T, Container>& a, const stack<T, Container>& b) {
    return a._get_container() != b._get_container();
}

template<typename T, typename Container>
bool operator<(const stack<T, Container>& a, const stack<T, Container>& b) {
    return a._get_container() < b._get_container();
}

template<typename T, typename Container>
bool operator>(const stack<T, Container>& a, const stack<T, Container>& b) {
    return a._get_container() > b._get_container();
}

template<typename T, typename Container>
bool operator<=(const stack<T, Container>& a, const stack<T, Container>& b) {
    return a._get_container() <= b._get_container();
}

template<typename T, typename Container>
bool operator>=(const stack<T, Container>& a, const stack<T, Container>& b) {
    return a._get_container() >= b._get_container();
}

} // namespace zstl
