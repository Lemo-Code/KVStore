#include "lemo/fiber/fiber.h"

#include "lemo/fiber/scheduler.h"
#include "lemo/memory/stack_pool.h"

#include <cassert>
#include <cstdlib>

namespace lemo {
namespace fiber {

namespace {

static_assert(Fiber::kDefaultStackSize == lemo::memory::StackPool::kPooledStackSize,
              "Fiber default stack must match StackPool pooled size");

std::atomic<uint64_t> s_fiber_id{0};
std::atomic<uint64_t> s_fiber_count{0};

thread_local Fiber* t_fiber = nullptr;
thread_local Fiber::ptr t_thread_fiber = nullptr;

class PoolStackAllocator {
 public:
  static void* Alloc(size_t size) {
    return lemo::memory::StackPool::allocate(size);
  }
  static void Dealloc(void* vp, size_t size) {
    lemo::memory::StackPool::deallocate(vp, size);
  }
};

using StackAllocator = PoolStackAllocator;

}  // namespace

Fiber::Fiber() {
  state_ = EXEC;
  SetThis(this);
  if (getcontext(&ctx_) != 0) {
    assert(false && "Fiber::Fiber getcontext failed");
  }
  ++s_fiber_count;
}

Fiber::Fiber(std::function<void()> cb, size_t stacksize, bool use_caller)
    : id_(++s_fiber_id), cb_(std::move(cb)) {
  ++s_fiber_count;
  stack_size_ =
      static_cast<uint32_t>(stacksize > 0 ? stacksize : kDefaultStackSize);

  stack_ = StackAllocator::Alloc(stack_size_);

  if (getcontext(&ctx_) != 0) {
    assert(false && "Fiber::Fiber getcontext failed");
  }

  ctx_.uc_link = nullptr;
  ctx_.uc_stack.ss_sp = stack_;
  ctx_.uc_stack.ss_size = stack_size_;

  if (!use_caller) {
    makecontext(&ctx_, &Fiber::MainFunc, 0);
  } else {
    makecontext(&ctx_, &Fiber::CallerMainFunc, 0);
  }
  bootstrap_ctx_ = ctx_;
  bootstrap_saved_ = true;
}

Fiber::~Fiber() {
  --s_fiber_count;
  if (stack_) {
    assert(state_ == TERM || state_ == INIT || state_ == EXCEPT);
    StackAllocator::Dealloc(stack_, stack_size_);
  } else {
    assert(!cb_);
    assert(state_ == EXEC);
    if (t_fiber == this) {
      SetThis(nullptr);
    }
  }
}

void Fiber::reset(std::function<void()> cb) {
  assert(stack_);
  assert(state_ == TERM || state_ == INIT || state_ == EXCEPT);
  cb_ = std::move(cb);

  if (bootstrap_saved_) {
    ctx_ = bootstrap_ctx_;
    ctx_.uc_link = nullptr;
    ctx_.uc_stack.ss_sp = stack_;
    ctx_.uc_stack.ss_size = stack_size_;
  } else if (getcontext(&ctx_) != 0) {
    assert(false && "Fiber::reset getcontext failed");
  } else {
    ctx_.uc_link = nullptr;
    ctx_.uc_stack.ss_sp = stack_;
    ctx_.uc_stack.ss_size = stack_size_;
  }

  makecontext(&ctx_, &Fiber::MainFunc, 0);
  state_ = INIT;
}

Fiber* Fiber::GetSwapMainFiber() {
  Fiber* scheduler_main = Scheduler::GetMainFiber();
  if (scheduler_main != nullptr) {
    return scheduler_main;
  }
  if (t_thread_fiber == nullptr) {
    GetThis();
  }
  return t_thread_fiber.get();
}

void Fiber::swapOut() {
  Fiber* main_fiber = GetSwapMainFiber();
  assert(main_fiber != nullptr);
  SetThis(main_fiber);
  if (swapcontext(&ctx_, &main_fiber->ctx_) != 0) {
    assert(false && "Fiber::swapOut swapcontext failed");
  }
}

void Fiber::swapIn() {
  Fiber* main_fiber = GetSwapMainFiber();
  assert(main_fiber != nullptr);
  SetThis(this);
  assert(state_ != EXEC);
  state_ = EXEC;
  if (swapcontext(&main_fiber->ctx_, &ctx_) != 0) {
    assert(false && "Fiber::swapIn swapcontext failed");
  }
}

void Fiber::call() {
  assert(t_thread_fiber != nullptr);
  SetThis(this);
  state_ = EXEC;
  if (swapcontext(&t_thread_fiber->ctx_, &ctx_) != 0) {
    assert(false && "Fiber::call swapcontext failed");
  }
}

void Fiber::back() {
  assert(t_thread_fiber != nullptr);
  SetThis(t_thread_fiber.get());
  if (swapcontext(&ctx_, &t_thread_fiber->ctx_) != 0) {
    assert(false && "Fiber::back swapcontext failed");
  }
}

void Fiber::MainFunc() {
  Fiber::ptr cur = GetThis();
  assert(cur);
  try {
    cur->cb_();
    cur->cb_ = nullptr;
    cur->state_ = TERM;
  } catch (...) {
    cur->state_ = EXCEPT;
  }

  Fiber* raw = cur.get();
  cur.reset();
  raw->swapOut();
  assert(false && "Fiber::MainFunc should not return");
}

void Fiber::CallerMainFunc() {
  Fiber::ptr cur = GetThis();
  assert(cur);
  try {
    cur->cb_();
    cur->cb_ = nullptr;
    cur->state_ = TERM;
  } catch (...) {
    cur->state_ = EXCEPT;
  }

  Fiber* raw = cur.get();
  cur.reset();
  raw->back();
  assert(false && "Fiber::CallerMainFunc should not return");
}

void Fiber::YieldToHold() {
  Fiber::ptr cur = GetThis();
  assert(cur->state_ == EXEC);
  cur->state_ = HOLD;
  cur->swapOut();
}

void Fiber::YieldToReady() {
  Fiber::ptr cur = GetThis();
  assert(cur->state_ == EXEC);
  cur->state_ = READY;
  cur->swapOut();
}

void Fiber::SleepMs(uint64_t ms) {
  Scheduler* sch = Scheduler::GetThis();
  assert(sch != nullptr);
  Fiber::ptr self = GetThis();
  sch->addTimer(ms, [sch, self]() { sch->schedule(self); });
  YieldToHold();
}

uint32_t Fiber::GetFiberId() {
  if (t_fiber != nullptr) {
    return static_cast<uint32_t>(t_fiber->getId());
  }
  return 0;
}

Fiber::ptr Fiber::GetThis() {
  if (t_fiber != nullptr) {
    return t_fiber->shared_from_this();
  }
  Fiber::ptr main_fiber(new Fiber);
  assert(t_fiber == main_fiber.get());
  t_thread_fiber = main_fiber;
  return t_fiber->shared_from_this();
}

void Fiber::SetThis(Fiber* fiber) { t_fiber = fiber; }

uint64_t Fiber::GetTotalCount() { return s_fiber_count.load(); }

uint32_t GetCurrentFiberId() { return Fiber::GetFiberId(); }

}  // namespace fiber
}  // namespace lemo
