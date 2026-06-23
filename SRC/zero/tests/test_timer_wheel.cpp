// test_timer_wheel.cpp — Unit tests for the 5-level hierarchical TimerWheel
//
// Tests: add timer with delay, tick advancement, timer fires at correct time,
// cancel timer, multiple timers fire in order, recurring timers, zero-delay
// timer, same deadline ordering, large number of timers (1000+), next_timeout.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <atomic>
#include <vector>
#include <set>
#include <algorithm>
#include <thread>
#include <chrono>

using namespace zero;

// =====================================================================
// Basic construction and properties
// =====================================================================

TEST(TimerWheelTest, Construct) {
    TimerWheel wheel;
    EXPECT_EQ(wheel.now_ms(), 0u);
    EXPECT_EQ(wheel.total_fired(), 0u);
    EXPECT_EQ(wheel.total_cancelled(), 0u);
    EXPECT_EQ(wheel.active_timers(), 0u);
    EXPECT_EQ(wheel.next_timeout_ms(), 1);
}

TEST(TimerWheelTest, NowMsStartsAtZero) {
    TimerWheel wheel;
    EXPECT_EQ(wheel.now_ms(), 0u);
}

TEST(TimerWheelTest, NextTimeoutMsReturnsOne) {
    TimerWheel wheel;
    EXPECT_EQ(wheel.next_timeout_ms(), 1);
}

// =====================================================================
// Simple timer firing tests
// =====================================================================

TEST(TimerWheelTest, AddTimerAndTickToFire) {
    TimerWheel wheel;
    std::atomic<int> fired{0};

    wheel.add_timer(5, [&fired]() {
        fired.fetch_add(1);
    });

    EXPECT_EQ(wheel.active_timers(), 1u);

    // Tick 4 times — timer should NOT fire yet
    for (int i = 0; i < 4; ++i) {
        int count = wheel.tick();
        EXPECT_EQ(count, 0);
    }

    EXPECT_EQ(fired.load(), 0);
    EXPECT_EQ(wheel.now_ms(), 5u);  // 5 ticks (tick is called after the 4th, wait...)

    // 5th tick should fire
    int count = wheel.tick();
    EXPECT_EQ(count, 1);
    EXPECT_EQ(fired.load(), 1);
    EXPECT_EQ(wheel.total_fired(), 1u);
    EXPECT_EQ(wheel.active_timers(), 0u);
}

TEST(TimerWheelTest, AddTimerWithZeroDelay) {
    TimerWheel wheel;
    std::atomic<int> fired{0};

    wheel.add_timer(0, [&fired]() {
        fired.fetch_add(1);
    });

    // Zero delay should fire on the very next tick
    int count = wheel.tick();
    EXPECT_GE(count, 1);
    EXPECT_EQ(fired.load(), 1);
}

TEST(TimerWheelTest, AddTimerWithOneMsDelay) {
    TimerWheel wheel;
    std::atomic<int> fired{0};

    wheel.add_timer(1, [&fired]() {
        fired.fetch_add(1);
    });

    // Should fire after exactly 1 tick
    int count = wheel.tick();
    EXPECT_EQ(count, 1);
    EXPECT_EQ(fired.load(), 1);
}

// =====================================================================
// Timer fires at correct time
// =====================================================================

TEST(TimerWheelTest, TimerFiresAtCorrectTime_ShortDelay) {
    TimerWheel wheel;
    std::atomic<int> fired{0};
    std::atomic<uint64_t> fire_time{0};

    wheel.add_timer(10, [&fired, &fire_time, &wheel]() {
        fired.store(1);
        fire_time.store(wheel.now_ms());
    });

    // Tick until fire
    int total_fired = 0;
    for (int i = 0; i < 10; ++i) {
        total_fired += wheel.tick();
    }

    EXPECT_EQ(total_fired, 1);
    EXPECT_EQ(fired.load(), 1);
    EXPECT_GE(fire_time.load(), 10u);
}

TEST(TimerWheelTest, TimerFiresAtCorrectTime_MediumDelay) {
    TimerWheel wheel;
    std::atomic<int> fired{0};
    std::atomic<uint64_t> fire_time{0};

    wheel.add_timer(100, [&fired, &fire_time, &wheel]() {
        fired.store(1);
        fire_time.store(wheel.now_ms());
    });

    // Tick until fire
    int total_fired = 0;
    for (int i = 0; i < 100; ++i) {
        total_fired += wheel.tick();
    }

    EXPECT_EQ(total_fired, 1);
    EXPECT_EQ(fired.load(), 1);
    EXPECT_GE(fire_time.load(), 100u);
}

TEST(TimerWheelTest, TimerFiresAtCorrectTime_LongDelay) {
    TimerWheel wheel;
    std::atomic<int> fired{0};
    std::atomic<uint64_t> fire_time{0};

    // 500ms delay
    wheel.add_timer(500, [&fired, &fire_time, &wheel]() {
        fired.store(1);
        fire_time.store(wheel.now_ms());
    });

    // Tick until fire
    int total_fired = 0;
    for (int i = 0; i < 500; ++i) {
        total_fired += wheel.tick();
    }

    EXPECT_EQ(total_fired, 1);
    EXPECT_EQ(fired.load(), 1);
    EXPECT_GE(fire_time.load(), 500u);
}

// =====================================================================
// Cancel timer tests
// =====================================================================

TEST(TimerWheelTest, CancelTimerBeforeFire) {
    TimerWheel wheel;
    std::atomic<int> fired{0};

    uint64_t id = wheel.add_timer(100, [&fired]() {
        fired.fetch_add(1);
    });

    EXPECT_NE(id, 0u);
    EXPECT_EQ(wheel.active_timers(), 1u);

    bool cancelled = wheel.cancel_timer(id);
    EXPECT_TRUE(cancelled);
    EXPECT_EQ(wheel.total_cancelled(), 1u);
    EXPECT_EQ(wheel.active_timers(), 0u);

    // Tick past the deadline
    for (int i = 0; i < 200; ++i) {
        wheel.tick();
    }

    EXPECT_EQ(fired.load(), 0);
    EXPECT_EQ(wheel.total_fired(), 0u);
}

TEST(TimerWheelTest, CancelAlreadyFiredTimer) {
    TimerWheel wheel;
    std::atomic<int> fired{0};

    uint64_t id = wheel.add_timer(1, [&fired]() {
        fired.fetch_add(1);
    });

    wheel.tick(); // Fires the timer
    EXPECT_EQ(fired.load(), 1);

    bool cancelled = wheel.cancel_timer(id);
    EXPECT_FALSE(cancelled);
}

TEST(TimerWheelTest, CancelInvalidId) {
    TimerWheel wheel;
    bool cancelled = wheel.cancel_timer(99999);
    EXPECT_FALSE(cancelled);
}

TEST(TimerWheelTest, CancelAndAddNewTimer) {
    TimerWheel wheel;
    std::atomic<int> fired1{0};
    std::atomic<int> fired2{0};

    uint64_t id1 = wheel.add_timer(50, [&fired1]() {
        fired1.fetch_add(1);
    });

    wheel.cancel_timer(id1);

    wheel.add_timer(5, [&fired2]() {
        fired2.fetch_add(1);
    });

    for (int i = 0; i < 10; ++i) {
        wheel.tick();
    }

    EXPECT_EQ(fired1.load(), 0);
    EXPECT_EQ(fired2.load(), 1);
}

// =====================================================================
// Multiple timers fire in order
// =====================================================================

TEST(TimerWheelTest, MultipleTimersFireInOrder) {
    TimerWheel wheel;
    std::vector<int> fire_order;

    wheel.add_timer(30, [&fire_order]() { fire_order.push_back(3); });
    wheel.add_timer(10, [&fire_order]() { fire_order.push_back(1); });
    wheel.add_timer(20, [&fire_order]() { fire_order.push_back(2); });
    wheel.add_timer(5,  [&fire_order]() { fire_order.push_back(0); });

    EXPECT_EQ(wheel.active_timers(), 4u);

    // Tick through all
    for (int i = 0; i < 35; ++i) {
        wheel.tick();
    }

    EXPECT_EQ(fire_order.size(), 4u);
    // They should fire in order of deadline: 5, 10, 20, 30
    EXPECT_EQ(fire_order[0], 0);
    EXPECT_EQ(fire_order[1], 1);
    EXPECT_EQ(fire_order[2], 2);
    EXPECT_EQ(fire_order[3], 3);
}

TEST(TimerWheelTest, MultipleTimersSameDeadline) {
    TimerWheel wheel;
    std::atomic<int> total_fired{0};

    const int kNumTimers = 10;
    for (int i = 0; i < kNumTimers; ++i) {
        wheel.add_timer(42, [&total_fired]() {
            total_fired.fetch_add(1);
        });
    }

    EXPECT_EQ(wheel.active_timers(), static_cast<size_t>(kNumTimers));

    // Tick past 42
    for (int i = 0; i < 50; ++i) {
        wheel.tick();
    }

    EXPECT_EQ(total_fired.load(), kNumTimers);
    EXPECT_EQ(wheel.total_fired(), static_cast<uint64_t>(kNumTimers));
}

// =====================================================================
// Recurring timers
// =====================================================================

TEST(TimerWheelTest, RecurringTimerFiresMultipleTimes) {
    TimerWheel wheel;
    std::atomic<int> fire_count{0};

    // Recurring: re-add the timer inside the callback
    std::function<void()> recurring;
    recurring = [&recurring, &fire_count, &wheel]() {
        fire_count.fetch_add(1);
        if (fire_count.load() < 5) {
            wheel.add_timer(5, recurring);
        }
    };

    wheel.add_timer(5, recurring);

    // Tick enough for 5 firings
    for (int i = 0; i < 30; ++i) {
        wheel.tick();
    }

    EXPECT_EQ(fire_count.load(), 5);
}

TEST(TimerWheelTest, RecurringTimerWithVaryingDelays) {
    TimerWheel wheel;
    std::atomic<int> fire_count{0};
    std::vector<uint64_t> fire_times;

    std::function<void()> recurring;
    recurring = [&recurring, &fire_count, &fire_times, &wheel]() {
        int count = fire_count.fetch_add(1);
        fire_times.push_back(wheel.now_ms());
        if (count < 3) {
            // Varying delays: 10, 20, 30
            wheel.add_timer(10 * (count + 1), recurring);
        }
    };

    wheel.add_timer(10, recurring);

    for (int i = 0; i < 100; ++i) {
        wheel.tick();
    }

    EXPECT_EQ(fire_count.load(), 4);
    EXPECT_EQ(fire_times.size(), 4u);
}

// =====================================================================
// Large number of timers (1000+)
// =====================================================================

TEST(TimerWheelTest, LargeNumberOfTimers) {
    TimerWheel wheel;
    std::atomic<int> total_fired{0};

    const int kNumTimers = 1000;
    for (int i = 0; i < kNumTimers; ++i) {
        // Spread delays between 1 and 500 ms
        uint64_t delay = static_cast<uint64_t>(1 + (i % 500));
        wheel.add_timer(delay, [&total_fired]() {
            total_fired.fetch_add(1);
        });
    }

    EXPECT_EQ(wheel.active_timers(), static_cast<size_t>(kNumTimers));

    // Tick enough for all timers to fire (max was 500)
    for (int i = 0; i < 600; ++i) {
        wheel.tick();
    }

    EXPECT_EQ(total_fired.load(), kNumTimers);
    EXPECT_EQ(wheel.total_fired(), static_cast<uint64_t>(kNumTimers));
    EXPECT_EQ(wheel.active_timers(), 0u);
}

TEST(TimerWheelTest, LargeNumberOfTimersWithCancellation) {
    TimerWheel wheel;
    std::atomic<int> total_fired{0};
    std::vector<uint64_t> ids;

    const int kNumTimers = 500;
    for (int i = 0; i < kNumTimers; ++i) {
        uint64_t id = wheel.add_timer(100, [&total_fired]() {
            total_fired.fetch_add(1);
        });
        ids.push_back(id);
    }

    // Cancel every other timer
    int cancelled = 0;
    for (size_t i = 0; i < ids.size(); i += 2) {
        if (wheel.cancel_timer(ids[i])) {
            ++cancelled;
        }
    }

    // Tick past deadline
    for (int i = 0; i < 120; ++i) {
        wheel.tick();
    }

    int expected_fired = kNumTimers - cancelled;
    EXPECT_EQ(total_fired.load(), expected_fired);
}

// =====================================================================
// Timer ordering with same deadline
// =====================================================================

TEST(TimerWheelTest, SameDeadlineAllFireTogether) {
    TimerWheel wheel;
    std::atomic<int> count{0};

    for (int i = 0; i < 100; ++i) {
        wheel.add_timer(25, [&count]() {
            count.fetch_add(1);
        });
    }

    for (int i = 0; i < 24; ++i) {
        int fired = wheel.tick();
        EXPECT_EQ(fired, 0);
    }

    int fired = wheel.tick(); // 25th tick
    EXPECT_EQ(fired, 100);
    EXPECT_EQ(count.load(), 100);
}

// =====================================================================
// Edge cases
// =====================================================================

TEST(TimerWheelTest, TickManyTimesBeyondAllTimers) {
    TimerWheel wheel;
    std::atomic<int> fired{0};

    wheel.add_timer(10, [&fired]() { fired.fetch_add(1); });

    // Tick well beyond the timer
    for (int i = 0; i < 1000; ++i) {
        wheel.tick();
    }

    EXPECT_EQ(fired.load(), 1);
    EXPECT_EQ(wheel.now_ms(), 1000u);
}

TEST(TimerWheelTest, AddTimerAfterTicks) {
    TimerWheel wheel;

    // Advance time first
    for (int i = 0; i < 100; ++i) {
        wheel.tick();
    }
    EXPECT_EQ(wheel.now_ms(), 100u);

    // Add a timer now — it should fire at now + delay
    std::atomic<int> fired{0};
    wheel.add_timer(10, [&fired]() { fired.fetch_add(1); });

    for (int i = 0; i < 10; ++i) {
        wheel.tick();
    }

    EXPECT_EQ(fired.load(), 1);
    EXPECT_EQ(wheel.now_ms(), 110u);
}

TEST(TimerWheelTest, CancelDuringCallback) {
    TimerWheel wheel;
    std::atomic<int> first_fired{0};
    std::atomic<int> second_fired{0};

    uint64_t second_id = 0;

    wheel.add_timer(5, [&first_fired, &second_id, &wheel]() {
        first_fired.store(1);
        // Cancel the second timer from within the first's callback
        if (second_id != 0) {
            wheel.cancel_timer(second_id);
        }
    });

    second_id = wheel.add_timer(10, [&second_fired]() {
        second_fired.store(1);
    });

    for (int i = 0; i < 15; ++i) {
        wheel.tick();
    }

    EXPECT_EQ(first_fired.load(), 1);
    EXPECT_EQ(second_fired.load(), 0);
}

TEST(TimerWheelTest, StatisticsAfterMixedOperations) {
    TimerWheel wheel;

    wheel.add_timer(10, []() {});
    wheel.add_timer(20, []() {});
    uint64_t id3 = wheel.add_timer(30, []() {});
    wheel.add_timer(40, []() {});

    wheel.cancel_timer(id3);

    for (int i = 0; i < 50; ++i) {
        wheel.tick();
    }

    EXPECT_EQ(wheel.total_fired(), 3u);
    EXPECT_EQ(wheel.total_cancelled(), 1u);
    EXPECT_EQ(wheel.active_timers(), 0u);
}

TEST(TimerWheelTest, MaxDelayTimer) {
    TimerWheel wheel;
    std::atomic<int> fired{0};

    // Add a timer with a very large delay (testing range)
    uint64_t large_delay = 2000; // 2 seconds in ms
    wheel.add_timer(large_delay, [&fired]() {
        fired.store(1);
    });

    // Tick partway
    for (uint64_t i = 0; i < large_delay - 1; ++i) {
        wheel.tick();
    }
    EXPECT_EQ(fired.load(), 0);

    // Final tick
    wheel.tick();
    EXPECT_EQ(fired.load(), 1);
}
