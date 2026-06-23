// zero optional — an optional value container with proper aligned storage
// Similar to std::optional<T> but C++17 compatible.
// Supports has_value, value, value_or, operator*, operator->, emplace,
// reset, swap, and comparisons with nullopt and values.
#pragma once

#include <type_traits>
#include <utility>
#include <new>
#include <exception>
#include <cstddef>
#include <functional>

namespace zero {

// Tag for disengaged optional
struct nullopt_t {
    struct init {};
    constexpr explicit nullopt_t(init) {}
};

// Constant for the nullopt state
constexpr nullopt_t nullopt{nullopt_t::init{}};

// Exception for accessing empty optional
class bad_optional_access : public std::exception {
public:
    bad_optional_access() = default;
    const char* what() const noexcept override {
        return "zero::bad_optional_access: optional has no value";
    }
};

namespace detail {

// In-place tag
struct in_place_t {};
constexpr in_place_t in_place{};

// Storage that handles aligned union of T and empty state
template <typename T, bool = std::is_trivially_destructible_v<T>>
struct optional_storage_destruct {
    union {
        char empty_;
        T value_;
    };
    bool has_value_;

    optional_storage_destruct() noexcept : empty_{}, has_value_(false) {}

    ~optional_storage_destruct() {
        if (has_value_) {
            value_.~T();
        }
    }
};

template <typename T>
struct optional_storage_destruct<T, true> {
    union {
        char empty_;
        T value_;
    };
    bool has_value_;

    optional_storage_destruct() noexcept : empty_{}, has_value_(false) {}
    ~optional_storage_destruct() = default;
};

template <typename T, bool = std::is_trivially_copyable_v<T>>
struct optional_storage_copy
    : optional_storage_destruct<T> {
    using optional_storage_destruct<T>::optional_storage_destruct;
    optional_storage_copy() = default;
    optional_storage_copy(const optional_storage_copy&) = default;
    optional_storage_copy(optional_storage_copy&&) = default;
    optional_storage_copy& operator=(const optional_storage_copy&) = default;
    optional_storage_copy& operator=(optional_storage_copy&&) = default;
};

template <typename T>
struct optional_storage_copy<T, false>
    : optional_storage_destruct<T> {
    using optional_storage_destruct<T>::optional_storage_destruct;

    optional_storage_copy() = default;

    optional_storage_copy(const optional_storage_copy& other) {
        if (other.has_value_) {
            ::new (&this->value_) T(other.value_);
            this->has_value_ = true;
        }
    }

    optional_storage_copy(optional_storage_copy&& other)
        noexcept(std::is_nothrow_move_constructible_v<T>) {
        if (other.has_value_) {
            ::new (&this->value_) T(std::move(other.value_));
            this->has_value_ = true;
        }
    }

    optional_storage_copy& operator=(const optional_storage_copy& other) {
        if (this == &other) return *this;
        if (this->has_value_ && other.has_value_) {
            this->value_ = other.value_;
        } else if (other.has_value_) {
            ::new (&this->value_) T(other.value_);
            this->has_value_ = true;
        } else if (this->has_value_) {
            this->value_.~T();
            this->has_value_ = false;
        }
        return *this;
    }

    optional_storage_copy& operator=(optional_storage_copy&& other)
        noexcept(std::is_nothrow_move_assignable_v<T> &&
                  std::is_nothrow_move_constructible_v<T>) {
        if (this == &other) return *this;
        if (this->has_value_ && other.has_value_) {
            this->value_ = std::move(other.value_);
        } else if (other.has_value_) {
            ::new (&this->value_) T(std::move(other.value_));
            this->has_value_ = true;
        } else if (this->has_value_) {
            this->value_.~T();
            this->has_value_ = false;
        }
        return *this;
    }
};

// Base class that holds the storage
template <typename T>
struct optional_base : optional_storage_copy<T> {
    using optional_storage_copy<T>::optional_storage_copy;

    template <typename... Args>
    void construct(Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, Args...>) {
        ::new (&this->value_) T(std::forward<Args>(args)...);
        this->has_value_ = true;
    }

    void destroy() noexcept {
        if (this->has_value_) {
            this->value_.~T();
            this->has_value_ = false;
        }
    }

    T& ref() noexcept { return this->value_; }
    const T& ref() const noexcept { return this->value_; }
    T* ptr() noexcept { return &this->value_; }
    const T* ptr() const noexcept { return &this->value_; }
};

} // namespace detail

template <typename T>
class optional : private detail::optional_base<T> {
    static_assert(!std::is_same_v<std::decay_t<T>, nullopt_t>,
                  "optional<T> cannot be instantiated with nullopt_t");
    static_assert(!std::is_same_v<std::decay_t<T>, std::in_place_t>,
                  "optional<T> cannot be instantiated with in_place_t");
    static_assert(!std::is_reference_v<T>,
                  "optional<T> cannot be instantiated with a reference type");

    using base = detail::optional_base<T>;

public:
    using value_type = T;

    // ============================================================
    // Constructors
    // ============================================================

    // Default — empty optional
    optional() noexcept = default;

    // Empty from nullopt
    optional(nullopt_t) noexcept {}

    // Copy constructor
    optional(const optional&) = default;

    // Move constructor
    optional(optional&&) noexcept(std::is_nothrow_move_constructible_v<T>) = default;

    // Copy from value
    optional(const T& value) {
        base::construct(value);
    }

    // Move from value
    optional(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>) {
        base::construct(std::move(value));
    }

    // In-place construction
    template <typename... Args,
              typename = std::enable_if_t<std::is_constructible_v<T, Args...>>>
    explicit optional(std::in_place_t, Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, Args...>) {
        base::construct(std::forward<Args>(args)...);
    }

    // In-place with initializer list
    template <typename U, typename... Args,
              typename = std::enable_if_t<
                  std::is_constructible_v<T, std::initializer_list<U>&,
                                           Args...>>>
    explicit optional(std::in_place_t,
                      std::initializer_list<U> ilist,
                      Args&&... args) {
        base::construct(ilist, std::forward<Args>(args)...);
    }

    // Destructor
    ~optional() = default;

    // ============================================================
    // Assignment
    // ============================================================

    optional& operator=(nullopt_t) noexcept {
        base::destroy();
        return *this;
    }

    optional& operator=(const optional&) = default;

    optional& operator=(optional&&) noexcept(
        std::is_nothrow_move_assignable_v<T> &&
        std::is_nothrow_move_constructible_v<T>) = default;

    optional& operator=(const T& value) {
        if (this->has_value_) {
            base::ref() = value;
        } else {
            base::construct(value);
        }
        return *this;
    }

    optional& operator=(T&& value)
        noexcept(std::is_nothrow_move_assignable_v<T> &&
                  std::is_nothrow_move_constructible_v<T>) {
        if (this->has_value_) {
            base::ref() = std::move(value);
        } else {
            base::construct(std::move(value));
        }
        return *this;
    }

    // ============================================================
    // Observers
    // ============================================================

    // Check if optional contains a value
    bool has_value() const noexcept { return this->has_value_; }
    explicit operator bool() const noexcept { return has_value(); }

    // Access the contained value (throws bad_optional_access if empty)
    T& value() & {
        if (!has_value()) throw bad_optional_access();
        return base::ref();
    }

    const T& value() const& {
        if (!has_value()) throw bad_optional_access();
        return base::ref();
    }

    T&& value() && {
        if (!has_value()) throw bad_optional_access();
        return std::move(base::ref());
    }

    const T&& value() const&& {
        if (!has_value()) throw bad_optional_access();
        return std::move(base::ref());
    }

    // Get the value or a fallback default
    template <typename U>
    T value_or(U&& default_value) const& {
        return has_value() ? base::ref()
                           : static_cast<T>(std::forward<U>(default_value));
    }

    template <typename U>
    T value_or(U&& default_value) && {
        return has_value() ? std::move(base::ref())
                           : static_cast<T>(std::forward<U>(default_value));
    }

    // Pointer access
    T* operator->() noexcept { return base::ptr(); }
    const T* operator->() const noexcept { return base::ptr(); }

    // Dereference
    T& operator*() & noexcept { return base::ref(); }
    const T& operator*() const& noexcept { return base::ref(); }
    T&& operator*() && noexcept { return std::move(base::ref()); }
    const T&& operator*() const&& noexcept { return std::move(base::ref()); }

    // ============================================================
    // Modifiers
    // ============================================================

    // Construct a new value in place (destroy old if any)
    template <typename... Args>
    T& emplace(Args&&... args) {
        base::destroy();
        base::construct(std::forward<Args>(args)...);
        return base::ref();
    }

    template <typename U, typename... Args>
    T& emplace(std::initializer_list<U> ilist, Args&&... args) {
        base::destroy();
        base::construct(ilist, std::forward<Args>(args)...);
        return base::ref();
    }

    // Destroy the contained value
    void reset() noexcept {
        base::destroy();
    }

    // Swap with another optional
    void swap(optional& other) noexcept(
        std::is_nothrow_move_constructible_v<T> &&
        std::is_nothrow_swappable_v<T>) {
        if (has_value() && other.has_value()) {
            using std::swap;
            swap(base::ref(), other.base::ref());
        } else if (has_value()) {
            other.base::construct(std::move(base::ref()));
            base::destroy();
        } else if (other.has_value()) {
            base::construct(std::move(other.base::ref()));
            other.base::destroy();
        }
    }

private:
    using base::has_value_;
    using base::value_;
};

// ============================================================
// make_optional
// ============================================================

template <typename T>
optional<std::decay_t<T>> make_optional(T&& value) {
    return optional<std::decay_t<T>>(std::forward<T>(value));
}

template <typename T, typename... Args>
optional<T> make_optional(Args&&... args) {
    return optional<T>(std::in_place, std::forward<Args>(args)...);
}

template <typename T, typename U, typename... Args>
optional<T> make_optional(std::initializer_list<U> ilist, Args&&... args) {
    return optional<T>(std::in_place, ilist, std::forward<Args>(args)...);
}

// ============================================================
// Comparisons
// ============================================================

template <typename T>
bool operator==(const optional<T>& lhs, const optional<T>& rhs) {
    if (lhs.has_value() != rhs.has_value()) return false;
    if (!lhs.has_value()) return true;
    return *lhs == *rhs;
}

template <typename T>
bool operator!=(const optional<T>& lhs, const optional<T>& rhs) {
    return !(lhs == rhs);
}

template <typename T>
bool operator<(const optional<T>& lhs, const optional<T>& rhs) {
    if (!rhs.has_value()) return false;
    if (!lhs.has_value()) return true;
    return *lhs < *rhs;
}

template <typename T>
bool operator>(const optional<T>& lhs, const optional<T>& rhs) {
    return rhs < lhs;
}

template <typename T>
bool operator<=(const optional<T>& lhs, const optional<T>& rhs) {
    return !(rhs < lhs);
}

template <typename T>
bool operator>=(const optional<T>& lhs, const optional<T>& rhs) {
    return !(lhs < rhs);
}

// Comparisons with nullopt
template <typename T>
bool operator==(const optional<T>& opt, nullopt_t) noexcept {
    return !opt.has_value();
}

template <typename T>
bool operator==(nullopt_t, const optional<T>& opt) noexcept {
    return !opt.has_value();
}

template <typename T>
bool operator!=(const optional<T>& opt, nullopt_t) noexcept {
    return opt.has_value();
}

template <typename T>
bool operator!=(nullopt_t, const optional<T>& opt) noexcept {
    return opt.has_value();
}

template <typename T>
bool operator<(const optional<T>&, nullopt_t) noexcept {
    return false;
}

template <typename T>
bool operator<(nullopt_t, const optional<T>& opt) noexcept {
    return opt.has_value();
}

// Comparisons with value T
template <typename T>
bool operator==(const optional<T>& opt, const T& value) {
    return opt.has_value() && *opt == value;
}

template <typename T>
bool operator==(const T& value, const optional<T>& opt) {
    return opt.has_value() && *opt == value;
}

template <typename T>
bool operator!=(const optional<T>& opt, const T& value) {
    return !opt.has_value() || *opt != value;
}

template <typename T>
bool operator!=(const T& value, const optional<T>& opt) {
    return !opt.has_value() || *opt != value;
}

template <typename T>
bool operator<(const optional<T>& opt, const T& value) {
    return !opt.has_value() || *opt < value;
}

template <typename T>
bool operator<(const T& value, const optional<T>& opt) {
    return opt.has_value() && value < *opt;
}

// ============================================================
// std::swap specialization
// ============================================================

} // namespace zero

namespace std {
template <typename T>
void swap(zero::optional<T>& lhs, zero::optional<T>& rhs) noexcept(
    noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}
} // namespace std
