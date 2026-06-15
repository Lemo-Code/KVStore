// TimerWheel 测试
#include "zero/scheduler/timer_wheel.h"
#include "zero/base/macro.h"

#include <cstdio>
#include <cassert>
#include <thread>
#include <chrono>

int main() {
    printf("=== TimerWheel Test ===\n");

    zero::TimerWheel wheel;

    // 初始状态
    assert(wheel.empty());
    assert(wheel.nextExpireMs() == ~0ull);

    // 先设置初始时钟 (避免从 0 开始)
    {
        std::vector<zero::TimerWheel::TimerCallback> dummy;
        wheel.tick(1000, dummy);  // 初始化时钟为 1000
    }

    // 添加一次性定时器 (基于已初始化的时钟)
    int fired = 0;
    wheel.addTimer(10, [&fired]() { fired++; });
    wheel.addTimer(20, [&fired]() { fired += 2; });
    wheel.addTimer(5,  [&fired]() { fired += 3; });

    assert(wheel.count() == 3);

    // Tick 到 1008ms: 5ms 和 10ms 的到期 (1010ms 还没到)
    {
        std::vector<zero::TimerWheel::TimerCallback> cbs;
        wheel.tick(1008, cbs);
        printf("Tick(1008): %zu callbacks\n", cbs.size());
        for (auto& cb : cbs) cb();
        assert(fired == 3);  // 5ms timer: 1000+5=1005 <= 1008
    }

    // Tick 到 1015ms: 10ms 的到期
    {
        std::vector<zero::TimerWheel::TimerCallback> cbs;
        wheel.tick(1015, cbs);
        printf("Tick(1015): %zu callbacks\n", cbs.size());
        for (auto& cb : cbs) cb();
        assert(fired == 4);  // +10ms timer: 1000+10=1010 <= 1015
    }

    // Tick 到 1025ms: 20ms 的到期
    {
        std::vector<zero::TimerWheel::TimerCallback> cbs;
        wheel.tick(1025, cbs);
        printf("Tick(1025): %zu callbacks\n", cbs.size());
        for (auto& cb : cbs) cb();
        assert(fired == 6);  // +20ms timer: 1000+20=1020 <= 1025
    }

    assert(wheel.empty());

    // 测试循环定时器 (从当前时间 1025ms + 50ms = 1075ms 开始)
    int loops = 0;
    auto id = wheel.addTimer(50, [&loops]() { loops++; }, true);  // recurring

    for (int i = 0; i < 5; ++i) {
        std::vector<zero::TimerWheel::TimerCallback> cbs;
        wheel.tick(1075 + i * 60, cbs);
        for (auto& cb : cbs) cb();
    }
    printf("Recurring timer fired: %d times\n", loops);
    assert(loops >= 3);

    // 测试取消
    wheel.cancelTimer(id);
    {
        std::vector<zero::TimerWheel::TimerCallback> cbs;
        wheel.tick(1500, cbs);
        for (auto& cb : cbs) cb();
    }
    printf("After cancel, loops: %d\n", loops);

    // 测试大量定时器 (用新的 wheel, 时间单调递增)
    zero::TimerWheel wheel2;
    int big_count = 0;
    // 初始时间戳
    {
        std::vector<zero::TimerWheel::TimerCallback> dummy;
        wheel2.tick(1000, dummy);
    }

    for (int i = 0; i < 1000; ++i) {
        wheel2.addTimer(100 + i % 50, [&big_count]() { big_count++; });
    }
    printf("Added 1000 timers, count=%zu\n", wheel2.count());

    {
        std::vector<zero::TimerWheel::TimerCallback> cbs;
        wheel2.tick(1200, cbs);
        printf("Tick(1200): %zu expired\n", cbs.size());
        for (auto& cb : cbs) cb();
    }
    printf("big_count=%d\n", big_count);
    assert(big_count == 1000);
    assert(wheel2.empty());

    printf("=== TimerWheel Test PASSED ===\n");
    return 0;
}
