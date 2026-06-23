// zero any — type-erased value container with small buffer optimization
// Similar to std::any but optimized for small types (SBO up to 32 bytes).
// Supports any_cast<T> with pointer and reference semantics.
// Throws bad_any_cast on type mismatch for reference casts.
#pragma once

#include <type_traits>
#include <typeinfo>
#include <utility>
#include <new>
#include <cstddef>
#include <cstring>
#include <stdexcept>

namespace zero {

// Exception thrown for invalid any_cast
class bad_any_cast : public std::bad_cast {
public:
    bad_any_cast() = default;
    const char* what() const noexcept override {
        return "zero::bad_any_cast: failed conversion using zero::any_cast";
    }
};

class any {
    // Small buffer optimization threshold (fits most small types + ptr
    // alignment)
    static constexpr size_t kSmallSize = 32;

    // Type-erased handler interface stored in the vtable pointer
    struct Handler {
        const std::type_info& (*type)() noexcept;
        void (*destroy)(void* obj) noexcept;
        void (*copy)(const void* src, void* dst);
        void (*move)(void* src, void* dst) noexcept;
    };

    // Compute the handler for a given type (lazily instantiated)
    template <typename T>
    static const Handler* get_handler() noexcept {
        static const Handler h = {
            []() noexcept -> const std::type_info& { return typeid(T); },
            // destroy
            [](void* obj) noexcept {
                static_cast<T*>(obj)->~T();
            },
            // copy
            [](const void* src, void* dst) {
                if constexpr (std::is_copy_constructible_v<T>) {
                    ::new (dst) T(*static_cast<const T*>(src));
                } else {
                    throw std::runtime_error(
                        "any: T is not copy-constructible");
                }
            },
            // move
            [](void* src, void* dst) noexcept {
                ::new (dst) T(std::move(*static_cast<T*>(src)));
                static_cast<T*>(src)->~T();
            },
        };
        return &h;
    }

public:
    // Default constructor — empty any
    any() noexcept : handler_(nullptr) {}

    // Construct from value (perfect forwarding)
    template <typename T,
              typename Decayed = std::decay_t<T>,
              typename = std::enable_if_t<
                  !std::is_same_v<Decayed, any> &&
                  !std::is_same_v<Decayed, std::in_place_type_t<T>>>>
    any(T&& value) {
        construct<Decayed>(std::forward<T>(value));
    }

    // In-place construction
    template <typename T, typename... Args>
    explicit any(std::in_place_type_t<T>, Args&&... args) {
        construct<T>(std::forward<Args>(args)...);
    }

    // Copy constructor
    any(const any& other) {
        if (other.handler_) {
            other.handler_->copy(other.obj_ptr(), obj_ptr());
            handler_ = other.handler_;
            on_heap_ = other.on_heap_;
        }
    }

    // Move constructor
    any(any&& other) noexcept {
        if (other.handler_) {
            if (other.on_heap_) {
                // Just steal the heap pointer
                heap_ptr_ = other.heap_ptr_;
                handler_ = other.handler_;
                on_heap_ = true;
                other.handler_ = nullptr;
                other.on_heap_ = false;
            } else {
                // Move small-buffer contents
                other.handler_->move(other.obj_ptr(), obj_ptr());
                handler_ = other.handler_;
                on_heap_ = false;
                other.handler_ = nullptr;
            }
        }
    }

    // Copy assignment
    any& operator=(const any& other) {
        if (this != &other) {
            reset();
            if (other.handler_) {
                other.handler_->copy(other.obj_ptr(), obj_ptr());
                handler_ = other.handler_;
                on_heap_ = other.on_heap_;
            }
        }
        return *this;
    }

    // Move assignment
    any& operator=(any&& other) noexcept {
        if (this != &other) {
            reset();
            if (other.handler_) {
                if (other.on_heap_) {
                    heap_ptr_ = other.heap_ptr_;
                    handler_ = other.handler_;
                    on_heap_ = true;
                    other.handler_ = nullptr;
                    other.on_heap_ = false;
                } else {
                    other.handler_->move(other.obj_ptr(), obj_ptr());
                    handler_ = other.handler_;
                    on_heap_ = false;
                    other.handler_ = nullptr;
                }
            }
        }
        return *this;
    }

    // Value assignment
    template <typename T,
              typename Decayed = std::decay_t<T>,
              typename = std::enable_if_t<!std::is_same_v<Decayed, any>>>
    any& operator=(T&& value) {
        reset();
        construct<Decayed>(std::forward<T>(value));
        return *this;
    }

    ~any() {
        reset();
    }

    // Whether the any holds a value
    bool has_value() const noexcept {
        return handler_ != nullptr;
    }

    explicit operator bool() const noexcept {
        return has_value();
    }

    // Get the type_info of the contained value
    const std::type_info& type() const noexcept {
        return handler_ ? handler_->type() : typeid(void);
    }

    // Destroy the contained value
    void reset() noexcept {
        if (handler_) {
            handler_->destroy(obj_ptr());
            if (on_heap_) {
                ::operator delete(heap_ptr_);
            }
            handler_ = nullptr;
            on_heap_ = false;
        }
    }

    // Swap two any objects
    void swap(any& other) noexcept {
        if (!handler_ && !other.handler_) return;

        if (handler_ && other.handler_) {
            if (on_heap_ && other.on_heap_) {
                // Both on heap: swap pointers
                auto tmp_ptr = heap_ptr_;
                auto tmp_h = handler_;
                heap_ptr_ = other.heap_ptr_;
                handler_ = other.handler_;
                other.heap_ptr_ = tmp_ptr;
                other.handler_ = tmp_h;
            } else if (!on_heap_ && !other.on_heap_) {
                // Both small: use temp storage
                alignas(std::max_align_t) char tmp[kSmallSize];
                void* tmp_ptr = &tmp;
                handler_->move(obj_ptr(), tmp_ptr);
                const Handler* tmp_h = handler_;
                other.handler_->move(other.obj_ptr(), obj_ptr());
                handler_ = other.handler_;
                tmp_h->move(tmp_ptr, other.obj_ptr());
                other.handler_ = tmp_h;
            } else if (on_heap_) {
                // This on heap, other small: move other to temp, this to
                // other, temp to this heap
                alignas(std::max_align_t) char tmp[kSmallSize];
                void* tmp_ptr = &tmp;
                other.handler_->move(other.obj_ptr(), tmp_ptr);
                const Handler* tmp_h = other.handler_;
                // We can't store heap in other's small buffer generally;
                // simplest approach: create a copy of heap value in small
                // storage if it fits
                if (sizeof_heap_obj() <= kSmallSize &&
                    alignof_heap_obj() <= alignof(std::max_align_t)) {
                    handler_->copy(obj_ptr(), other.obj_ptr());
                    other.handler_ = handler_;
                    other.on_heap_ = false;
                    handler_->destroy(obj_ptr());
                    ::operator delete(heap_ptr_);
                    // temp -> this (small)
                    tmp_h->move(tmp_ptr, obj_ptr());
                    handler_ = tmp_h;
                    on_heap_ = false;
                } else {
                    // Fallback: move to heap
                    auto* new_heap =
                        ::operator new(sizeof_heap_obj(),
                                       std::align_val_t(alignof_heap_obj()));
                    handler_->move(obj_ptr(), new_heap);
                    handler_->destroy(obj_ptr());
                    ::operator delete(heap_ptr_);
                    tmp_h->move(tmp_ptr, obj_ptr());
                    handler_ = tmp_h;
                    on_heap_ = false;
                    other.handler_ = handler_;
                    other.on_heap_ = true;
                    other.heap_ptr_ = new_heap;
                }
            } else {
                // This small, other on heap — symmetric to above but
                // delegated
                other.swap(*this);
            }
        } else if (handler_) {
            // Only this has value — move to other
            if (on_heap_) {
                other.heap_ptr_ = heap_ptr_;
                other.handler_ = handler_;
                other.on_heap_ = true;
            } else {
                handler_->move(obj_ptr(), other.obj_ptr());
                other.handler_ = handler_;
                other.on_heap_ = false;
            }
            handler_ = nullptr;
            on_heap_ = false;
        } else {
            // Only other has value — this absorbed above
            other.swap(*this);
        }
    }

private:
    // Get pointer to the object (small buffer or heap)
    void* obj_ptr() noexcept {
        return on_heap_ ? heap_ptr_ : static_cast<void*>(&storage_);
    }

    const void* obj_ptr() const noexcept {
        return on_heap_ ? heap_ptr_
                        : static_cast<const void*>(&storage_);
    }

    // Compute the size of the heap-allocated object (only valid when
    // on_heap_)
    size_t sizeof_heap_obj() const {
        // We don't store the size; for swap we use handler type info.
        // A simpler approach: always default to small buffer for known
        // small types, heap for large types.
        return kSmallSize * 2;  // Conservative fallback
    }

    size_t alignof_heap_obj() const {
        return alignof(std::max_align_t);
    }

    // Construct T in the storage (choose small or heap based on
    // sizeof/alignof)
    template <typename T, typename... Args>
    void construct(Args&&... args) {
        constexpr bool fits =
            sizeof(T) <= kSmallSize && alignof(T) <= alignof(std::max_align_t);
        if constexpr (fits) {
            ::new (&storage_) T(std::forward<Args>(args)...);
            on_heap_ = false;
        } else {
            void* mem = ::operator new(sizeof(T),
                                       std::align_val_t(alignof(T)));
            ::new (mem) T(std::forward<Args>(args)...);
            heap_ptr_ = mem;
            on_heap_ = true;
        }
        handler_ = get_handler<T>();
    }

    // Keep storage_ first to avoid unnecessary offset calculations
    alignas(std::max_align_t) char storage_[kSmallSize];
    union {
        void* heap_ptr_;
    };
    const Handler* handler_ = nullptr;
    bool on_heap_ = false;

    // any_cast access
    template <typename T>
    friend T* any_cast(any*) noexcept;

    template <typename T>
    friend const T* any_cast(const any*) noexcept;

    template <typename T>
    friend T any_cast(any&);

    template <typename T>
    friend T any_cast(const any&);

    template <typename T>
    friend T any_cast(any&&);
};

// ============================================================
// any_cast — pointer form (returns nullptr on type mismatch)
// ============================================================

template <typename T>
T* any_cast(any* operand) noexcept {
    if (operand && operand->handler_ &&
        operand->handler_->type() == typeid(T)) {
        return static_cast<T*>(operand->obj_ptr());
    }
    return nullptr;
}

template <typename T>
const T* any_cast(const any* operand) noexcept {
    return any_cast<T>(const_cast<any*>(operand));
}

// ============================================================
// any_cast — reference form (throws bad_any_cast on type mismatch)
// ============================================================

template <typename T>
T any_cast(any& operand) {
    auto* ptr = any_cast<std::remove_reference_t<T>>(&operand);
    if (!ptr) {
        throw bad_any_cast();
    }
    using ResultType =
        std::conditional_t<std::is_reference_v<T>, T, T&>;
    return static_cast<ResultType>(*ptr);
}

template <typename T>
T any_cast(const any& operand) {
    auto* ptr = any_cast<const std::remove_reference_t<T>>(&operand);
    if (!ptr) {
        throw bad_any_cast();
    }
    using ResultType =
        std::conditional_t<std::is_reference_v<T>, T, const T&>;
    return static_cast<ResultType>(*ptr);
}

// ============================================================
// any_cast — rvalue form (moves the value out)
// ============================================================

template <typename T>
T any_cast(any&& operand) {
    auto* ptr = any_cast<std::remove_reference_t<T>>(&operand);
    if (!ptr) {
        throw bad_any_cast();
    }
    return std::move(*ptr);
}

// ============================================================
// make_any<T> — construct an any from arguments
// ============================================================

template <typename T, typename... Args>
any make_any(Args&&... args) {
    return any(std::in_place_type<T>, std::forward<Args>(args)...);
}

} // namespace zero
