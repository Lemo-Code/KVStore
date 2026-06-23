// zstl uninitialized — raw_storage_iterator, temporary_buffer,
// addressof, and alignment utilities.
#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <iterator>
#include <climits>
#include "zstl/memory/type_traits.h"
#include "zstl/memory/utility.h"
#include "zstl/memory/construct.h"

namespace zstl {

// ============================================================
// Part 1: align — calculate the aligned size/offset
// Note: addressof is defined in utility.h (shared between
// construct.h and uninitialized.h to avoid circular includes).
// ============================================================

// Returns the smallest value >= n that is a multiple of alignment.
constexpr size_t align_up(size_t n, size_t alignment) noexcept {
    return (n + alignment - 1) & ~(alignment - 1);
}

// Returns the largest value <= n that is a multiple of alignment.
constexpr size_t align_down(size_t n, size_t alignment) noexcept {
    return n & ~(alignment - 1);
}

// Check if a pointer is aligned to a given boundary.
inline bool is_aligned(const void* ptr, size_t alignment) noexcept {
    return (reinterpret_cast<uintptr_t>(ptr) & (alignment - 1)) == 0;
}

// ============================================================
// Part 3: aligned_alloc / aligned_free — aligned dynamic memory
// ============================================================

// Allocate memory with a specified alignment.
// alignment must be a power of 2.
inline void* aligned_alloc(size_t alignment, size_t size) noexcept {
    // Ensure minimum alignment and size
    if (alignment < alignof(void*)) {
        alignment = alignof(void*);
    }
    if (size == 0) {
        return nullptr;
    }

#if defined(__GNUC__) || defined(__clang__)
    // On glibc and most POSIX systems, we can use posix_memalign-like
    // approaches, or aligned_alloc from C11.
    // For simplicity and portability, overallocate and align manually.

    // We need to store the original pointer for later free.
    // Allocate extra space: alignment + size + sizeof(void*)
    size_t total = size + alignment + sizeof(void*);
    void* raw = ::operator new(total, std::nothrow);
    if (!raw) return nullptr;

    // Align the pointer, reserving space at the front for the raw pointer
    void* aligned = reinterpret_cast<void*>(
        align_up(reinterpret_cast<uintptr_t>(raw) + sizeof(void*), alignment));

    // Store raw pointer just before the aligned block
    *(reinterpret_cast<void**>(aligned) - 1) = raw;

    return aligned;
#else
    // Fallback
    return ::operator new(size);
#endif
}

// Free memory allocated by aligned_alloc.
inline void aligned_free(void* ptr) noexcept {
    if (!ptr) return;
#if defined(__GNUC__) || defined(__clang__)
    // Retrieve the original raw pointer stored before the aligned block
    void* raw = *(reinterpret_cast<void**>(ptr) - 1);
    ::operator delete(raw);
#else
    ::operator delete(ptr);
#endif
}

// ============================================================
// Part 4: raw_storage_iterator — output iterator that
//         placement-constructs objects into raw memory
// ============================================================

template<typename OutputIterator, typename T>
class raw_storage_iterator
{
public:
    using iterator_category = std::output_iterator_tag;
    using value_type        = void;
    using difference_type   = void;
    using pointer           = void;
    using reference         = void;

    explicit raw_storage_iterator(OutputIterator it) noexcept
        : iter_(it) {}

    // Dereference returns *this for chained assignment
    raw_storage_iterator& operator*() noexcept {
        return *this;
    }

    // Assignment placement-constructs the element
    raw_storage_iterator& operator=(const T& element) {
        construct(zstl::addressof(*iter_), element);
        return *this;
    }

    // Move-assignment placement-constructs from rvalue
    raw_storage_iterator& operator=(T&& element) {
        construct(zstl::addressof(*iter_), zstl::move(element));
        return *this;
    }

    // Pre-increment
    raw_storage_iterator& operator++() noexcept {
        ++iter_;
        return *this;
    }

    // Post-increment
    raw_storage_iterator operator++(int) noexcept {
        raw_storage_iterator tmp = *this;
        ++iter_;
        return tmp;
    }

    // Get base iterator
    OutputIterator base() const noexcept {
        return iter_;
    }

private:
    OutputIterator iter_;
};

// ============================================================
// Part 5: temporary_buffer — RAII wrapper for temporary
//         uninitialized storage
// ============================================================

template<typename T>
class temporary_buffer {
public:
    using value_type = T;
    using pointer    = T*;
    using size_type  = ptrdiff_t;

    // Allocate a temporary buffer for at most n elements.
    // The actual size may be less than n (best-effort).
    explicit temporary_buffer(ptrdiff_t n) noexcept
        : ptr_(nullptr), size_(0)
    {
        if (n <= 0) return;

        // Try to allocate. If allocation fails with std::bad_alloc,
        // reduce requested size by half and retry.
        ptrdiff_t requested = n;
        while (requested > 0) {
            try {
                ptr_ = static_cast<T*>(
                    ::operator new(static_cast<size_t>(requested) * sizeof(T),
                                   std::nothrow));
                if (ptr_) {
                    size_ = requested;
                }
                return;
            } catch (...) {
                // Reduction loop continues below
            }
            requested /= 2;
        }
        // If all attempts fail, ptr_ remains nullptr, size_ remains 0
    }

    ~temporary_buffer() {
        if (ptr_) {
            ::operator delete(ptr_);
        }
    }

    temporary_buffer(const temporary_buffer&) = delete;
    temporary_buffer& operator=(const temporary_buffer&) = delete;

    temporary_buffer(temporary_buffer&& other) noexcept
        : ptr_(other.ptr_), size_(other.size_)
    {
        other.ptr_  = nullptr;
        other.size_ = 0;
    }

    temporary_buffer& operator=(temporary_buffer&& other) noexcept {
        if (this != &other) {
            if (ptr_) {
                ::operator delete(ptr_);
            }
            ptr_  = other.ptr_;
            size_ = other.size_;
            other.ptr_  = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    pointer data() const noexcept { return ptr_; }
    size_type size() const noexcept { return size_; }
    size_type requested_size() const noexcept { return size_; }

private:
    pointer  ptr_;
    ptrdiff_t size_;
};

// ============================================================
// Part 6: get_temporary_buffer / return_temporary_buffer
//         (legacy STL interface, implemented for compatibility)
// ============================================================

template<typename T>
pair<T*, ptrdiff_t> get_temporary_buffer(ptrdiff_t n) noexcept {
    if (n <= 0) return pair<T*, ptrdiff_t>(nullptr, 0);

    ptrdiff_t requested = n;
    while (requested > 0) {
        T* p = static_cast<T*>(
            ::operator new(static_cast<size_t>(requested) * sizeof(T),
                           std::nothrow));
        if (p) {
            return pair<T*, ptrdiff_t>(p, requested);
        }
        requested /= 2;
    }
    return pair<T*, ptrdiff_t>(nullptr, 0);
}

template<typename T>
void return_temporary_buffer(T* p) noexcept {
    ::operator delete(p);
}

// ============================================================
// Part 7: alignment metadata helpers
// ============================================================

// alignment_of_v is defined in type_traits.h — do not redefine here.

// Check if two values have the same alignment
template<typename T, typename U>
constexpr bool is_same_alignment_v = (alignof(T) == alignof(U));

// ============================================================
// Part 8: assume_aligned — hint to the compiler about pointer
//          alignment (for optimization)
// ============================================================

#if defined(__GNUC__) || defined(__clang__)
template<size_t N, typename T>
inline T* assume_aligned(T* ptr) noexcept {
    static_assert(N >= 1 && (N & (N - 1)) == 0,
                  "Alignment must be a power of 2");
    return reinterpret_cast<T*>(
        __builtin_assume_aligned(ptr, N));
}
#else
template<size_t N, typename T>
inline T* assume_aligned(T* ptr) noexcept {
    return ptr;
}
#endif

} // namespace zstl
