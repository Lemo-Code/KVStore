// zero NonCopyable, NonMovable, and move-only wrapper
// Provides standard RAII ownership semantics for resource classes
#pragma once

#include <utility>
namespace zero {

// Base class to disable copy semantics while leaving move enabled.
// Inherit publicly (or privately) from this to mark a type as non-copyable.
class Noncopyable {
public:
    Noncopyable() = default;
    ~Noncopyable() = default;

    // Delete copy
    Noncopyable(const Noncopyable&) = delete;
    Noncopyable& operator=(const Noncopyable&) = delete;

    // Default move
    Noncopyable(Noncopyable&&) noexcept = default;
    Noncopyable& operator=(Noncopyable&&) noexcept = default;
};

// Base class to disable move semantics while leaving copy enabled.
// Useful for types whose address stability is required (e.g. mutex-like objects).
class Nonmovable {
public:
    Nonmovable() = default;
    ~Nonmovable() = default;

    // Default copy
    Nonmovable(const Nonmovable&) = default;
    Nonmovable& operator=(const Nonmovable&) = default;

    // Delete move
    Nonmovable(Nonmovable&&) noexcept = delete;
    Nonmovable& operator=(Nonmovable&&) noexcept = delete;
};

// Disables both copy and move — the object is pinned at its construction site.
class NoncopyableNonmovable {
public:
    NoncopyableNonmovable() = default;
    ~NoncopyableNonmovable() = default;

    NoncopyableNonmovable(const NoncopyableNonmovable&) = delete;
    NoncopyableNonmovable& operator=(const NoncopyableNonmovable&) = delete;
    NoncopyableNonmovable(NoncopyableNonmovable&&) noexcept = delete;
    NoncopyableNonmovable& operator=(NoncopyableNonmovable&&) noexcept = delete;
};

// Movable: a move-only wrapper around a value type T.
// Useful for wrapping RAII handles (file descriptors, pointers) so that
// they can be moved but not accidentally copied.
template <typename T>
class Movable {
public:
    Movable() noexcept : value_() {}
    explicit Movable(T value) noexcept : value_(std::move(value)) {}

    Movable(const Movable&) = delete;
    Movable& operator=(const Movable&) = delete;

    Movable(Movable&& other) noexcept
        : value_(std::move(other.value_)) {
        other.value_ = T{};
    }

    Movable& operator=(Movable&& other) noexcept {
        if (this != &other) {
            value_ = std::move(other.value_);
            other.value_ = T{};
        }
        return *this;
    }

    ~Movable() = default;

    T& get() noexcept { return value_; }
    const T& get() const noexcept { return value_; }

    // Allow implicit conversion to underlying T reference
    operator T&() noexcept { return value_; }
    operator const T&() const noexcept { return value_; }

    T* operator->() noexcept { return &value_; }
    const T* operator->() const noexcept { return &value_; }

    T& operator*() noexcept { return value_; }
    const T& operator*() const noexcept { return value_; }

    void reset(T value = T{}) noexcept { value_ = std::move(value); }
    T release() noexcept {
        T v = std::move(value_);
        value_ = T{};
        return v;
    }

private:
    T value_;
};

} // namespace zero
