// zstl mutex — synchronization primitives wrapping pthread (Linux) or std::mutex
#pragma once

#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <system_error>
#include <new>

namespace zstl {

// ============================================================
// mutex — non-recursive mutual exclusion
//
// Thin wrapper around std::mutex.  Not copyable or movable.
// Locking an already-locked mutex from the same thread is
// undefined behavior.
// ============================================================
class mutex {
public:
    using native_handle_type = std::mutex::native_handle_type;

    constexpr mutex() noexcept = default;

    mutex(const mutex&) = delete;
    mutex& operator=(const mutex&) = delete;
    mutex(mutex&&) = delete;
    mutex& operator=(mutex&&) = delete;

    void lock() { impl_.lock(); }
    bool try_lock() noexcept { return impl_.try_lock(); }
    void unlock() { impl_.unlock(); }

    native_handle_type native_handle() noexcept { return impl_.native_handle(); }

    // Internal access for condition variable interop
    std::mutex& _impl() noexcept { return impl_; }

private:
    std::mutex impl_;
};

// ============================================================
// recursive_mutex — re-entrant mutual exclusion
// ============================================================
class recursive_mutex {
public:
    using native_handle_type = std::recursive_mutex::native_handle_type;

    constexpr recursive_mutex() noexcept = default;

    recursive_mutex(const recursive_mutex&) = delete;
    recursive_mutex& operator=(const recursive_mutex&) = delete;
    recursive_mutex(recursive_mutex&&) = delete;
    recursive_mutex& operator=(recursive_mutex&&) = delete;

    void lock() { impl_.lock(); }
    bool try_lock() noexcept { return impl_.try_lock(); }
    void unlock() { impl_.unlock(); }

    native_handle_type native_handle() noexcept { return impl_.native_handle(); }

    std::recursive_mutex& _impl() noexcept { return impl_; }

private:
    std::recursive_mutex impl_;
};

// ============================================================
// timed_mutex — mutex with timeout-based locking
// ============================================================
class timed_mutex {
public:
    using native_handle_type = std::timed_mutex::native_handle_type;

    constexpr timed_mutex() noexcept = default;

    timed_mutex(const timed_mutex&) = delete;
    timed_mutex& operator=(const timed_mutex&) = delete;
    timed_mutex(timed_mutex&&) = delete;
    timed_mutex& operator=(timed_mutex&&) = delete;

    void lock() { impl_.lock(); }
    bool try_lock() noexcept { return impl_.try_lock(); }

    template<typename Rep, typename Period>
    bool try_lock_for(const std::chrono::duration<Rep, Period>& d) {
        return impl_.try_lock_for(d);
    }

    template<typename Clock, typename Duration>
    bool try_lock_until(const std::chrono::time_point<Clock, Duration>& t) {
        return impl_.try_lock_until(t);
    }

    void unlock() { impl_.unlock(); }

    native_handle_type native_handle() noexcept { return impl_.native_handle(); }
    std::timed_mutex& _impl() noexcept { return impl_; }

private:
    std::timed_mutex impl_;
};

// ============================================================
// Tag types for unique_lock construction
// ============================================================

struct defer_lock_t  { explicit defer_lock_t() = default; };
struct try_to_lock_t { explicit try_to_lock_t() = default; };
struct adopt_lock_t  { explicit adopt_lock_t() = default; };

inline constexpr defer_lock_t  defer_lock{};
inline constexpr try_to_lock_t try_to_lock{};
inline constexpr adopt_lock_t  adopt_lock{};

// ============================================================
// lock_guard<Mutex> — simple RAII scoped lock
// ============================================================
template<typename Mutex>
class lock_guard {
public:
    using mutex_type = Mutex;

    explicit lock_guard(Mutex& m) : m_(&m) { m_->lock(); }

    lock_guard(Mutex& m, adopt_lock_t) noexcept : m_(&m) {}

    ~lock_guard() { if (m_) m_->unlock(); }

    lock_guard(const lock_guard&) = delete;
    lock_guard& operator=(const lock_guard&) = delete;
    lock_guard(lock_guard&&) = delete;
    lock_guard& operator=(lock_guard&&) = delete;

private:
    Mutex* m_;
};

// Deduction guide
template<typename Mutex>
lock_guard(Mutex&) -> lock_guard<Mutex>;

// ============================================================
// unique_lock<Mutex> — movable, flexibly-managed RAII lock
//
// Supports deferred, try, adopt, timed locking, early unlock,
// and move semantics.  Works as the lock parameter for
// condition_variable[_any].
// ============================================================
template<typename Mutex>
class unique_lock {
public:
    using mutex_type = Mutex;

    // ---- constructors ----

    unique_lock() noexcept : m_(nullptr), owns_(false) {}

    explicit unique_lock(Mutex& m)
        : m_(&m), owns_(false) { m_->lock(); owns_ = true; }

    unique_lock(Mutex& m, defer_lock_t) noexcept
        : m_(&m), owns_(false) {}

    unique_lock(Mutex& m, try_to_lock_t)
        : m_(&m), owns_(m.try_lock()) {}

    unique_lock(Mutex& m, adopt_lock_t) noexcept
        : m_(&m), owns_(true) {}

    template<typename Rep, typename Period>
    unique_lock(Mutex& m, const std::chrono::duration<Rep, Period>& d)
        : m_(&m), owns_(m.try_lock_for(d)) {}

    template<typename Clock, typename Duration>
    unique_lock(Mutex& m, const std::chrono::time_point<Clock, Duration>& t)
        : m_(&m), owns_(m.try_lock_until(t)) {}

    // Move constructor
    unique_lock(unique_lock&& other) noexcept
        : m_(other.m_), owns_(other.owns_) {
        other.m_ = nullptr;
        other.owns_ = false;
    }

    // Move assignment
    unique_lock& operator=(unique_lock&& other) noexcept {
        if (this != &other) {
            if (owns_) m_->unlock();
            m_ = other.m_;
            owns_ = other.owns_;
            other.m_ = nullptr;
            other.owns_ = false;
        }
        return *this;
    }

    // Non-copyable
    unique_lock(const unique_lock&) = delete;
    unique_lock& operator=(const unique_lock&) = delete;

    // Destructor
    ~unique_lock() {
        if (owns_) m_->unlock();
    }

    // ---- locking operations ----

    void lock() {
        _check_null();
        _check_not_owns();
        m_->lock();
        owns_ = true;
    }

    bool try_lock() {
        _check_null();
        _check_not_owns();
        owns_ = m_->try_lock();
        return owns_;
    }

    template<typename Rep, typename Period>
    bool try_lock_for(const std::chrono::duration<Rep, Period>& d) {
        _check_null();
        _check_not_owns();
        owns_ = m_->try_lock_for(d);
        return owns_;
    }

    template<typename Clock, typename Duration>
    bool try_lock_until(const std::chrono::time_point<Clock, Duration>& t) {
        _check_null();
        _check_not_owns();
        owns_ = m_->try_lock_until(t);
        return owns_;
    }

    void unlock() {
        if (!owns_)
            throw std::system_error(std::make_error_code(std::errc::operation_not_permitted),
                                    "zstl::unique_lock::unlock: not locked");
        m_->unlock();
        owns_ = false;
    }

    // ---- observers ----

    bool owns_lock() const noexcept { return owns_; }
    explicit operator bool() const noexcept { return owns_lock(); }

    Mutex* mutex() const noexcept { return m_; }

    // Release ownership without unlocking
    Mutex* release() noexcept {
        Mutex* tmp = m_;
        m_ = nullptr;
        owns_ = false;
        return tmp;
    }

    // Swap
    void swap(unique_lock& other) noexcept {
        std::swap(m_, other.m_);
        std::swap(owns_, other.owns_);
    }

    // Public data access for condition_variable interop
    Mutex* _m_ptr() const noexcept { return m_; }
    bool   _is_owned() const noexcept { return owns_; }

private:
    void _check_null() const {
        if (m_ == nullptr)
            throw std::system_error(std::make_error_code(std::errc::operation_not_permitted),
                                    "zstl::unique_lock: no associated mutex");
    }
    void _check_not_owns() const {
        if (owns_)
            throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur),
                                    "zstl::unique_lock: already locked");
    }

    Mutex* m_;
    bool   owns_;
};

// Deduction guide
template<typename Mutex>
unique_lock(Mutex&) -> unique_lock<Mutex>;

// Non-member swap
template<typename Mutex>
void swap(unique_lock<Mutex>& a, unique_lock<Mutex>& b) noexcept { a.swap(b); }

// ============================================================
// lock(L1&, L2&, ...) — deadlock-free multi-lock
// ============================================================

template<typename L1>
void lock(L1& l1) { l1.lock(); }

template<typename L1, typename L2>
void lock(L1& l1, L2& l2) {
    // Simple back-off algorithm for two mutexes
    while (true) {
        l1.lock();
        if (l2.try_lock()) return;
        l1.unlock();
        // Brief pause to reduce contention
        l2.lock();
        if (l1.try_lock()) return;
        l2.unlock();
    }
}

template<typename L1, typename L2, typename L3>
void lock(L1& l1, L2& l2, L3& l3) {
    // Try-and-back-off for N=3
    while (true) {
        l1.lock();
        if (l2.try_lock()) {
            if (l3.try_lock()) return;
            l2.unlock();
        }
        l1.unlock();
        // Rotate: try starting from l2 next time
        l2.lock();
        if (l3.try_lock()) {
            if (l1.try_lock()) return;
            l3.unlock();
        }
        l2.unlock();
    }
}

// General N >= 4: delegate to std::lock which uses a proven algorithm
template<typename L1, typename L2, typename L3, typename L4, typename... Ls>
void lock(L1& l1, L2& l2, L3& l3, L4& l4, Ls&... rest) {
    std::lock(l1, l2, l3, l4, rest...);
}

// ============================================================
// try_lock(L1&, L2&, ...) — try all, return -1 on success
// ============================================================

template<typename L1>
int try_lock(L1& l1) { return l1.try_lock() ? -1 : 0; }

template<typename L1, typename L2>
int try_lock(L1& l1, L2& l2) {
    unique_lock<L1> u1(l1, try_to_lock);
    if (!u1) return 0;
    if (l2.try_lock()) { u1.release(); return -1; }
    return 1;
}

template<typename L1, typename L2, typename L3>
int try_lock(L1& l1, L2& l2, L3& l3) {
    unique_lock<L1> u1(l1, try_to_lock);
    if (!u1) return 0;
    unique_lock<L2> u2(l2, try_to_lock);
    if (!u2) return 1;
    if (l3.try_lock()) { u1.release(); u2.release(); return -1; }
    return 2;
}

template<typename L1, typename L2, typename L3, typename L4>
int try_lock(L1& l1, L2& l2, L3& l3, L4& l4) {
    unique_lock<L1> u1(l1, try_to_lock);
    if (!u1) return 0;
    unique_lock<L2> u2(l2, try_to_lock);
    if (!u2) return 1;
    unique_lock<L3> u3(l3, try_to_lock);
    if (!u3) return 2;
    if (l4.try_lock()) { u1.release(); u2.release(); u3.release(); return -1; }
    return 3;
}

template<typename L1, typename L2, typename L3, typename L4, typename L5>
int try_lock(L1& l1, L2& l2, L3& l3, L4& l4, L5& l5) {
    unique_lock<L1> u1(l1, try_to_lock);
    if (!u1) return 0;
    unique_lock<L2> u2(l2, try_to_lock);
    if (!u2) return 1;
    unique_lock<L3> u3(l3, try_to_lock);
    if (!u3) return 2;
    unique_lock<L4> u4(l4, try_to_lock);
    if (!u4) return 3;
    if (l5.try_lock()) { u1.release(); u2.release(); u3.release(); u4.release(); return -1; }
    return 4;
}

// ============================================================
// once_flag + call_once
// ============================================================
class once_flag {
public:
    constexpr once_flag() noexcept = default;

    once_flag(const once_flag&) = delete;
    once_flag& operator=(const once_flag&) = delete;
    once_flag(once_flag&&) = delete;
    once_flag& operator=(once_flag&&) = delete;

private:
    std::once_flag impl_;

    template<typename C, typename... A>
    friend void call_once(once_flag&, C&&, A&&...);
};

template<typename Callable, typename... Args>
void call_once(once_flag& flag, Callable&& f, Args&&... args) {
    std::call_once(flag.impl_,
                   std::forward<Callable>(f),
                   std::forward<Args>(args)...);
}

// ============================================================
// condition_variable
//
// Uses std::condition_variable_any internally so it accepts
// zstl::unique_lock<Mutex> for any Mutex type directly —
// no conversion to std::unique_lock required.
// ============================================================
class condition_variable {
public:
    condition_variable() = default;

    condition_variable(const condition_variable&) = delete;
    condition_variable& operator=(const condition_variable&) = delete;
    condition_variable(condition_variable&&) = delete;
    condition_variable& operator=(condition_variable&&) = delete;

    void notify_one() noexcept { impl_.notify_one(); }
    void notify_all() noexcept { impl_.notify_all(); }

    // Wait indefinitely — accepts zstl::unique_lock<zstl::mutex>
    void wait(unique_lock<mutex>& lock) {
        impl_.wait(lock);
    }

    template<typename Predicate>
    void wait(unique_lock<mutex>& lock, Predicate pred) {
        impl_.wait(lock, pred);
    }

    // Wait with timeout
    template<typename Rep, typename Period>
    std::cv_status wait_for(unique_lock<mutex>& lock,
                             const std::chrono::duration<Rep, Period>& d) {
        return impl_.wait_for(lock, d);
    }

    template<typename Rep, typename Period, typename Predicate>
    bool wait_for(unique_lock<mutex>& lock,
                  const std::chrono::duration<Rep, Period>& d,
                  Predicate pred) {
        return impl_.wait_for(lock, d, pred);
    }

    // Wait until time point
    template<typename Clock, typename Duration>
    std::cv_status wait_until(unique_lock<mutex>& lock,
                               const std::chrono::time_point<Clock, Duration>& t) {
        return impl_.wait_until(lock, t);
    }

    template<typename Clock, typename Duration, typename Predicate>
    bool wait_until(unique_lock<mutex>& lock,
                    const std::chrono::time_point<Clock, Duration>& t,
                    Predicate pred) {
        return impl_.wait_until(lock, t, pred);
    }

    // Access to the underlying native handle (for interop)
    // std::condition_variable_any does not expose native_handle_type
    // in the standard; use the platform-specific handle via impl_ if needed.
    using native_handle_type = std::condition_variable::native_handle_type;

private:
    std::condition_variable_any impl_;
};

} // namespace zstl
