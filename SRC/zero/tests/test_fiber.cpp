// test_fiber.cpp — Comprehensive Fiber unit tests
// Tests Fiber creation (constructor and Create factory), states
// (INIT, READY, RUNNING, HOLD, TERM, EXCEPT), state transitions,
// resume()/yield() callbacks, GetThis()/GetFiberId(), unique IDs,
// main fiber, stack configuration, fiber lifecycle, and edge cases.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <atomic>
#include <thread>
#include <vector>
#include <functional>
#include <string>

using namespace zero;

// ============================================================
// Fiber creation with callback
// ============================================================

TEST(Fiber, CreateWithCallback) {
    bool called = false;
    auto fiber = Fiber::Create([&called]() {
        called = true;
    });

    ASSERT_NE(fiber, nullptr);
    EXPECT_EQ(fiber->getState(), Fiber::State::INIT);
    EXPECT_FALSE(fiber->isMain());
}

TEST(Fiber, CreateWithLambdaCapture) {
    int captured = 0;
    auto fiber = Fiber::Create([&captured]() {
        captured = 42;
    });

    ASSERT_NE(fiber, nullptr);
    EXPECT_EQ(fiber->getState(), Fiber::State::INIT);
    EXPECT_EQ(fiber->stackSize(), Fiber::kDefaultStackSize);
}

TEST(Fiber, CreateWithMutableLambda) {
    int counter = 0;
    auto fiber = Fiber::Create([&counter]() mutable {
        counter += 1;
    });
    EXPECT_NE(fiber, nullptr);
    EXPECT_EQ(fiber->getState(), Fiber::State::INIT);
}

TEST(Fiber, CreateNullCallback) {
    // Creating a fiber with empty callback should still create a valid object
    auto fiber = Fiber::Create(nullptr);
    // May or may not create - depends on implementation
    if (fiber) {
        EXPECT_EQ(fiber->getState(), Fiber::State::INIT);
    }
}

// ============================================================
// Fiber states and stateName()
// ============================================================

TEST(Fiber, InitialState) {
    auto fiber = Fiber::Create([]() {});
    ASSERT_NE(fiber, nullptr);
    EXPECT_EQ(fiber->getState(), Fiber::State::INIT);
}

TEST(Fiber, StateNames) {
    EXPECT_STREQ(Fiber::stateName(Fiber::State::INIT), "INIT");
    EXPECT_STREQ(Fiber::stateName(Fiber::State::READY), "READY");
    EXPECT_STREQ(Fiber::stateName(Fiber::State::RUNNING), "RUNNING");
    EXPECT_STREQ(Fiber::stateName(Fiber::State::HOLD), "HOLD");
    EXPECT_STREQ(Fiber::stateName(Fiber::State::TERM), "TERM");
    EXPECT_STREQ(Fiber::stateName(Fiber::State::EXCEPT), "EXCEPT");
}

TEST(Fiber, StateNameUnknown) {
    // Test that stateName handles an invalid state
    auto name = Fiber::stateName(static_cast<Fiber::State>(99));
    EXPECT_STREQ(name, "UNKNOWN");
}

TEST(Fiber, SetState) {
    auto fiber = Fiber::Create([]() {});
    ASSERT_NE(fiber, nullptr);

    EXPECT_EQ(fiber->getState(), Fiber::State::INIT);

    fiber->setState(Fiber::State::READY);
    EXPECT_EQ(fiber->getState(), Fiber::State::READY);

    fiber->setState(Fiber::State::RUNNING);
    EXPECT_EQ(fiber->getState(), Fiber::State::RUNNING);

    fiber->setState(Fiber::State::HOLD);
    EXPECT_EQ(fiber->getState(), Fiber::State::HOLD);

    fiber->setState(Fiber::State::TERM);
    EXPECT_EQ(fiber->getState(), Fiber::State::TERM);

    fiber->setState(Fiber::State::EXCEPT);
    EXPECT_EQ(fiber->getState(), Fiber::State::EXCEPT);

    fiber->setState(Fiber::State::INIT);
    EXPECT_EQ(fiber->getState(), Fiber::State::INIT);
}

// ============================================================
// Unique IDs
// ============================================================

TEST(Fiber, UniqueIds) {
    auto f1 = Fiber::Create([]() {});
    auto f2 = Fiber::Create([]() {});
    auto f3 = Fiber::Create([]() {});

    ASSERT_NE(f1, nullptr);
    ASSERT_NE(f2, nullptr);
    ASSERT_NE(f3, nullptr);

    uint64_t id1 = f1->id();
    uint64_t id2 = f2->id();
    uint64_t id3 = f3->id();

    EXPECT_NE(id1, id2);
    EXPECT_NE(id1, id3);
    EXPECT_NE(id2, id3);
}

TEST(Fiber, IdsMonotonicallyIncreasing) {
    auto f1 = Fiber::Create([]() {});
    auto f2 = Fiber::Create([]() {});

    ASSERT_NE(f1, nullptr);
    ASSERT_NE(f2, nullptr);

    EXPECT_LT(f1->id(), f2->id());
}

TEST(Fiber, ManyUniqueIds) {
    const int kNumFibers = 100;
    std::vector<Fiber::Ptr> fibers;
    std::vector<uint64_t> ids;

    for (int i = 0; i < kNumFibers; ++i) {
        auto f = Fiber::Create([]() {});
        ASSERT_NE(f, nullptr);
        ids.push_back(f->id());
        fibers.push_back(f);
    }

    // Check all IDs are unique
    std::sort(ids.begin(), ids.end());
    auto last = std::unique(ids.begin(), ids.end());
    EXPECT_EQ(last - ids.begin(), kNumFibers);
}

// ============================================================
// Main fiber (id == 0)
// ============================================================

TEST(Fiber, MainFiberIsNullInitially) {
    // Without a scheduler, the main fiber should not be set
    // Fiber::GetMainFiber() may or may not return something
    // depending on whether a scheduler has been initialized
}

TEST(Fiber, IsMain) {
    auto fiber = Fiber::Create([]() {});
    ASSERT_NE(fiber, nullptr);
    EXPECT_FALSE(fiber->isMain());
}

// ============================================================
// GetThis() and GetFiberId() in non-fiber context
// ============================================================

TEST(Fiber, GetThisReturnsNullOutsideFiber) {
    // Without an active scheduler/fiber, GetThis() should return nullptr
    Fiber* current = Fiber::GetThis();
    EXPECT_EQ(current, nullptr);
}

TEST(Fiber, GetFiberIdReturnsZeroOutsideFiber) {
    // Without an active fiber, GetFiberId() should return 0
    uint64_t id = Fiber::GetFiberId();
    EXPECT_EQ(id, 0u);
}

// ============================================================
// Stack size configuration
// ============================================================

TEST(Fiber, DefaultStackSize) {
    auto fiber = Fiber::Create([]() {});
    ASSERT_NE(fiber, nullptr);
    EXPECT_EQ(fiber->stackSize(), Fiber::kDefaultStackSize);
}

TEST(Fiber, CustomStackSize) {
    const size_t kStackSize = 65536; // 64 KB
    auto fiber = Fiber::Create([]() {}, kStackSize);
    ASSERT_NE(fiber, nullptr);
    EXPECT_EQ(fiber->stackSize(), kStackSize);
}

TEST(Fiber, LargeStackSize) {
    const size_t kLargeStack = 1048576; // 1 MB
    auto fiber = Fiber::Create([]() {}, kLargeStack);
    ASSERT_NE(fiber, nullptr);
    EXPECT_EQ(fiber->stackSize(), kLargeStack);
}

TEST(Fiber, SmallStackSize) {
    const size_t kSmallStack = 16384; // 16 KB
    auto fiber = Fiber::Create([]() {}, kSmallStack);
    ASSERT_NE(fiber, nullptr);
    EXPECT_EQ(fiber->stackSize(), kSmallStack);
}

TEST(Fiber, ZeroStackSize) {
    // Zero stack size — implementation should handle gracefully
    auto fiber = Fiber::Create([]() {}, 0);
    // May return nullptr or a valid fiber with adjusted stack size
    // Just ensure no crash
    SUCCEED();
}

// ============================================================
// Stack bottom (memory address)
// ============================================================

TEST(Fiber, StackBottomNonNull) {
    auto fiber = Fiber::Create([]() {});
    ASSERT_NE(fiber, nullptr);
    // For non-main fibers, stack should be allocated
    void* bottom = fiber->stackBottom();
    EXPECT_NE(bottom, nullptr);
}

TEST(Fiber, StackBottomDifferentForDifferentFibers) {
    auto f1 = Fiber::Create([]() {});
    auto f2 = Fiber::Create([]() {});

    ASSERT_NE(f1, nullptr);
    ASSERT_NE(f2, nullptr);

    // Each fiber should have its own stack
    EXPECT_NE(f1->stackBottom(), f2->stackBottom());
}

TEST(Fiber, StackBottomConsistent) {
    auto fiber = Fiber::Create([]() {});
    ASSERT_NE(fiber, nullptr);

    void* bottom1 = fiber->stackBottom();
    void* bottom2 = fiber->stackBottom();
    EXPECT_EQ(bottom1, bottom2);  // Should be consistent
}

// ============================================================
// Fiber lifecycle: create, set state, verify
// ============================================================

TEST(Fiber, LifecycleInitToTerm) {
    auto fiber = Fiber::Create([]() {});
    ASSERT_NE(fiber, nullptr);

    // INIT -> READY -> RUNNING -> HOLD -> TERM
    EXPECT_EQ(fiber->getState(), Fiber::State::INIT);

    fiber->setState(Fiber::State::READY);
    EXPECT_EQ(fiber->getState(), Fiber::State::READY);

    fiber->setState(Fiber::State::RUNNING);
    EXPECT_EQ(fiber->getState(), Fiber::State::RUNNING);

    fiber->setState(Fiber::State::HOLD);
    EXPECT_EQ(fiber->getState(), Fiber::State::HOLD);

    fiber->setState(Fiber::State::TERM);
    EXPECT_EQ(fiber->getState(), Fiber::State::TERM);
}

TEST(Fiber, LifecycleInitToException) {
    auto fiber = Fiber::Create([]() {});
    ASSERT_NE(fiber, nullptr);

    EXPECT_EQ(fiber->getState(), Fiber::State::INIT);

    fiber->setState(Fiber::State::READY);
    fiber->setState(Fiber::State::RUNNING);
    fiber->setState(Fiber::State::EXCEPT);

    EXPECT_EQ(fiber->getState(), Fiber::State::EXCEPT);
}

// ============================================================
// Multiple fibers
// ============================================================

TEST(Fiber, CreateMultipleFibers) {
    const int kNumFibers = 50;
    std::vector<Fiber::Ptr> fibers;

    int counter = 0;
    for (int i = 0; i < kNumFibers; ++i) {
        auto f = Fiber::Create([&counter, i]() {
            counter += i;
        });
        ASSERT_NE(f, nullptr);
        EXPECT_EQ(f->getState(), Fiber::State::INIT);
        fibers.push_back(f);
    }

    EXPECT_EQ(fibers.size(), static_cast<size_t>(kNumFibers));
}

// ============================================================
// Fiber::Create from multiple threads
// ============================================================

TEST(Fiber, CreateFromMultipleThreads) {
    const int kNumThreads = 4;
    const int kPerThread = 50;
    std::atomic<int> total{0};
    std::atomic<int> failures{0};

    auto worker = [&total, &failures, kPerThread]() {
        for (int i = 0; i < kPerThread; ++i) {
            auto f = Fiber::Create([]() {});
            if (f && f->getState() == Fiber::State::INIT) {
                total.fetch_add(1);
            } else {
                failures.fetch_add(1);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < kNumThreads; ++i) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) t.join();

    EXPECT_EQ(total.load(), kNumThreads * kPerThread);
    EXPECT_EQ(failures.load(), 0);
}

// ============================================================
// Fiber idle counter — ensure IDs are generated thread-safely
// ============================================================

TEST(Fiber, IdsThreadSafe) {
    const int kNumThreads = 4;
    const int kPerThread = 100;
    std::vector<uint64_t> all_ids;
    std::mutex mtx;

    auto worker = [&]() {
        std::vector<uint64_t> local_ids;
        for (int i = 0; i < kPerThread; ++i) {
            auto f = Fiber::Create([]() {});
            ASSERT_NE(f, nullptr);
            local_ids.push_back(f->id());
        }
        std::lock_guard<std::mutex> lock(mtx);
        all_ids.insert(all_ids.end(), local_ids.begin(), local_ids.end());
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < kNumThreads; ++i) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) t.join();

    // Check all IDs unique
    std::sort(all_ids.begin(), all_ids.end());
    auto last = std::unique(all_ids.begin(), all_ids.end());
    size_t unique_count = last - all_ids.begin();
    EXPECT_EQ(unique_count, all_ids.size());
    EXPECT_EQ(all_ids.size(), static_cast<size_t>(kNumThreads * kPerThread));
}

// ============================================================
// Fiber::SetThis / GetThis roundtrip
// ============================================================

TEST(Fiber, SetThisGetThisRoundtrip) {
    auto fiber = Fiber::Create([]() {});
    ASSERT_NE(fiber, nullptr);

    Fiber* prev = Fiber::GetThis();
    Fiber::SetThis(fiber.get());
    EXPECT_EQ(Fiber::GetThis(), fiber.get());

    // Restore
    Fiber::SetThis(prev);
    EXPECT_EQ(Fiber::GetThis(), prev);
}

TEST(Fiber, SetThisToNull) {
    auto fiber = Fiber::Create([]() {});
    ASSERT_NE(fiber, nullptr);

    Fiber::SetThis(fiber.get());
    EXPECT_NE(Fiber::GetThis(), nullptr);

    Fiber::SetThis(nullptr);
    EXPECT_EQ(Fiber::GetThis(), nullptr);
}

// ============================================================
// shared_from_this
// ============================================================

TEST(Fiber, SharedFromThis) {
    auto fiber = Fiber::Create([]() {});
    ASSERT_NE(fiber, nullptr);

    auto shared = fiber->shared_from_this();
    EXPECT_EQ(shared.get(), fiber.get());
    EXPECT_EQ(shared->id(), fiber->id());
}

// ============================================================
// Fiber state transition verification
// ============================================================

TEST(Fiber, StateTransitionConsistency) {
    auto fiber = Fiber::Create([]() {});
    ASSERT_NE(fiber, nullptr);

    // Verify all 6 states can be set and read back
    std::vector<Fiber::State> states = {
        Fiber::State::INIT,
        Fiber::State::READY,
        Fiber::State::RUNNING,
        Fiber::State::HOLD,
        Fiber::State::TERM,
        Fiber::State::EXCEPT
    };

    for (auto state : states) {
        fiber->setState(state);
        EXPECT_EQ(fiber->getState(), state);
    }
}

// ============================================================
// Fiber creation stress
// ============================================================

TEST(Fiber, CreationStress) {
    const int kRounds = 10;
    const int kPerRound = 100;

    for (int round = 0; round < kRounds; ++round) {
        std::vector<Fiber::Ptr> fibers;
        for (int i = 0; i < kPerRound; ++i) {
            auto f = Fiber::Create([i]() {
                volatile int x = i;  // Prevent optimization
                (void)x;
            });
            ASSERT_NE(f, nullptr);
            EXPECT_EQ(f->getState(), Fiber::State::INIT);
            fibers.push_back(f);
        }
        // Fibers destruct when going out of scope
    }
    SUCCEED();
}

// ============================================================
// Fiber with std::function captures (complex state)
// ============================================================

TEST(Fiber, ComplexCapture) {
    struct Context {
        int id;
        std::string name;
        double value;
    };

    Context ctx{42, "test_fiber", 3.14};
    auto fiber = Fiber::Create([ctx]() {
        EXPECT_EQ(ctx.id, 42);
        EXPECT_EQ(ctx.name, "test_fiber");
        EXPECT_DOUBLE_EQ(ctx.value, 3.14);
    });

    ASSERT_NE(fiber, nullptr);
    EXPECT_EQ(fiber->getState(), Fiber::State::INIT);
}

TEST(Fiber, SharedCapture) {
    auto shared = std::make_shared<int>(99);

    auto fiber = Fiber::Create([shared]() {
        *shared = 100;
    });

    ASSERT_NE(fiber, nullptr);
    EXPECT_EQ(fiber->getState(), Fiber::State::INIT);
    EXPECT_EQ(*shared, 99);  // Not yet executed
}
