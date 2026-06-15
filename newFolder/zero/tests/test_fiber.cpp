/**
 * @file    test_fiber.cpp
 * @brief   Fiber context switching test.
 */
#include "zero/fiber/fiber.h"
#include <cstdio>
#include <cassert>

using namespace zero;

static int g_counter = 0;

int main() {
    printf("=== Fiber context switch test ===\n");

    Fiber main_fiber;
    assert(Fiber::GetCurrent() == &main_fiber);
    assert(g_counter == 0);

    Fiber f1([&]() {
        printf("  fiber: step 1 (counter=%d)\n", ++g_counter);
        Fiber::GetCurrent()->yield();
        printf("  fiber: step 2 (counter=%d)\n", ++g_counter);
        Fiber::GetCurrent()->yield();
        printf("  fiber: step 3 (counter=%d)\n", ++g_counter);
    });

    printf("  main: resume fiber\n");
    f1.resume();
    assert(g_counter == 1);
    assert(Fiber::GetCurrent() == &main_fiber);

    printf("  main: resume fiber again\n");
    f1.resume();
    assert(g_counter == 2);

    printf("  main: resume fiber final time\n");
    f1.resume();
    assert(g_counter == 3);

    printf("=== PASS ===\n");
    return 0;
}
