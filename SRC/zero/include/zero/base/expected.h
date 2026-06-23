// zero expected — result-or-error type (C++23 style, simplified)
// expected<T, E> holds either a value of type T or an error of type E.
// Supports monadic operations: and_then, or_else, transform,
// transform_error. Similar to std::expected but works with C++17.
#pragma once

#include <type_traits>
#include <utility>
#include <new>
#include <exception>
#include <cstddef>
#include <functional>

#include "zero/base/macro.h"

namespace zero {

// Exception thrown when accessing value of an expected holding an error
template <typename E>
class bad_expected_access : public std::exception {
public:
    explicit bad_expected_access(E error) : error_(std::move(error)) {}

    const E& error() const& noexcept { return error_; }
    E& error() & noexcept { return error_; }
    const E&& error() const&& noexcept { return std::move(error_); }
    E&& error() && noexcept { return std::move(error_); }

    const char* what() const noexcept override {
        return "zero::bad_expected_access: expected has no value";
    }

private:
    E error_;
};

// Tag for constructing with unexpected error
template <typename E>
class unexpected {
public:
    unexpected() = delete;

    explicit unexpected(const E& e) : error_(e) {}
    explicit unexpected(E&& e) noexcept : error_(std::move(e)) {}

    template <typename... Args>
    explicit unexpected(std::in_place_t, Args&&... args)
        : error_(std::forward<Args>(args)...) {}

    template <typename U, typename... Args>
    explicit unexpected(std::in_place_t,
                         std::initializer_list<U> ilist,
                         Args&&... args)
        : error_(ilist, std::forward<Args>(args)...) {}

    const E& error() const& noexcept { return error_; }
    E& error() & noexcept { return error_; }
    const E&& error() const&& noexcept { return std::move(error_); }
    E&& error() && noexcept { return std::move(error_); }

private:
    E error_;
};

template <typename T, typename E>
class expected {
    static_assert(!std::is_reference_v<T>, "expected<T, E>: T must not be a reference");
    static_assert(!std::is_reference_v<E>, "expected<T, E>: E must not be a reference");
    static_assert(!std::is_same_v<std::remove_cv_t<T>, unexpected<E>>,
                  "expected<T, E>: T must not be unexpected<E>");
    static_assert(!std::is_same_v<std::remove_cv_t<T>, std::in_place_t>,
                  "expected<T, E>: T must not be in_place_t");

    union {
        T value_;
        E error_;
    };
    bool has_value_;

public:
    using value_type = T;
    using error_type = E;
    using unexpected_type = unexpected<E>;

    // ============================================================
    // Constructors
    // ============================================================

    // Construct with a value
    template <typename U = T,
              typename = std::enable_if_t<
                  !std::is_same_v<std::decay_t<U>, expected> &&
                  !std::is_same_v<std::decay_t<U>, unexpected<E>>>>
    expected(U&& value)
        noexcept(std::is_nothrow_constructible_v<T, U>)
        : value_(std::forward<U>(value)), has_value_(true) {}

    // Construct from unexpected (error)
    expected(const unexpected<E>& unexp)
        : error_(unexp.error()), has_value_(false) {}

    expected(unexpected<E>&& unexp) noexcept
        : error_(std::move(unexp).error()), has_value_(false) {}

    // In-place value construction
    template <typename... Args,
              typename = std::enable_if_t<std::is_constructible_v<T, Args...>>>
    explicit expected(std::in_place_t, Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, Args...>)
        : value_(std::forward<Args>(args)...), has_value_(true) {}

    template <typename U, typename... Args>
    explicit expected(std::in_place_t,
                       std::initializer_list<U> ilist,
                       Args&&... args)
        : value_(ilist, std::forward<Args>(args)...), has_value_(true) {}

    // Copy constructor
    expected(const expected& other)
        noexcept(std::is_nothrow_copy_constructible_v<T> &&
                  std::is_nothrow_copy_constructible_v<E>) {
        if (other.has_value_) {
            ::new (&value_) T(other.value_);
        } else {
            ::new (&error_) E(other.error_);
        }
        has_value_ = other.has_value_;
    }

    // Move constructor
    expected(expected&& other)
        noexcept(std::is_nothrow_move_constructible_v<T> &&
                  std::is_nothrow_move_constructible_v<E>) {
        if (other.has_value_) {
            ::new (&value_) T(std::move(other.value_));
        } else {
            ::new (&error_) E(std::move(other.error_));
        }
        has_value_ = other.has_value_;
    }

    // Destructor
    ~expected() {
        if (has_value_) {
            value_.~T();
        } else {
            error_.~E();
        }
    }

    // ============================================================
    // Assignment
    // ============================================================

    expected& operator=(const expected& other)
        noexcept(std::is_nothrow_copy_constructible_v<T> &&
                  std::is_nothrow_copy_constructible_v<E> &&
                  std::is_nothrow_copy_assignable_v<T> &&
                  std::is_nothrow_copy_assignable_v<E>) {
        if (this == &other) return *this;
        if (has_value_ && other.has_value_) {
            value_ = other.value_;
        } else if (!has_value_ && !other.has_value_) {
            error_ = other.error_;
        } else if (has_value_) {
            value_.~T();
            ::new (&error_) E(other.error_);
            has_value_ = false;
        } else {
            error_.~E();
            ::new (&value_) T(other.value_);
            has_value_ = true;
        }
        return *this;
    }

    expected& operator=(expected&& other)
        noexcept(std::is_nothrow_move_constructible_v<T> &&
                  std::is_nothrow_move_constructible_v<E> &&
                  std::is_nothrow_move_assignable_v<T> &&
                  std::is_nothrow_move_assignable_v<E>) {
        if (this == &other) return *this;
        if (has_value_ && other.has_value_) {
            value_ = std::move(other.value_);
        } else if (!has_value_ && !other.has_value_) {
            error_ = std::move(other.error_);
        } else if (has_value_) {
            value_.~T();
            ::new (&error_) E(std::move(other.error_));
            has_value_ = false;
        } else {
            error_.~E();
            ::new (&value_) T(std::move(other.value_));
            has_value_ = true;
        }
        return *this;
    }

    expected& operator=(const T& v) {
        if (has_value_) {
            value_ = v;
        } else {
            error_.~E();
            ::new (&value_) T(v);
            has_value_ = true;
        }
        return *this;
    }

    expected& operator=(T&& v) noexcept {
        if (has_value_) {
            value_ = std::move(v);
        } else {
            error_.~E();
            ::new (&value_) T(std::move(v));
            has_value_ = true;
        }
        return *this;
    }

    expected& operator=(const unexpected<E>& unexp) {
        if (!has_value_) {
            error_ = unexp.error();
        } else {
            value_.~T();
            ::new (&error_) E(unexp.error());
            has_value_ = false;
        }
        return *this;
    }

    expected& operator=(unexpected<E>&& unexp) noexcept {
        if (!has_value_) {
            error_ = std::move(unexp).error();
        } else {
            value_.~T();
            ::new (&error_) E(std::move(unexp).error());
            has_value_ = false;
        }
        return *this;
    }

    // ============================================================
    // Observers
    // ============================================================

    bool has_value() const noexcept { return has_value_; }
    explicit operator bool() const noexcept { return has_value_; }

    // Access the value (UB if !has_value())
    T& operator*() & noexcept {
        ZERO_ASSERT(has_value_);
        return value_;
    }

    const T& operator*() const& noexcept {
        ZERO_ASSERT(has_value_);
        return value_;
    }

    T&& operator*() && noexcept {
        ZERO_ASSERT(has_value_);
        return std::move(value_);
    }

    const T&& operator*() const&& noexcept {
        ZERO_ASSERT(has_value_);
        return std::move(value_);
    }

    T* operator->() noexcept {
        ZERO_ASSERT(has_value_);
        return &value_;
    }

    const T* operator->() const noexcept {
        ZERO_ASSERT(has_value_);
        return &value_;
    }

    // Access the value (throws bad_expected_access<E> if !has_value())
    T& value() & {
        if (!has_value_) {
            throw bad_expected_access<E>(error_);
        }
        return value_;
    }

    const T& value() const& {
        if (!has_value_) {
            throw bad_expected_access<E>(error_);
        }
        return value_;
    }

    T&& value() && {
        if (!has_value_) {
            throw bad_expected_access<E>(std::move(error_));
        }
        return std::move(value_);
    }

    const T&& value() const&& {
        if (!has_value_) {
            throw bad_expected_access<E>(std::move(error_));
        }
        return std::move(value_);
    }

    // Access the error (UB if has_value())
    E& error() & noexcept {
        ZERO_ASSERT(!has_value_);
        return error_;
    }

    const E& error() const& noexcept {
        ZERO_ASSERT(!has_value_);
        return error_;
    }

    E&& error() && noexcept {
        ZERO_ASSERT(!has_value_);
        return std::move(error_);
    }

    const E&& error() const&& noexcept {
        ZERO_ASSERT(!has_value_);
        return std::move(error_);
    }

    // Get the value, or a fallback
    template <typename U>
    T value_or(U&& default_value) const& {
        return has_value_ ? value_ : static_cast<T>(std::forward<U>(default_value));
    }

    template <typename U>
    T value_or(U&& default_value) && {
        return has_value_ ? std::move(value_) : static_cast<T>(std::forward<U>(default_value));
    }

    // ============================================================
    // Monadic operations
    // ============================================================

    // and_then: if has_value, invoke f(value) returning expected<U, E>;
    // otherwise propagate the error.
    template <typename F>
    auto and_then(F&& f) & -> std::invoke_result_t<F, T&> {
        using result_type = std::invoke_result_t<F, T&>;
        if (has_value_) {
            return std::invoke(std::forward<F>(f), value_);
        }
        return result_type(unexpected<E>(error_));
    }

    template <typename F>
    auto and_then(F&& f) const& -> std::invoke_result_t<F, const T&> {
        using result_type = std::invoke_result_t<F, const T&>;
        if (has_value_) {
            return std::invoke(std::forward<F>(f), value_);
        }
        return result_type(unexpected<E>(error_));
    }

    template <typename F>
    auto and_then(F&& f) && -> std::invoke_result_t<F, T&&> {
        using result_type = std::invoke_result_t<F, T&&>;
        if (has_value_) {
            return std::invoke(std::forward<F>(f), std::move(value_));
        }
        return result_type(unexpected<E>(std::move(error_)));
    }

    // or_else: if has_error, invoke f(error) returning expected<T, E2>;
    // otherwise propagate the value.
    template <typename F>
    auto or_else(F&& f) & -> std::invoke_result_t<F, E&> {
        using result_type = std::invoke_result_t<F, E&>;
        if (!has_value_) {
            return std::invoke(std::forward<F>(f), error_);
        }
        return result_type(std::in_place, value_);
    }

    template <typename F>
    auto or_else(F&& f) const& -> std::invoke_result_t<F, const E&> {
        using result_type = std::invoke_result_t<F, const E&>;
        if (!has_value_) {
            return std::invoke(std::forward<F>(f), error_);
        }
        return result_type(std::in_place, value_);
    }

    template <typename F>
    auto or_else(F&& f) && -> std::invoke_result_t<F, E&&> {
        using result_type = std::invoke_result_t<F, E&&>;
        if (!has_value_) {
            return std::invoke(std::forward<F>(f), std::move(error_));
        }
        return result_type(std::in_place, std::move(value_));
    }

    // transform: if has_value, apply f(value) and wrap in expected<U, E>;
    // otherwise propagate the error.
    template <typename F>
    auto transform(F&& f) & -> expected<
        std::remove_cv_t<std::invoke_result_t<F, T&>>,
        E> {
        using U = std::remove_cv_t<std::invoke_result_t<F, T&>>;
        if (has_value_) {
            return expected<U, E>(
                std::invoke(std::forward<F>(f), value_));
        }
        return expected<U, E>(unexpected<E>(error_));
    }

    template <typename F>
    auto transform(F&& f) const& -> expected<
        std::remove_cv_t<std::invoke_result_t<F, const T&>>,
        E> {
        using U = std::remove_cv_t<std::invoke_result_t<F, const T&>>;
        if (has_value_) {
            return expected<U, E>(
                std::invoke(std::forward<F>(f), value_));
        }
        return expected<U, E>(unexpected<E>(error_));
    }

    template <typename F>
    auto transform(F&& f) && -> expected<
        std::remove_cv_t<std::invoke_result_t<F, T&&>>,
        E> {
        using U = std::remove_cv_t<std::invoke_result_t<F, T&&>>;
        if (has_value_) {
            return expected<U, E>(
                std::invoke(std::forward<F>(f), std::move(value_)));
        }
        return expected<U, E>(unexpected<E>(std::move(error_)));
    }

    // transform_error: if has_error, apply f(error) and wrap in
    // expected<T, E2>; otherwise propagate the value.
    template <typename F>
    auto transform_error(F&& f) & -> expected<
        T,
        std::remove_cv_t<std::invoke_result_t<F, E&>>> {
        using E2 = std::remove_cv_t<std::invoke_result_t<F, E&>>;
        if (!has_value_) {
            return expected<T, E2>(
                unexpected<E2>(std::invoke(std::forward<F>(f), error_)));
        }
        return expected<T, E2>(std::in_place, value_);
    }

    template <typename F>
    auto transform_error(F&& f) const& -> expected<
        T,
        std::remove_cv_t<std::invoke_result_t<F, const E&>>> {
        using E2 = std::remove_cv_t<std::invoke_result_t<F, const E&>>;
        if (!has_value_) {
            return expected<T, E2>(
                unexpected<E2>(std::invoke(std::forward<F>(f), error_)));
        }
        return expected<T, E2>(std::in_place, value_);
    }

    template <typename F>
    auto transform_error(F&& f) && -> expected<
        T,
        std::remove_cv_t<std::invoke_result_t<F, E&&>>> {
        using E2 = std::remove_cv_t<std::invoke_result_t<F, E&&>>;
        if (!has_value_) {
            return expected<T, E2>(
                unexpected<E2>(std::invoke(std::forward<F>(f),
                                            std::move(error_))));
        }
        return expected<T, E2>(std::in_place, std::move(value_));
    }

    // ============================================================
    // Swap
    // ============================================================

    void swap(expected& other)
        noexcept(std::is_nothrow_move_constructible_v<T> &&
                  std::is_nothrow_swappable_v<T> &&
                  std::is_nothrow_move_constructible_v<E> &&
                  std::is_nothrow_swappable_v<E>) {
        if (has_value_ && other.has_value_) {
            using std::swap;
            swap(value_, other.value_);
        } else if (!has_value_ && !other.has_value_) {
            using std::swap;
            swap(error_, other.error_);
        } else if (has_value_) {
            // This has value, other has error
            E tmp(std::move(other.error_));
            other.error_.~E();
            ::new (&other.value_) T(std::move(value_));
            value_.~T();
            ::new (&error_) E(std::move(tmp));
            std::swap(has_value_, other.has_value_);
        } else {
            other.swap(*this);
        }
    }

    // ============================================================
    // Equality comparison
    // ============================================================

    friend bool operator==(const expected& lhs, const expected& rhs) {
        if (lhs.has_value_ != rhs.has_value_) return false;
        if (lhs.has_value_) return lhs.value_ == rhs.value_;
        return lhs.error_ == rhs.error_;
    }

    friend bool operator!=(const expected& lhs, const expected& rhs) {
        return !(lhs == rhs);
    }

    // Compare with value
    friend bool operator==(const expected& exp, const T& val) {
        return exp.has_value_ && exp.value_ == val;
    }

    friend bool operator==(const T& val, const expected& exp) {
        return exp == val;
    }

    friend bool operator!=(const expected& exp, const T& val) {
        return !(exp == val);
    }

    friend bool operator!=(const T& val, const expected& exp) {
        return !(exp == val);
    }

    // Compare with unexpected
    friend bool operator==(const expected& exp, const unexpected<E>& unexp) {
        return !exp.has_value_ && exp.error_ == unexp.error();
    }

    friend bool operator==(const unexpected<E>& unexp, const expected& exp) {
        return exp == unexp;
    }

    friend bool operator!=(const expected& exp, const unexpected<E>& unexp) {
        return !(exp == unexp);
    }

    friend bool operator!=(const unexpected<E>& unexp, const expected& exp) {
        return !(exp == unexp);
    }
};

} // namespace zero

namespace std {
template <typename T, typename E>
void swap(zero::expected<T, E>& lhs,
           zero::expected<T, E>& rhs) noexcept(
               noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}
} // namespace std
