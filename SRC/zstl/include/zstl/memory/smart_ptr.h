// zstl smart_ptr — unique_ptr, shared_ptr, weak_ptr, enable_shared_from_this
//
// Design notes:
// - unique_ptr uses a compressed pair for deleter (EBCO via [[no_unique_address]])
// - shared_ptr uses a control block with atomic reference counts
// - weak_ptr lock() is thread-safe via atomic compare-exchange on control block
// - enable_shared_from_this uses the control block's weak-ref mechanism
// - Atomic shared_ptr operations are NOT included (complex and rarely needed)
#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <new>
#include <stdexcept>     // for std::logic_error (weak_ptr lock failure)
#include <utility>       // for std::exchange (only std usage)
#include "zstl/memory/type_traits.h"
#include "zstl/memory/utility.h"
#include "zstl/memory/construct.h"

namespace zstl {

// ============================================================
// Part 1: default_delete — default deleter for unique_ptr
// ============================================================

template<typename T>
struct default_delete {
    constexpr default_delete() noexcept = default;

    template<typename U, enable_if_t<is_convertible_v<U*, T*>, int> = 0>
    default_delete(const default_delete<U>&) noexcept {}

    void operator()(T* ptr) const noexcept {
        static_assert(sizeof(T) > 0, "default_delete: cannot delete incomplete type");
        delete ptr;
    }
};

// Specialization for arrays
template<typename T>
struct default_delete<T[]> {
    constexpr default_delete() noexcept = default;

    template<typename U, enable_if_t<is_convertible_v<U(*)[], T(*)[]>, int> = 0>
    default_delete(const default_delete<U[]>&) noexcept {}

    template<typename U, enable_if_t<is_convertible_v<U(*)[], T(*)[]>, int> = 0>
    void operator()(U* ptr) const noexcept {
        static_assert(sizeof(T) > 0, "default_delete: cannot delete incomplete type");
        delete[] ptr;
    }
};

// ============================================================
// Part 2: unique_ptr<T, Deleter>
// ============================================================

template<typename T, typename Deleter = default_delete<T>>
class unique_ptr {
public:
    using pointer      = T*;
    using element_type = T;
    using deleter_type = Deleter;

private:
    // Compressed pair: optimize for empty deleters (EBCO)
    // Using [[no_unique_address]] for C++20-compatible compilers
    struct compressed_pair {
        pointer     ptr;
        deleter_type deleter;

        compressed_pair() noexcept(is_nothrow_default_constructible_v<deleter_type>)
            : ptr(nullptr), deleter() {}

        compressed_pair(pointer p) noexcept(is_nothrow_default_constructible_v<deleter_type>)
            : ptr(p), deleter() {}

        compressed_pair(pointer p, const deleter_type& d)
            : ptr(p), deleter(d) {}

        compressed_pair(pointer p, deleter_type&& d)
            noexcept(is_nothrow_move_constructible_v<deleter_type>)
            : ptr(p), deleter(zstl::move(d)) {}
    };

    compressed_pair impl_;

public:
    // --- Constructors ---

    constexpr unique_ptr() noexcept = default;
    constexpr unique_ptr(nullptr_t) noexcept : unique_ptr() {}

    explicit unique_ptr(pointer p) noexcept
        : impl_(p) {}

    unique_ptr(pointer p, const deleter_type& d) noexcept
        : impl_(p, d) {}

    unique_ptr(pointer p, deleter_type&& d) noexcept
        : impl_(p, zstl::move(d)) {}

    unique_ptr(unique_ptr&& other) noexcept
        : impl_(other.release(), zstl::move(other.get_deleter())) {}

    template<typename U, typename E,
             enable_if_t<is_convertible_v<typename unique_ptr<U, E>::pointer, pointer> &&
                         !is_array_v<U>, int> = 0,
             enable_if_t<is_convertible_v<E, deleter_type> ||
                         (is_same_v<E, deleter_type> && is_reference_v<deleter_type>), int> = 0>
    unique_ptr(unique_ptr<U, E>&& other) noexcept
        : impl_(other.release(), zstl::move(other.get_deleter())) {}

    // --- Destructor ---
    ~unique_ptr() {
        if (impl_.ptr) {
            impl_.deleter(impl_.ptr);
        }
    }

    // --- Assignment ---
    unique_ptr& operator=(unique_ptr&& other) noexcept {
        if (this != &other) {
            reset(other.release());
            impl_.deleter = zstl::move(other.impl_.deleter);
        }
        return *this;
    }

    template<typename U, typename E,
             enable_if_t<is_convertible_v<typename unique_ptr<U, E>::pointer, pointer> &&
                         !is_array_v<U>, int> = 0,
             enable_if_t<is_convertible_v<E, deleter_type>, int> = 0>
    unique_ptr& operator=(unique_ptr<U, E>&& other) noexcept {
        reset(other.release());
        impl_.deleter = zstl::move(other.get_deleter());
        return *this;
    }

    unique_ptr& operator=(nullptr_t) noexcept {
        reset();
        return *this;
    }

    // --- Observers ---

    add_lvalue_reference_t<T> operator*() const noexcept {
        return *impl_.ptr;
    }

    pointer operator->() const noexcept {
        return impl_.ptr;
    }

    pointer get() const noexcept {
        return impl_.ptr;
    }

    deleter_type& get_deleter() noexcept {
        return impl_.deleter;
    }

    const deleter_type& get_deleter() const noexcept {
        return impl_.deleter;
    }

    explicit operator bool() const noexcept {
        return impl_.ptr != nullptr;
    }

    // --- Modifiers ---

    pointer release() noexcept {
        pointer p = impl_.ptr;
        impl_.ptr = nullptr;
        return p;
    }

    void reset(pointer p = nullptr) noexcept {
        pointer old = impl_.ptr;
        impl_.ptr = p;
        if (old) {
            impl_.deleter(old);
        }
    }

    void swap(unique_ptr& other) noexcept {
        zstl::swap(impl_.ptr, other.impl_.ptr);
        zstl::swap(impl_.deleter, other.impl_.deleter);
    }
};

// ============================================================
// unique_ptr<T[]> specialization (array)
// ============================================================

template<typename T, typename Deleter>
class unique_ptr<T[], Deleter> {
public:
    using pointer      = T*;
    using element_type = T;
    using deleter_type = Deleter;

private:
    pointer      ptr_ = nullptr;
    deleter_type deleter_;

public:
    constexpr unique_ptr() noexcept = default;
    constexpr unique_ptr(nullptr_t) noexcept : unique_ptr() {}

    explicit unique_ptr(pointer p) noexcept : ptr_(p) {}

    unique_ptr(pointer p, const deleter_type& d) noexcept
        : ptr_(p), deleter_(d) {}

    unique_ptr(pointer p, deleter_type&& d) noexcept
        : ptr_(p), deleter_(zstl::move(d)) {}

    unique_ptr(unique_ptr&& other) noexcept
        : ptr_(other.release()), deleter_(zstl::move(other.deleter_)) {}

    template<typename U, typename E,
             enable_if_t<is_convertible_v<U(*)[], T(*)[]>, int> = 0>
    unique_ptr(unique_ptr<U[], E>&& other) noexcept
        : ptr_(other.release()), deleter_(zstl::move(other.get_deleter())) {}

    ~unique_ptr() {
        if (ptr_) {
            deleter_(ptr_);
        }
    }

    unique_ptr& operator=(unique_ptr&& other) noexcept {
        if (this != &other) {
            reset(other.release());
            deleter_ = zstl::move(other.deleter_);
        }
        return *this;
    }

    unique_ptr& operator=(nullptr_t) noexcept {
        reset();
        return *this;
    }

    T& operator[](size_t i) const noexcept {
        return ptr_[i];
    }

    pointer get() const noexcept { return ptr_; }
    deleter_type& get_deleter() noexcept { return deleter_; }
    const deleter_type& get_deleter() const noexcept { return deleter_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    pointer release() noexcept {
        pointer p = ptr_;
        ptr_ = nullptr;
        return p;
    }

    void reset(pointer p = nullptr) noexcept {
        pointer old = ptr_;
        ptr_ = p;
        if (old) {
            deleter_(old);
        }
    }

    void swap(unique_ptr& other) noexcept {
        zstl::swap(ptr_, other.ptr_);
        zstl::swap(deleter_, other.deleter_);
    }
};

// unique_ptr comparison operators
template<typename T1, typename D1, typename T2, typename D2>
bool operator==(const unique_ptr<T1, D1>& a, const unique_ptr<T2, D2>& b) noexcept {
    return a.get() == b.get();
}

template<typename T1, typename D1, typename T2, typename D2>
bool operator!=(const unique_ptr<T1, D1>& a, const unique_ptr<T2, D2>& b) noexcept {
    return a.get() != b.get();
}

template<typename T1, typename D1, typename T2, typename D2>
bool operator<(const unique_ptr<T1, D1>& a, const unique_ptr<T2, D2>& b) noexcept {
    return a.get() < b.get();
}

template<typename T1, typename D1, typename T2, typename D2>
bool operator<=(const unique_ptr<T1, D1>& a, const unique_ptr<T2, D2>& b) noexcept {
    return a.get() <= b.get();
}

template<typename T1, typename D1, typename T2, typename D2>
bool operator>(const unique_ptr<T1, D1>& a, const unique_ptr<T2, D2>& b) noexcept {
    return a.get() > b.get();
}

template<typename T1, typename D1, typename T2, typename D2>
bool operator>=(const unique_ptr<T1, D1>& a, const unique_ptr<T2, D2>& b) noexcept {
    return a.get() >= b.get();
}

template<typename T, typename D>
bool operator==(const unique_ptr<T, D>& a, nullptr_t) noexcept {
    return !a;
}

template<typename T, typename D>
bool operator==(nullptr_t, const unique_ptr<T, D>& a) noexcept {
    return !a;
}

template<typename T, typename D>
bool operator!=(const unique_ptr<T, D>& a, nullptr_t) noexcept {
    return static_cast<bool>(a);
}

template<typename T, typename D>
bool operator!=(nullptr_t, const unique_ptr<T, D>& a) noexcept {
    return static_cast<bool>(a);
}

// ============================================================
// make_unique
// ============================================================

template<typename T, typename... Args,
         enable_if_t<!is_array_v<T>, int> = 0>
unique_ptr<T> make_unique(Args&&... args) {
    return unique_ptr<T>(new T(zstl::forward<Args>(args)...));
}

template<typename T,
         enable_if_t<is_array_v<T> && extent_v<T> == 0, int> = 0>
unique_ptr<T> make_unique(size_t size) {
    using E = remove_extent_t<T>;
    return unique_ptr<T>(new E[size]());
}

// Prevent make_unique for bounded arrays (T[N])
template<typename T, typename... Args,
         enable_if_t<extent_v<T> != 0, int> = 0>
void make_unique(Args&&...) = delete;

// ============================================================
// Part 3: shared_ptr control block
// ============================================================

// Control block stores reference counts and deleter.
// Managed object is allocated separately (or together for make_shared).
struct control_block_base {
    std::atomic<long> shared_count{1};
    std::atomic<long> weak_count{1};  // also counts the shared_ptr's "1"

    control_block_base() = default;
    virtual ~control_block_base() = default;
    virtual void destroy_resource() noexcept = 0;
    virtual void* get_object() noexcept { return nullptr; }

    void add_shared() noexcept {
        shared_count.fetch_add(1, std::memory_order_relaxed);
    }

    void add_weak() noexcept {
        weak_count.fetch_add(1, std::memory_order_relaxed);
    }

    // Release shared reference. Returns true if this was the last shared ref.
    bool release_shared() noexcept {
        if (shared_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            destroy_resource();
            // After destroying resource, release the weak count for this shared ref
            release_weak();
            return true;
        }
        return false;
    }

    // Release weak reference. If both counts hit zero, delete control block.
    void release_weak() noexcept {
        if (weak_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete this;
        }
    }

    long use_count() const noexcept {
        return shared_count.load(std::memory_order_relaxed);
    }
};

// Typed control block: stores deleter and (optionally) allocator
template<typename T, typename Deleter = default_delete<T>>
struct control_block_impl : control_block_base {
    T*       ptr;
    Deleter  deleter;

    control_block_impl(T* p, Deleter d)
        : ptr(p), deleter(zstl::move(d)) {}

    void destroy_resource() noexcept override {
        deleter(ptr);
    }

    void* get_object() noexcept override {
        return static_cast<void*>(ptr);
    }
};

// ============================================================
// Part 4: shared_ptr<T>
// ============================================================

// Forward declarations (used by shared_ptr)
template<typename T>
class weak_ptr;

template<typename T>
class enable_shared_from_this;

template<typename T>
class shared_ptr {
public:
    using element_type = remove_extent_t<T>;
    using pointer      = element_type*;

private:
    element_type*    ptr_ = nullptr;
    control_block_base* cb_ = nullptr;

    // Private constructor from raw parts
    shared_ptr(element_type* ptr, control_block_base* cb) noexcept
        : ptr_(ptr), cb_(cb) {}

    // Friends for access to private constructor
    template<typename U>
    friend class shared_ptr;
    template<typename U>
    friend class weak_ptr;
    template<typename U, typename... Args>
    friend shared_ptr<U> make_shared(Args&&... args);

public:
    // --- Constructors ---

    constexpr shared_ptr() noexcept = default;
    constexpr shared_ptr(nullptr_t) noexcept : shared_ptr() {}

    // From raw pointer (takes ownership)
    // If T derives from enable_shared_from_this, sets up the
    // weak back-reference so shared_from_this() works.
    template<typename U,
             enable_if_t<is_convertible_v<U*, element_type*>, int> = 0>
    explicit shared_ptr(U* p)
        : ptr_(p), cb_(p ? new control_block_impl<U>(p, default_delete<U>()) : nullptr)
    {
        if constexpr (is_base_of_v<enable_shared_from_this<U>, U>) {
            if (p) p->_internal_accept_owner(*this);
        }
    }

    // From raw pointer with custom deleter
    template<typename U, typename Deleter,
             enable_if_t<is_convertible_v<U*, element_type*>, int> = 0>
    shared_ptr(U* p, Deleter d)
        : ptr_(p), cb_(p ? new control_block_impl<U, Deleter>(p, zstl::move(d)) : nullptr)
    {
        if constexpr (is_base_of_v<enable_shared_from_this<U>, U>) {
            if (p) p->_internal_accept_owner(*this);
        }
    }

    // Copy constructor
    shared_ptr(const shared_ptr& other) noexcept
        : ptr_(other.ptr_), cb_(other.cb_)
    {
        if (cb_) cb_->add_shared();
    }

    // Converting copy constructor
    template<typename U,
             enable_if_t<is_convertible_v<U*, element_type*>, int> = 0>
    shared_ptr(const shared_ptr<U>& other) noexcept
        : ptr_(other.ptr_), cb_(other.cb_)
    {
        if (cb_) cb_->add_shared();
    }

    // Move constructor
    shared_ptr(shared_ptr&& other) noexcept
        : ptr_(other.ptr_), cb_(other.cb_)
    {
        other.ptr_ = nullptr;
        other.cb_  = nullptr;
    }

    // Converting move constructor
    template<typename U,
             enable_if_t<is_convertible_v<U*, element_type*>, int> = 0>
    shared_ptr(shared_ptr<U>&& other) noexcept
        : ptr_(other.ptr_), cb_(other.cb_)
    {
        other.ptr_ = nullptr;
        other.cb_  = nullptr;
    }

    // Aliasing constructor — shares ownership but points to a different object
    template<typename U>
    shared_ptr(const shared_ptr<U>& other, element_type* ptr) noexcept
        : ptr_(ptr), cb_(other.cb_)
    {
        if (cb_) cb_->add_shared();
    }

    // From unique_ptr
    template<typename U, typename Deleter,
             enable_if_t<is_convertible_v<U*, element_type*>, int> = 0>
    shared_ptr(unique_ptr<U, Deleter>&& other)
        : ptr_(other.get())
    {
        if (ptr_) {
            cb_ = new control_block_impl<U, Deleter>(other.get(), zstl::move(other.get_deleter()));
        }
        other.release();
    }

    // From weak_ptr (throws if expired)
    template<typename U>
    explicit shared_ptr(const weak_ptr<U>& other);

    // --- Destructor ---
    ~shared_ptr() {
        if (cb_) {
            cb_->release_shared();
        }
    }

    // --- Assignment ---

    shared_ptr& operator=(const shared_ptr& other) noexcept {
        if (this != &other) {
            shared_ptr(other).swap(*this);
        }
        return *this;
    }

    shared_ptr& operator=(shared_ptr&& other) noexcept {
        shared_ptr(zstl::move(other)).swap(*this);
        return *this;
    }

    template<typename U>
    shared_ptr& operator=(const shared_ptr<U>& other) noexcept {
        shared_ptr(other).swap(*this);
        return *this;
    }

    template<typename U>
    shared_ptr& operator=(shared_ptr<U>&& other) noexcept {
        shared_ptr(zstl::move(other)).swap(*this);
        return *this;
    }

    // --- Modifiers ---

    void reset() noexcept {
        shared_ptr().swap(*this);
    }

    template<typename U>
    void reset(U* p) {
        shared_ptr(p).swap(*this);
    }

    template<typename U, typename Deleter>
    void reset(U* p, Deleter d) {
        shared_ptr(p, zstl::move(d)).swap(*this);
    }

    void swap(shared_ptr& other) noexcept {
        zstl::swap(ptr_, other.ptr_);
        zstl::swap(cb_, other.cb_);
    }

    // --- Observers ---

    element_type* get() const noexcept { return ptr_; }

    add_lvalue_reference_t<element_type> operator*() const noexcept {
        return *ptr_;
    }

    element_type* operator->() const noexcept {
        return ptr_;
    }

    long use_count() const noexcept {
        return cb_ ? cb_->use_count() : 0;
    }

    explicit operator bool() const noexcept {
        return ptr_ != nullptr;
    }

    // Check if this is the only owner
    bool unique() const noexcept {
        return use_count() == 1;
    }

    // For enable_shared_from_this support
    template<typename U>
    bool owner_before(const shared_ptr<U>& other) const noexcept {
        return cb_ < other.cb_;
    }

    template<typename U>
    bool owner_before(const weak_ptr<U>& other) const noexcept;
};

// shared_ptr comparison operators
template<typename T, typename U>
bool operator==(const shared_ptr<T>& a, const shared_ptr<U>& b) noexcept {
    return a.get() == b.get();
}

template<typename T, typename U>
bool operator!=(const shared_ptr<T>& a, const shared_ptr<U>& b) noexcept {
    return a.get() != b.get();
}

template<typename T, typename U>
bool operator<(const shared_ptr<T>& a, const shared_ptr<U>& b) noexcept {
    return a.get() < b.get();
}

template<typename T>
bool operator==(const shared_ptr<T>& a, nullptr_t) noexcept {
    return !a;
}

template<typename T>
bool operator==(nullptr_t, const shared_ptr<T>& a) noexcept {
    return !a;
}

template<typename T>
bool operator!=(const shared_ptr<T>& a, nullptr_t) noexcept {
    return static_cast<bool>(a);
}

template<typename T>
bool operator!=(nullptr_t, const shared_ptr<T>& a) noexcept {
    return static_cast<bool>(a);
}

// ============================================================
// Part 5: weak_ptr<T>
// ============================================================

template<typename T>
class weak_ptr {
public:
    using element_type = remove_extent_t<T>;
    using pointer      = element_type*;

private:
    element_type*       ptr_ = nullptr;
    control_block_base* cb_  = nullptr;

    template<typename U>
    friend class shared_ptr;
    template<typename U>
    friend class weak_ptr;

public:
    constexpr weak_ptr() noexcept = default;

    // From shared_ptr
    weak_ptr(const shared_ptr<T>& sptr) noexcept
        : ptr_(sptr.ptr_), cb_(sptr.cb_)
    {
        if (cb_) cb_->add_weak();
    }

    // Copy constructor
    weak_ptr(const weak_ptr& other) noexcept
        : ptr_(other.ptr_), cb_(other.cb_)
    {
        if (cb_) cb_->add_weak();
    }

    // Converting copy constructor
    template<typename U,
             enable_if_t<is_convertible_v<U*, element_type*>, int> = 0>
    weak_ptr(const weak_ptr<U>& other) noexcept
        : ptr_(other.ptr_), cb_(other.cb_)
    {
        if (cb_) cb_->add_weak();
    }

    // Move constructor
    weak_ptr(weak_ptr&& other) noexcept
        : ptr_(other.ptr_), cb_(other.cb_)
    {
        other.ptr_ = nullptr;
        other.cb_  = nullptr;
    }

    // Destructor
    ~weak_ptr() {
        if (cb_) cb_->release_weak();
    }

    // --- Assignment ---

    weak_ptr& operator=(const weak_ptr& other) noexcept {
        weak_ptr(other).swap(*this);
        return *this;
    }

    weak_ptr& operator=(const shared_ptr<T>& sptr) noexcept {
        weak_ptr(sptr).swap(*this);
        return *this;
    }

    weak_ptr& operator=(weak_ptr&& other) noexcept {
        weak_ptr(zstl::move(other)).swap(*this);
        return *this;
    }

    // --- Modifiers ---

    void reset() noexcept {
        weak_ptr().swap(*this);
    }

    void swap(weak_ptr& other) noexcept {
        zstl::swap(ptr_, other.ptr_);
        zstl::swap(cb_, other.cb_);
    }

    // --- Observers ---

    long use_count() const noexcept {
        return cb_ ? cb_->use_count() : 0;
    }

    bool expired() const noexcept {
        return use_count() == 0;
    }

    // lock() — attempt to obtain a shared_ptr.
    // Thread-safe: uses compare-exchange to safely increment shared count
    // only if it is non-zero.
    shared_ptr<T> lock() const noexcept {
        if (!cb_) return shared_ptr<T>();

        // Try to increment shared_count from a non-zero value
        long old_count = cb_->shared_count.load(std::memory_order_relaxed);
        do {
            if (old_count == 0) {
                return shared_ptr<T>();  // Expired
            }
        } while (!cb_->shared_count.compare_exchange_weak(
            old_count, old_count + 1,
            std::memory_order_acquire, std::memory_order_relaxed));

        // Successfully acquired shared reference
        return shared_ptr<T>(ptr_, cb_);
    }

    template<typename U>
    bool owner_before(const shared_ptr<U>& other) const noexcept {
        return cb_ < other.cb_;
    }

    template<typename U>
    bool owner_before(const weak_ptr<U>& other) const noexcept {
        return cb_ < other.cb_;
    }
};

// shared_ptr constructor from weak_ptr (defined here because it needs
// weak_ptr::lock() which is defined above)
template<typename T>
template<typename U>
shared_ptr<T>::shared_ptr(const weak_ptr<U>& other)
    : ptr_(nullptr), cb_(nullptr)
{
    // Attempt to lock the weak_ptr
    if (other.cb_) {
        // Try to increment shared_count while it's > 0
        long old_count = other.cb_->shared_count.load(std::memory_order_relaxed);
        do {
            if (old_count == 0) {
                throw std::logic_error("zstl::shared_ptr: bad weak_ptr");
            }
        } while (!other.cb_->shared_count.compare_exchange_weak(
            old_count, old_count + 1,
            std::memory_order_acquire, std::memory_order_relaxed));

        ptr_ = other.ptr_;
        cb_  = other.cb_;
    }
}

// owner_before for shared_ptr comparing with weak_ptr
template<typename T>
template<typename U>
bool shared_ptr<T>::owner_before(const weak_ptr<U>& other) const noexcept {
    return cb_ < other.cb_;
}

// ============================================================
// make_shared
// ============================================================

namespace detail {

// Control block + object combined allocation for make_shared
template<typename T>
struct combined_block : control_block_base {
    // Storage for the managed object (aligned and sized appropriately)
    alignas(T) unsigned char storage[sizeof(T)];

    T* object_ptr() noexcept {
        return reinterpret_cast<T*>(storage);
    }

    void destroy_resource() noexcept override {
        object_ptr()->~T();
    }

    void* get_object() noexcept override {
        return static_cast<void*>(object_ptr());
    }
};

} // namespace detail

template<typename T, typename... Args>
shared_ptr<T> make_shared(Args&&... args) {
    // Allocate control block + object in one contiguous allocation
    auto* combined = new detail::combined_block<T>();

    // Construct the object in-place
    ::new (combined->object_ptr()) T(zstl::forward<Args>(args)...);

    // Create shared_ptr with the combined control block
    return shared_ptr<T>(combined->object_ptr(), combined);
}

// ============================================================
// static_pointer_cast, dynamic_pointer_cast, const_pointer_cast
// ============================================================

template<typename T, typename U>
shared_ptr<T> static_pointer_cast(const shared_ptr<U>& r) noexcept {
    auto p = static_cast<typename shared_ptr<T>::element_type*>(r.get());
    return shared_ptr<T>(r, p);
}

template<typename T, typename U>
shared_ptr<T> dynamic_pointer_cast(const shared_ptr<U>& r) noexcept {
    if (auto p = dynamic_cast<typename shared_ptr<T>::element_type*>(r.get())) {
        return shared_ptr<T>(r, p);
    }
    return shared_ptr<T>();
}

template<typename T, typename U>
shared_ptr<T> const_pointer_cast(const shared_ptr<U>& r) noexcept {
    auto p = const_cast<typename shared_ptr<T>::element_type*>(r.get());
    return shared_ptr<T>(r, p);
}

// ============================================================
// Part 6: enable_shared_from_this
// ============================================================

template<typename T>
class enable_shared_from_this {
protected:
    enable_shared_from_this() noexcept = default;
    enable_shared_from_this(const enable_shared_from_this&) noexcept {}
    enable_shared_from_this& operator=(const enable_shared_from_this&) noexcept {
        return *this;
    }
    ~enable_shared_from_this() = default;

public:
    shared_ptr<T> shared_from_this() {
        return shared_ptr<T>(weak_this_.lock());
    }

    shared_ptr<const T> shared_from_this() const {
        return shared_ptr<const T>(weak_this_.lock());
    }

    weak_ptr<T> weak_from_this() noexcept {
        return weak_this_;
    }

    weak_ptr<const T> weak_from_this() const noexcept {
        return weak_this_;
    }

private:
    template<typename U>
    friend class shared_ptr;

    // Called by shared_ptr constructor when taking ownership of a
    // raw T* that derives from enable_shared_from_this
    template<typename U>
    void _internal_accept_owner(const shared_ptr<U>& sp) const noexcept {
        if (weak_this_.expired()) {
            weak_this_ = shared_ptr<T>(sp, static_cast<T*>(const_cast<U*>(sp.get())));
        }
    }

    mutable weak_ptr<T> weak_this_;
};

// ESFT initialization is handled directly in shared_ptr's
// raw-pointer constructors (see above), which call
// enable_shared_from_this::_internal_accept_owner().

// ============================================================
// swap specializations for smart pointers
// ============================================================

template<typename T, typename D>
void swap(unique_ptr<T, D>& a, unique_ptr<T, D>& b) noexcept {
    a.swap(b);
}

template<typename T>
void swap(shared_ptr<T>& a, shared_ptr<T>& b) noexcept {
    a.swap(b);
}

template<typename T>
void swap(weak_ptr<T>& a, weak_ptr<T>& b) noexcept {
    a.swap(b);
}

} // namespace zstl
