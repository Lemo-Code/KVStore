#include <gtest/gtest.h>
#include <functional>
#include <vector>
#include "zero/scheduler/timer_wheel.h"
#include "zero/base/macro.h"

using namespace zero;

TEST(TimerWheelTest, FiresAfterDelay) {
    TimerWheel wheel;
    int fired = 0;
    wheel.addTimer(100, [&]() { fired++; });
    uint64_t base = GetCurrentMS();
    std::vector<std::function<void()>> cbs;
    wheel.tick(base, cbs);
    EXPECT_TRUE(cbs.empty());
    wheel.tick(base + 150, cbs);
    ASSERT_GE(cbs.size(), 1u);
    cbs[0]();
    EXPECT_EQ(fired, 1);
}

TEST(TimerWheelTest, RecurringTimer) {
    TimerWheel wheel;
    int count = 0;
    wheel.addTimer(50, [&]() { count++; }, true);
    uint64_t base = GetCurrentMS();
    std::vector<std::function<void()>> cbs;
    wheel.tick(base + 60, cbs);
    for (auto& cb : cbs) cb();
    EXPECT_GE(count, 1);
    cbs.clear();
    wheel.tick(base + 120, cbs);
    for (auto& cb : cbs) cb();
    EXPECT_GE(count, 2);
}

TEST(TimerWheelTest, CancelPreventsFire) {
    TimerWheel wheel;
    int fired = 0;
    uint64_t id = wheel.addTimer(100, [&]() { fired++; });
    uint64_t base = GetCurrentMS();
    EXPECT_TRUE(wheel.cancelTimer(id));
    std::vector<std::function<void()>> cbs;
    wheel.tick(base + 200, cbs);
    EXPECT_TRUE(cbs.empty());
    EXPECT_EQ(fired, 0);
}

TEST(TimerWheelTest, NextExpireMs) {
    TimerWheel wheel;
    EXPECT_EQ(wheel.nextExpireMs(), ~0ull);
    wheel.addTimer(500, []() {});
    EXPECT_NE(wheel.nextExpireMs(), ~0ull);
}

TEST(TimerWheelTest, MultipleTimersOrder) {
    TimerWheel wheel;
    std::vector<int> order;
    wheel.addTimer(300, [&]() { order.push_back(3); });
    wheel.addTimer(100, [&]() { order.push_back(1); });
    wheel.addTimer(200, [&]() { order.push_back(2); });
    uint64_t base = GetCurrentMS();
    std::vector<std::function<void()>> cbs;
    wheel.tick(base + 120, cbs);
    for (auto& cb : cbs) cb();
    EXPECT_FALSE(order.empty());
    EXPECT_EQ(order.front(), 1);
}
