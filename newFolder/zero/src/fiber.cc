/**
 * @file    fiber.cc
 * @brief   Fiber implementation.
 */

#include "zero/fiber/fiber.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
#include <stdexcept>

namespace zero {

// ---- Static members ----
std::atomic<uint64_t> Fiber::s_id_counter{1};
thread_local Fiber* Fiber::t_current_fiber = nullptr;
thread_local Fiber* Fiber::t_main_fiber = nullptr;

// =========================================================================
// Fiber constructor (user fiber)
// =========================================================================
Fiber::Fiber(Callback cb, size_t stack_size, bool)
    : id_(s_id_counter.fetch_add(1, std::memory_order_relaxed))
    , state_(READY)
    , is_main_(false)
    , stack_size_(stack_size > 0 ? stack_size : kDefaultStackSize)
    , stack_(nullptr)
    , cb_(std::move(cb))
    , back_(nullptr)
{
    // Allocate stack with guard page
    size_t total = stack_size_ + 4096;  // + guard page
    stack_ = mmap(nullptr, total, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack_ == MAP_FAILED) {
        throw std::bad_alloc();
    }

    // Set up guard page (lowest 4KB, no access)
    mprotect(stack_, 4096, PROT_NONE);

    // Usable stack starts after guard page
    void* usable_stack = static_cast<char*>(stack_) + 4096;
    InitContext(&ctx_, usable_stack, stack_size_, &Fiber::MainFunc, this);
}

// =========================================================================
// Fiber constructor (main fiber — represents the thread)
// =========================================================================
Fiber::Fiber()
    : id_(0)
    , state_(RUNNING)
    , is_main_(true)
    , stack_size_(0)
    , stack_(nullptr)
    , back_(nullptr)
{
    t_main_fiber = this;
    t_current_fiber = this;
}

// =========================================================================
// Fiber destructor
// =========================================================================
Fiber::~Fiber() {
    if (stack_ && !is_main_) {
        size_t total = stack_size_ + 4096;
        munmap(stack_, total);
    }
}

// =========================================================================
// Static entry point
// =========================================================================
void Fiber::MainFunc(void* arg) {
    Fiber* fiber = static_cast<Fiber*>(arg);
    // Execute the user's callback
    if (fiber->cb_) {
        fiber->cb_();
    }
    // Fiber finished — mark as terminated and yield back to scheduler
    fiber->state_ = TERM;
    // Yield back to whoever switched to us
    Fiber* back = fiber->back_;
    if (back) {
        fiber->back_ = nullptr;
        SetCurrent(back);
        back->state_ = RUNNING;
        SwapContext(&fiber->ctx_, &back->ctx_);
    }
    // Should never reach here
    std::abort();
}

// =========================================================================
// resume — switch TO this fiber
// =========================================================================
void Fiber::resume() {
    Fiber* current = GetCurrent();
    if (!current) return;

    back_ = current;
    state_ = RUNNING;
    SetCurrent(this);
    SwapContext(&current->ctx_, &ctx_);
}

// =========================================================================
// yield — switch back to the fiber that resumed us
// =========================================================================
void Fiber::yield() {
    Fiber* current = GetCurrent();
    Fiber* back = current->back_;
    if (back) {
        current->state_ = HOLD;
        current->back_ = nullptr;
        SetCurrent(back);
        back->state_ = RUNNING;
        SwapContext(&current->ctx_, &back->ctx_);
    }
}

// ---- Static accessors ----
Fiber* Fiber::GetCurrent()    { return t_current_fiber; }
void Fiber::SetCurrent(Fiber* f) { t_current_fiber = f; }
Fiber* Fiber::GetMainFiber() { return t_main_fiber; }

} // namespace zero
