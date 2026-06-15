/**
 * @file    context.h
 * @brief   Fiber context switching using POSIX ucontext.
 * @ingroup fiber
 */

#pragma once

#include <ucontext.h>

namespace zero {

struct FiberContext {
    ucontext_t uc;
    bool       valid;

    // Store entry function and arg here (makecontext only takes int args)
    void (*entry_func)(void*);
    void* entry_arg;
};

/// Initialize context to run entry_func(entry_arg) on the given stack.
void InitContext(FiberContext* ctx, void* stack, size_t stack_size,
                 void (*func)(void*), void* arg);

/// Save to `from`, restore `to`.
void SwapContext(FiberContext* from, FiberContext* to);

} // namespace zero
