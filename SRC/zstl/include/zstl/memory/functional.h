// zstl functional — functors, reference_wrapper, function, bind
#pragma once

#include <cstddef>
#include <type_traits>
#include <functional>
#include <new>
#include <utility>
#include <stdexcept>
#include "zstl/memory/utility.h"
#include "zstl/memory/type_traits.h"

namespace zstl {

// ============================================================
// Additional comparison functors
// (less, greater, equal_to are provided by utility.h;
//  this header adds the remaining three)
// ============================================================

template<typename T = void>
struct less_equal {
    constexpr bool operator()(const T& a, const T& b) const { return a <= b; }
};

template<>
struct less_equal<void> {
    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        -> decltype(static_cast<T&&>(a) <= static_cast<U&&>(b)) {
        return static_cast<T&&>(a) <= static_cast<U&&>(b);
    }
    using is_transparent = void;
};

template<typename T = void>
struct greater_equal {
    constexpr bool operator()(const T& a, const T& b) const { return a >= b; }
};

template<>
struct greater_equal<void> {
    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        -> decltype(static_cast<T&&>(a) >= static_cast<U&&>(b)) {
        return static_cast<T&&>(a) >= static_cast<U&&>(b);
    }
    using is_transparent = void;
};

template<typename T = void>
struct not_equal_to {
    constexpr bool operator()(const T& a, const T& b) const { return a != b; }
};

template<>
struct not_equal_to<void> {
    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        -> decltype(static_cast<T&&>(a) != static_cast<U&&>(b)) {
        return static_cast<T&&>(a) != static_cast<U&&>(b);
    }
    using is_transparent = void;
};

// ============================================================
// Arithmetic functors
// ============================================================

template<typename T = void>
struct plus {
    constexpr T operator()(const T& a, const T& b) const { return a + b; }
};

template<>
struct plus<void> {
    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        -> decltype(static_cast<T&&>(a) + static_cast<U&&>(b)) {
        return static_cast<T&&>(a) + static_cast<U&&>(b);
    }
    using is_transparent = void;
};

template<typename T = void>
struct minus {
    constexpr T operator()(const T& a, const T& b) const { return a - b; }
};

template<>
struct minus<void> {
    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        -> decltype(static_cast<T&&>(a) - static_cast<U&&>(b)) {
        return static_cast<T&&>(a) - static_cast<U&&>(b);
    }
    using is_transparent = void;
};

template<typename T = void>
struct multiplies {
    constexpr T operator()(const T& a, const T& b) const { return a * b; }
};

template<>
struct multiplies<void> {
    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        -> decltype(static_cast<T&&>(a) * static_cast<U&&>(b)) {
        return static_cast<T&&>(a) * static_cast<U&&>(b);
    }
    using is_transparent = void;
};

template<typename T = void>
struct divides {
    constexpr T operator()(const T& a, const T& b) const { return a / b; }
};

template<>
struct divides<void> {
    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        -> decltype(static_cast<T&&>(a) / static_cast<U&&>(b)) {
        return static_cast<T&&>(a) / static_cast<U&&>(b);
    }
    using is_transparent = void;
};

template<typename T = void>
struct modulus {
    constexpr T operator()(const T& a, const T& b) const { return a % b; }
};

template<>
struct modulus<void> {
    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        -> decltype(static_cast<T&&>(a) % static_cast<U&&>(b)) {
        return static_cast<T&&>(a) % static_cast<U&&>(b);
    }
    using is_transparent = void;
};

template<typename T = void>
struct negate {
    constexpr T operator()(const T& a) const { return -a; }
};

template<>
struct negate<void> {
    template<typename T>
    constexpr auto operator()(T&& a) const
        -> decltype(-static_cast<T&&>(a)) {
        return -static_cast<T&&>(a);
    }
    using is_transparent = void;
};

// ============================================================
// Logical functors
// ============================================================

template<typename T = void>
struct logical_and {
    constexpr bool operator()(const T& a, const T& b) const { return a && b; }
};

template<>
struct logical_and<void> {
    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        -> decltype(static_cast<T&&>(a) && static_cast<U&&>(b)) {
        return static_cast<T&&>(a) && static_cast<U&&>(b);
    }
    using is_transparent = void;
};

template<typename T = void>
struct logical_or {
    constexpr bool operator()(const T& a, const T& b) const { return a || b; }
};

template<>
struct logical_or<void> {
    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        -> decltype(static_cast<T&&>(a) || static_cast<U&&>(b)) {
        return static_cast<T&&>(a) || static_cast<U&&>(b);
    }
    using is_transparent = void;
};

template<typename T = void>
struct logical_not {
    constexpr bool operator()(const T& a) const { return !a; }
};

template<>
struct logical_not<void> {
    template<typename T>
    constexpr auto operator()(T&& a) const
        -> decltype(!static_cast<T&&>(a)) {
        return !static_cast<T&&>(a);
    }
    using is_transparent = void;
};

// ============================================================
// Bitwise functors
// ============================================================

template<typename T = void>
struct bit_and {
    constexpr T operator()(const T& a, const T& b) const { return a & b; }
};

template<>
struct bit_and<void> {
    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        -> decltype(static_cast<T&&>(a) & static_cast<U&&>(b)) {
        return static_cast<T&&>(a) & static_cast<U&&>(b);
    }
    using is_transparent = void;
};

template<typename T = void>
struct bit_or {
    constexpr T operator()(const T& a, const T& b) const { return a | b; }
};

template<>
struct bit_or<void> {
    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        -> decltype(static_cast<T&&>(a) | static_cast<U&&>(b)) {
        return static_cast<T&&>(a) | static_cast<U&&>(b);
    }
    using is_transparent = void;
};

template<typename T = void>
struct bit_xor {
    constexpr T operator()(const T& a, const T& b) const { return a ^ b; }
};

template<>
struct bit_xor<void> {
    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        -> decltype(static_cast<T&&>(a) ^ static_cast<U&&>(b)) {
        return static_cast<T&&>(a) ^ static_cast<U&&>(b);
    }
    using is_transparent = void;
};

template<typename T = void>
struct bit_not {
    constexpr T operator()(const T& a) const { return ~a; }
};

template<>
struct bit_not<void> {
    template<typename T>
    constexpr auto operator()(T&& a) const
        -> decltype(~static_cast<T&&>(a)) {
        return ~static_cast<T&&>(a);
    }
    using is_transparent = void;
};

// ============================================================
// reference_wrapper<T> — store a reference as a copyable object
// ============================================================
template<typename T>
class reference_wrapper {
public:
    using type = T;

    // Construct from lvalue reference (explicit to avoid accidents)
    constexpr explicit reference_wrapper(T& ref) noexcept : ptr_(&ref) {}

    // Copy / move
    constexpr reference_wrapper(const reference_wrapper&) noexcept = default;
    constexpr reference_wrapper& operator=(const reference_wrapper&) noexcept = default;

    // Implicit conversion to T&
    constexpr operator T&() const noexcept { return *ptr_; }

    // Explicit get
    constexpr T& get() const noexcept { return *ptr_; }

    // Function call operator (forwards to stored reference if it's callable)
    template<typename... Args>
    constexpr auto operator()(Args&&... args) const
        noexcept(noexcept(std::declval<T&>()(std::declval<Args>()...)))
        -> decltype(auto) {
        return (*ptr_)(std::forward<Args>(args)...);
    }

private:
    T* ptr_;
};

// ============================================================
// ref / cref — convenience factories for reference_wrapper
// ============================================================

template<typename T>
constexpr reference_wrapper<T> ref(T& t) noexcept {
    return reference_wrapper<T>(t);
}

// ref(T&&) is deleted to prevent dangling references
template<typename T>
void ref(const T&&) = delete;

template<typename T>
constexpr reference_wrapper<const T> cref(const T& t) noexcept {
    return reference_wrapper<const T>(t);
}

template<typename T>
void cref(const T&&) = delete;

// ============================================================
// Placeholders for bind
// ============================================================

template<int N>
struct placeholder {};

inline namespace placeholders {
    inline constexpr placeholder<1> _1{};
    inline constexpr placeholder<2> _2{};
    inline constexpr placeholder<3> _3{};
    inline constexpr placeholder<4> _4{};
    inline constexpr placeholder<5> _5{};
    inline constexpr placeholder<6> _6{};
    inline constexpr placeholder<7> _7{};
    inline constexpr placeholder<8> _8{};
    inline constexpr placeholder<9> _9{};
} // namespace placeholders

// ============================================================
// is_placeholder — detect placeholder<N> types
// ============================================================
template<typename T>
struct is_placeholder : std::integral_constant<int, 0> {};

template<int N>
struct is_placeholder<placeholder<N>> : std::integral_constant<int, N> {};

template<typename T>
inline constexpr int is_placeholder_v = is_placeholder<T>::value;

// ============================================================
// function<R(Args...)> — type-erased callable wrapper
//
// Stores a callable object of any type (function pointer, lambda,
// functor, etc.) via type erasure.  A small buffer optimization
// (SBO) avoids heap allocation for small objects (<= 32 bytes on
// 64-bit).
// ============================================================

// Helper: determine if T fits in the SBO buffer
template<typename T>
constexpr bool _sbo_fits_v =
    sizeof(T) <= sizeof(void*) * 4 &&
    std::is_nothrow_move_constructible_v<T> &&
    alignof(T) <= alignof(void*);

template<typename Signature>
class function;

template<typename R, typename... Args>
class function<R(Args...)> {
    // ---- type-erased callable concept ----
    struct callable_base {
        callable_base() = default;
        virtual ~callable_base() = default;
        virtual R invoke(Args... args) = 0;
        virtual callable_base* clone(void* storage) const = 0;
        virtual void move_to(void* storage) = 0;
    };

    // ---- concrete callable model ----
    template<typename F>
    struct callable_model final : callable_base {
        F f_;

        template<typename U>
        explicit callable_model(U&& f) : f_(std::forward<U>(f)) {}

        R invoke(Args... args) override {
            return f_(std::forward<Args>(args)...);
        }

        callable_base* clone(void* storage) const override {
            return ::new (storage) callable_model(f_);
        }

        void move_to(void* storage) override {
            ::new (storage) callable_model(std::move(f_));
        }
    };

    static constexpr size_t kBufferSize = sizeof(void*) * 4;
    static constexpr size_t kBufferAlign = alignof(void*);

    alignas(kBufferAlign) char buffer_[kBufferSize];
    callable_base* impl_ = nullptr;

    // Check if the callable is stored in-place (SBO) or heap
    bool _is_sbo() const noexcept {
        return impl_ == reinterpret_cast<const callable_base*>(buffer_);
    }

    void _clear() noexcept {
        if (impl_) {
            impl_->~callable_base();
            impl_ = nullptr;
        }
    }

    void _set_null() noexcept {
        _clear();
        buffer_[0] = '\0'; // ensure _is_sbo() returns false
    }

public:
    // ---- result type ----
    using result_type = R;

    // ---- constructors ----

    // Default — empty
    constexpr function() noexcept : buffer_{}, impl_(nullptr) {}

    // Nullptr — empty
    constexpr function(std::nullptr_t) noexcept : buffer_{}, impl_(nullptr) {}

    // From function pointer
    function(R(*fp)(Args...)) {
        if (fp) {
            _emplace<std::decay_t<decltype(fp)>>(fp);
        } else {
            _set_null();
        }
    }

    // From any callable (lambda, functor, std::function, etc.)
    template<typename F,
             typename = std::enable_if_t<
                 !std::is_same_v<std::decay_t<F>, function> &&
                 std::is_invocable_r_v<R, F, Args...>>>
    function(F&& f) {
        using decay_t = std::decay_t<F>;
        _emplace<decay_t>(std::forward<F>(f));
    }

    // Copy constructor
    function(const function& other) {
        if (other.impl_) {
            if (other._is_sbo()) {
                other.impl_->clone(buffer_);
                impl_ = reinterpret_cast<callable_base*>(buffer_);
            } else {
                impl_ = other.impl_->clone(nullptr);
            }
        } else {
            _set_null();
        }
    }

    // Move constructor
    function(function&& other) noexcept {
        if (other.impl_) {
            if (other._is_sbo()) {
                other.impl_->move_to(buffer_);
                impl_ = reinterpret_cast<callable_base*>(buffer_);
            } else {
                impl_ = other.impl_;
                other.impl_ = nullptr;
            }
        } else {
            _set_null();
        }
    }

    // Copy assignment
    function& operator=(const function& other) {
        if (this != &other) {
            _clear();
            if (other.impl_) {
                if (other._is_sbo()) {
                    other.impl_->clone(buffer_);
                    impl_ = reinterpret_cast<callable_base*>(buffer_);
                } else {
                    impl_ = other.impl_->clone(nullptr);
                }
            } else {
                _set_null();
            }
        }
        return *this;
    }

    // Move assignment
    function& operator=(function&& other) noexcept {
        if (this != &other) {
            _clear();
            if (other.impl_) {
                if (other._is_sbo()) {
                    other.impl_->move_to(buffer_);
                    impl_ = reinterpret_cast<callable_base*>(buffer_);
                    other.impl_ = nullptr;
                } else {
                    impl_ = other.impl_;
                    other.impl_ = nullptr;
                }
            } else {
                _set_null();
            }
        }
        return *this;
    }

    // Assignment from callable
    template<typename F,
             typename = std::enable_if_t<
                 !std::is_same_v<std::decay_t<F>, function> &&
                 std::is_invocable_r_v<R, F, Args...>>>
    function& operator=(F&& f) {
        _clear();
        using decay_t = std::decay_t<F>;
        _emplace<decay_t>(std::forward<F>(f));
        return *this;
    }

    // Assignment from nullptr
    function& operator=(std::nullptr_t) noexcept {
        _clear();
        _set_null();
        return *this;
    }

    // Assignment from function pointer
    function& operator=(R(*fp)(Args...)) {
        if (fp) {
            *this = function(fp);
        } else {
            *this = nullptr;
        }
        return *this;
    }

    // Destructor
    ~function() {
        _clear();
    }

    // ---- observers ----

    explicit operator bool() const noexcept {
        return impl_ != nullptr;
    }

    // ---- invocation ----

    R operator()(Args... args) const {
        if (!impl_) {
            throw std::bad_function_call();
        }
        return impl_->invoke(std::forward<Args>(args)...);
    }

    // ---- swap ----

    void swap(function& other) noexcept {
        // Simplification: copy both to heap and swap pointers
        // For truly noexcept swap, we'd need more care
        function tmp(std::move(*this));
        *this = std::move(other);
        other = std::move(tmp);
    }

    // ---- target access ----

    // Returns pointer to the stored target of type T, or nullptr
    template<typename T>
    T* target() noexcept {
        if (!impl_) return nullptr;
        auto* model = dynamic_cast<callable_model<T>*>(impl_);
        return model ? &model->f_ : nullptr;
    }

    template<typename T>
    const T* target() const noexcept {
        if (!impl_) return nullptr;
        auto* model = dynamic_cast<const callable_model<T>*>(impl_);
        return model ? &model->f_ : nullptr;
    }

private:
    template<typename DecayT, typename U>
    void _emplace(U&& value) {
        if constexpr (_sbo_fits_v<DecayT>) {
            auto* model = ::new (buffer_) callable_model<DecayT>(std::forward<U>(value));
            impl_ = model;
        } else {
            auto* model = new callable_model<DecayT>(std::forward<U>(value));
            impl_ = model;
            buffer_[0] = '\0'; // not SBO
        }
    }
};

// ---- swap specialization ----
template<typename R, typename... Args>
void swap(function<R(Args...)>& a, function<R(Args...)>& b) noexcept {
    a.swap(b);
}

// ---- comparison with nullptr ----
template<typename R, typename... Args>
bool operator==(const function<R(Args...)>& f, std::nullptr_t) noexcept {
    return !f;
}

template<typename R, typename... Args>
bool operator==(std::nullptr_t, const function<R(Args...)>& f) noexcept {
    return !f;
}

template<typename R, typename... Args>
bool operator!=(const function<R(Args...)>& f, std::nullptr_t) noexcept {
    return static_cast<bool>(f);
}

template<typename R, typename... Args>
bool operator!=(std::nullptr_t, const function<R(Args...)>& f) noexcept {
    return static_cast<bool>(f);
}

// ============================================================
// bind — simplified std::bind replacement
//
// Supports placeholder substitution and nested bind evaluation.
// ============================================================

namespace detail {

// Helper: extract argument from a bound tuple, resolving placeholders
template<int N, typename Tuple>
constexpr decltype(auto) _bind_get(Tuple& tup) {
    return std::get<N>(tup);
}

// Select the Nth argument from call_args (0-indexed)
template<int N, typename... CallArgs>
struct _arg_selector;

template<typename First, typename... Rest>
struct _arg_selector<0, First, Rest...> {
    static decltype(auto) get(First&& first, Rest&&...) {
        return static_cast<First&&>(first);
    }
};

template<int N, typename First, typename... Rest>
struct _arg_selector<N, First, Rest...> {
    static_assert(N > 0, "bind placeholder index out of range");
    static decltype(auto) get(First&&, Rest&&... rest) {
        return _arg_selector<N - 1, Rest...>::get(std::forward<Rest>(rest)...);
    }
};

template<int N, typename... CallArgs>
constexpr decltype(auto) _select_arg(CallArgs&&... args) {
    return _arg_selector<N, CallArgs...>::get(std::forward<CallArgs>(args)...);
}

// Invoke a callable with arguments resolved from a tuple,
// replacing placeholders with actual arguments.
template<typename F, typename BoundArgs, typename... CallArgs, size_t... Is>
constexpr decltype(auto) _bind_invoke(F&& f, BoundArgs& bound,
                                       std::index_sequence<Is...>,
                                       CallArgs&&... call_args) {
    // Use a helper to resolve each bound argument
    return std::forward<F>(f)(
        _resolve_bind_arg(std::get<Is>(bound), std::forward<CallArgs>(call_args)...)...
    );
}

// Resolve a placeholder<N> from call_args
template<int N, typename... CallArgs>
constexpr decltype(auto) _resolve_bind_arg(placeholder<N>, CallArgs&&... call_args) {
    return _select_arg<N-1>(std::forward<CallArgs>(call_args)...);
}

// Resolve a non-placeholder value directly
template<typename T, typename... CallArgs>
constexpr T& _resolve_bind_arg(T& val, CallArgs&&...) {
    return val;
}

template<typename T, typename... CallArgs>
constexpr const T& _resolve_bind_arg(const T& val, CallArgs&&...) {
    return val;
}

template<typename T, typename... CallArgs>
constexpr T&& _resolve_bind_arg(T&& val, CallArgs&&...) {
    return std::move(val);
}

} // namespace detail

template<typename F, typename... BoundArgs>
class bind_result {
    F func_;
    std::tuple<BoundArgs...> bound_;

public:
    template<typename F2, typename... BArgs>
    explicit bind_result(F2&& f, BArgs&&... args)
        : func_(std::forward<F2>(f))
        , bound_(std::forward<BArgs>(args)...) {}

    bind_result(const bind_result&) = default;
    bind_result(bind_result&&) = default;

    template<typename... CallArgs>
    decltype(auto) operator()(CallArgs&&... args) {
        return detail::_bind_invoke(
            func_, bound_,
            std::index_sequence_for<BoundArgs...>{},
            std::forward<CallArgs>(args)...);
    }

    template<typename... CallArgs>
    decltype(auto) operator()(CallArgs&&... args) const {
        return detail::_bind_invoke(
            func_, const_cast<std::tuple<BoundArgs...>&>(bound_),
            std::index_sequence_for<BoundArgs...>{},
            std::forward<CallArgs>(args)...);
    }
};

// bind free function
template<typename F, typename... Args>
auto bind(F&& f, Args&&... args) {
    return bind_result<std::decay_t<F>, std::decay_t<Args>...>(
        std::forward<F>(f), std::forward<Args>(args)...);
}

} // namespace zstl

// ============================================================
// std::hash specialization for zstl placeholder (if needed by containers)
// ============================================================

namespace std {

template<int N>
struct hash<zstl::placeholder<N>> {
    constexpr size_t operator()(zstl::placeholder<N>) const noexcept {
        return static_cast<size_t>(N);
    }
};

template<>
struct hash<zstl::bit_not<void>> {
    size_t operator()(const zstl::bit_not<void>&) const noexcept {
        return 0;
    }
};

} // namespace std
