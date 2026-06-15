// Fiber 基础测试: 创建、切换、yield
#include "zero/zero.h"

#include <iostream>
#include <cstdio>
#include <atomic>

static std::atomic<int> g_counter{0};
static std::atomic<int> g_done{0};

void fiber_func() {
    int id = g_counter.fetch_add(1);
    printf("  Fiber %d: started (tid=%u)\n", id, zero::GetThreadId());

    for (int i = 0; i < 3; ++i) {
        printf("  Fiber %d: yield %d\n", id, i);
        zero::Fiber::YieldToReady();
    }

    printf("  Fiber %d: done\n", id);
    ++g_done;
}

int main() {
    printf("=== Zero Fiber Test ===\n");
    printf("Main thread tid=%u, fibers=%lu\n",
           zero::GetThreadId(), zero::Fiber::GetTotalCount());

    // 创建调度器: 2 线程
    zero::Scheduler scheduler(2, false, "test_sched");
    scheduler.start();

    // 调度 5 个 fiber
    for (int i = 0; i < 5; ++i) {
        scheduler.schedule(std::bind(fiber_func));
    }

    // 等待所有 fiber 完成
    while (g_done.load() < 5) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    printf("All 5 fibers completed.\n");

    // 停止调度器
    scheduler.stop();

    printf("Scheduler stopped. Total fibers created: %lu\n",
           zero::Fiber::GetTotalCount());
    printf("=== Test PASSED ===\n");
    return 0;
}
