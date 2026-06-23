// zero Singleton — thread-safe Meyers' singleton pattern
// Guarantees single instance per process, lazy-initialized, thread-safe
// via C++11 magic statics. Destruction is in reverse order of construction.
#pragma once

#include "zero/base/noncopyable.h"

namespace zero {

// Singleton<T> — CRTP base class providing a singleton entry point.
// Usage:
//   class MyClass : public Singleton<MyClass> {
//       friend class Singleton<MyClass>;
//       MyClass() = default;      // constructor private
//       ~MyClass() = default;     // destructor private
//   };
// then: MyClass::instance() to get the single instance.
template <typename T>
class Singleton : public Noncopyable {
public:
    // Returns the single instance of T. Thread-safe, lazy-initialized.
    // The instance is a local static inside the function body (Meyers' singleton),
    // guaranteeing exactly one construction across all threads.
    static T& instance() noexcept {
        static T inst;
        return inst;
    }

    // Alias for instance()
    static T& get() noexcept {
        return instance();
    }

    // Convenience pointer accessor
    static T* ptr() noexcept {
        return &instance();
    }

protected:
    Singleton() = default;
    ~Singleton() = default;
};

// Convenience macro to declare a class as a singleton.
// Place inside the private section of the class definition.
#define ZERO_SINGLETON(ClassName)                          \
    friend class ::zero::Singleton<ClassName>;             \
    ClassName() = default;                                 \
    ~ClassName() = default;                                \
    ClassName(const ClassName&) = delete;                  \
    ClassName& operator=(const ClassName&) = delete;       \
    ClassName(ClassName&&) noexcept = delete;              \
    ClassName& operator=(ClassName&&) noexcept = delete

} // namespace zero
