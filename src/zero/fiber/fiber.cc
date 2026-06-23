#include "zero/fiber/fiber.h"
#include "zero/fiber/stack_pool.h"
#include "zero/fiber/fiber_pool.h"
#include "zero/scheduler/scheduler.h"
#include "zero/base/macro.h"
#include "zero/log/log.h"

#include <cstring>
#include <exception>

namespace zero {

std::atomic<uint64_t> Fiber::s_fiber_id{0};
std::atomic<uint64_t> Fiber::s_fiber_count{0};
thread_local Fiber* Fiber::t_this_fiber = nullptr;

static constexpr size_t kDefaultStackSize = 128 * 1024;

// ====================================================================
// 构造函数
// ====================================================================

Fiber::Fiber()
    : id_(0)
    , state_(RUNNING)
    , name_("main")
    , stack_(nullptr)
    , stack_size_(0) {
    SetThis(this);
    ++s_fiber_count;
}

Fiber::Fiber(Callback cb, size_t stack_size, std::string name)
    : id_(++s_fiber_id)
    , state_(INIT)
    , cb_(std::move(cb))
    , name_(std::move(name)) {

    if (stack_size == 0) stack_size = kDefaultStackSize;
    stack_size_ = stack_size;

    auto& pool = StackPool::GetInstance();
    stack_ = pool.allocate();
    context_init(&ctx_, stack_, pool.stackSize(),
                 reinterpret_cast<void(*)(void*)>(&Fiber::MainFunc), this);
    ++s_fiber_count;
}

Fiber::~Fiber() {
    --s_fiber_count;
    if (stack_) {
        ZERO_ASSERT(state_ == TERM || state_ == INIT || state_ == EXCEPT);
        StackPool::GetInstance().deallocate(stack_);
        stack_ = nullptr;
    } else {
        ZERO_ASSERT(!cb_);
        ZERO_ASSERT(state_ == RUNNING);
        if (t_this_fiber == this) t_this_fiber = nullptr;
    }
}

// ====================================================================
// 调度核心
// ====================================================================

void Fiber::resume() {
    SetThis(this);
    ZERO_ASSERT(state_ != RUNNING);
    state_ = RUNNING;

    Fiber* main = Scheduler::GetMainFiber();
    ZERO_ASSERT(main);
    context_swap(&main->ctx_, &ctx_);
}

void Fiber::reset(Callback cb) {
    ZERO_ASSERT(stack_);
    ZERO_ASSERT(state_ == TERM || state_ == INIT || state_ == EXCEPT);

    cb_ = std::move(cb);
    context_init(&ctx_, stack_, StackPool::GetInstance().stackSize(),
                 reinterpret_cast<void(*)(void*)>(&Fiber::MainFunc), this);
    state_ = INIT;
}

// ====================================================================
// 线程局部
// ====================================================================

Fiber::ptr Fiber::GetThis() {
    if (t_this_fiber) {
        return t_this_fiber->shared_from_this();
    }
    Fiber::ptr main_fiber(new Fiber());
    ZERO_ASSERT(t_this_fiber == main_fiber.get());
    return main_fiber;
}

void Fiber::SetThis(Fiber* fiber) { t_this_fiber = fiber; }

uint64_t Fiber::GetFiberId() {
    return t_this_fiber ? t_this_fiber->getId() : 0;
}

uint64_t Fiber::GetTotalCount() {
    return s_fiber_count.load();
}

// ====================================================================
// 挂起操作
// ====================================================================

void Fiber::YieldToHold() {
    Fiber::ptr cur = GetThis();
    ZERO_ASSERT(cur->state_ == RUNNING);
    cur->state_ = HOLD;

    Fiber* main = Scheduler::GetMainFiber();
    ZERO_ASSERT(main);
    context_swap(&cur->ctx_, &main->ctx_);
}

void Fiber::YieldToReady() {
    Fiber::ptr cur = GetThis();
    ZERO_ASSERT(cur->state_ == RUNNING);
    cur->state_ = READY;

    Fiber* main = Scheduler::GetMainFiber();
    ZERO_ASSERT(main);
    context_swap(&cur->ctx_, &main->ctx_);
}

// ====================================================================
// 协程入口
// ====================================================================

void Fiber::MainFunc(void* /*arg*/) {
    Fiber::ptr cur = GetThis();
    ZERO_ASSERT(cur);

    try {
        cur->cb_();
        cur->cb_ = nullptr;
        cur->state_ = TERM;
    } catch (std::exception& ex) {
        cur->state_ = EXCEPT;
        ZERO_LOG_ERROR(ZERO_LOG_ROOT()) << "Fiber [" << cur->name_ << ":" << cur->id_
            << "] exception: " << ex.what();
    } catch (...) {
        cur->state_ = EXCEPT;
        ZERO_LOG_ERROR(ZERO_LOG_ROOT()) << "Fiber [" << cur->name_ << ":" << cur->id_
            << "] unknown exception";
    }

    Fiber* main = Scheduler::GetMainFiber();
    ZERO_ASSERT(main);
    context_swap(&cur->ctx_, &main->ctx_);

    ZERO_ASSERT_MSG(false, "Fiber::MainFunc should never reach here");
}

} // namespace zero
