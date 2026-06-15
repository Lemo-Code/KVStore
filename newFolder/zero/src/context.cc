/**
 * @file    context.cc
 * @brief   POSIX ucontext-based fiber context switching.
 */

#include "zero/fiber/context.h"
#include <cstring>

namespace zero {

static thread_local void (*tls_entry_func)(void*) = nullptr;
static thread_local void* tls_entry_arg = nullptr;

static void Trampoline() {
    void (*func)(void*) = tls_entry_func;
    void* arg = tls_entry_arg;
    tls_entry_func = nullptr;
    tls_entry_arg = nullptr;
    func(arg);
}

void InitContext(FiberContext* ctx, void* stack, size_t stack_size,
                 void (*func)(void*), void* arg) {
    std::memset(ctx, 0, sizeof(FiberContext));

    ctx->entry_func = func;
    ctx->entry_arg  = arg;

    getcontext(&ctx->uc);
    ctx->uc.uc_stack.ss_sp   = stack;
    ctx->uc.uc_stack.ss_size = stack_size;
    ctx->uc.uc_link          = nullptr;

    makecontext(&ctx->uc, Trampoline, 0);
    ctx->valid = true;
}

void SwapContext(FiberContext* from, FiberContext* to) {
    // Set TLS only when entering a freshly-initialized context.
    // After the first swap, the context continues from swapcontext's
    // return point — we don't want to re-enter the trampoline.
    if (to->entry_func) {
        tls_entry_func = to->entry_func;
        tls_entry_arg  = to->entry_arg;
        to->entry_func = nullptr; // clear so subsequent swaps don't re-enter
    }

    swapcontext(&from->uc, &to->uc);
}

} // namespace zero
