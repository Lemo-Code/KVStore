// test_singleton.cpp — Singleton pattern unit tests
// Tests Meyers' singleton instance identity, lazy initialization,
// thread safety (basic), and non-copyable/non-movable enforcement.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <atomic>
#include <thread>
#include <type_traits>
#include <vector>

using namespace zero;

// ============================================================
// Test singleton types
// ============================================================

struct TestSingleton : public Singleton<TestSingleton> {
    friend class Singleton<TestSingleton>;
    int value = 0;

    // Protected constructor prevents direct instantiation
protected:
    TestSingleton() = default;
    ~TestSingleton() = default;
};

struct CounterSingleton : public Singleton<CounterSingleton> {
    friend class Singleton<CounterSingleton>;
    inline static std::atomic<int> construct_count{0};
    int id = 0;

protected:
    CounterSingleton() {
        id = construct_count.fetch_add(1);
    }
    ~CounterSingleton() = default;
};

// Using the ZERO_SINGLETON macro
class MacroSingleton {
public:
    int value = 0;

private:
    ZERO_SINGLETON(MacroSingleton);
};

// ============================================================
// instance() identity
// ============================================================

TEST(Singleton, SameInstance) {
    TestSingleton& a = TestSingleton::instance();
    TestSingleton& b = TestSingleton::instance();
    EXPECT_EQ(&a, &b);
}

TEST(Singleton, PtrReturnsAddressOfInstance) {
    EXPECT_EQ(TestSingleton::ptr(), &TestSingleton::instance());
}

TEST(Singleton, GetReturnsSameAsInstance) {
    auto& a = TestSingleton::instance();
    auto& b = TestSingleton::get();
    EXPECT_EQ(&a, &b);
}

// ============================================================
// State preservation
// ============================================================

TEST(Singleton, StatePreserved) {
    TestSingleton::instance().value = 42;
    EXPECT_EQ(TestSingleton::instance().value, 42);
    TestSingleton::instance().value = 100;
    EXPECT_EQ(TestSingleton::instance().value, 100);
    TestSingleton::instance().value = 0; // Reset
}

TEST(Singleton, ModificationPersistsAcrossCalls) {
    Singleton<MacroSingleton>::instance().value = 7;
    EXPECT_EQ(Singleton<MacroSingleton>::instance().value, 7);
    Singleton<MacroSingleton>::instance().value = 0;
}

// ============================================================
// Lazy initialization (constructed exactly once)
// ============================================================

TEST(Singleton, ConstructedExactlyOnce) {
    CounterSingleton::construct_count.store(0);
    // First access triggers construction
    EXPECT_EQ(CounterSingleton::instance().id, 0);
    EXPECT_EQ(CounterSingleton::construct_count.load(), 1);
    // Second access does not trigger construction
    EXPECT_EQ(CounterSingleton::instance().id, 0);
    EXPECT_EQ(CounterSingleton::construct_count.load(), 1);
}

// ============================================================
// Thread safety (basic): concurrent access returns same instance
// ============================================================

TEST(Singleton, ThreadSafeSameInstance) {
    const int num_threads = 8;
    std::vector<std::thread> threads;
    std::vector<TestSingleton*> ptrs(num_threads, nullptr);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            ptrs[i] = &TestSingleton::instance();
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All threads should see the same pointer
    for (int i = 1; i < num_threads; ++i) {
        EXPECT_EQ(ptrs[i], ptrs[0]);
    }
}

TEST(Singleton, ThreadSafeLazyInit) {
    // Ensure exactly one construction even with concurrent access
    CounterSingleton::construct_count.store(0);
    const int num_threads = 16;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            (void)CounterSingleton::instance().id;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(CounterSingleton::construct_count.load(), 1);
}

// ============================================================
// Non-copyable / Non-movable enforcement
// ============================================================

TEST(Singleton, NonCopyable) {
    static_assert(!std::is_copy_constructible_v<TestSingleton>,
                  "Singleton must not be copy-constructible");
    static_assert(!std::is_copy_assignable_v<TestSingleton>,
                  "Singleton must not be copy-assignable");
}

TEST(Singleton, NonMovable) {
    // Singleton inherits Noncopyable which allows move by default,
    // but ZERO_SINGLETON macro explicitly deletes move
    static_assert(!std::is_move_constructible_v<MacroSingleton>,
                  "MacroSingleton must not be move-constructible");
    static_assert(!std::is_move_assignable_v<MacroSingleton>,
                  "MacroSingleton must not be move-assignable");
}

// ============================================================
// Instance() returns reference (not copy)
// ============================================================

TEST(Singleton, InstanceReturnsReference) {
    using instance_ret_type = decltype(TestSingleton::instance());
    static_assert(std::is_reference_v<instance_ret_type>,
                  "instance() must return a reference");
    static_assert(std::is_same_v<instance_ret_type, TestSingleton&>,
                  "instance() must return T&");
}

TEST(Singleton, GetReturnsReference) {
    using get_ret_type = decltype(TestSingleton::get());
    static_assert(std::is_reference_v<get_ret_type>,
                  "get() must return a reference");
}

// ============================================================
// Multiple singletons are independent
// ============================================================

TEST(Singleton, IndependentInstances) {
    TestSingleton::instance().value = 1;
    Singleton<MacroSingleton>::instance().value = 2;

    EXPECT_EQ(TestSingleton::instance().value, 1);
    EXPECT_EQ(Singleton<MacroSingleton>::instance().value, 2);

    TestSingleton::instance().value = 0;
    Singleton<MacroSingleton>::instance().value = 0;
}

// ============================================================
// Empty / zero-value singleton
// ============================================================

TEST(Singleton, DefaultZeroValue) {
    EXPECT_EQ(Singleton<MacroSingleton>::instance().value, 0);
}
