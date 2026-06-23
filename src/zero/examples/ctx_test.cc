// 最小化测试: 直接测试 context_swap 和 fiber 切换
#include "zero/fiber/context.h"
#include "zero/fiber/stack_pool.h"
#include "zero/fiber/fiber.h"

#include <cstdio>
#include <cstdlib>
#include <thread>

static zero::Context main_ctx;
static zero::Context test_ctx;
static bool swap_ok = false;

void test_entry(void* arg) {
    printf("  [test_entry] entered! arg=%p\n", arg);
    swap_ok = true;
    // Swap back to main
    context_swap(&test_ctx, &main_ctx);
    printf("  [test_entry] back after second swap\n");
}

int main() {
    printf("=== Minimal context_swap test ===\n");

    auto& pool = zero::StackPool::GetInstance();
    void* stack = pool.allocate();
    printf("Stack: %p, size: %zu\n", stack, pool.stackSize());

    // Set up test context
    context_init(&test_ctx, stack, pool.stackSize(), test_entry, (void*)0xdead);

    printf("Before swap: test_ctx.sp=%p test_ctx.lr=%p\n",
           test_ctx.sp, test_ctx.lr);

    // Swap to test context
    printf("Calling context_swap...\n");
    context_swap(&main_ctx, &test_ctx);

    printf("Back in main! swap_ok=%d\n", swap_ok);

    if (swap_ok) {
        printf("=== Test PASSED ===\n");
    } else {
        printf("=== Test FAILED ===\n");
    }

    pool.deallocate(stack);
    return swap_ok ? 0 : 1;
}
