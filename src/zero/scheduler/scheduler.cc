#include "zero/scheduler/scheduler.h"
#include "zero/scheduler/hook.h"
#include "zero/scheduler/reactor.h"
#include "zero/base/macro.h"
#include "zero/thread/thread.h"

#include <chrono>
#include <random>

namespace zero {

thread_local Scheduler*  Scheduler::t_this_scheduler = nullptr;
thread_local Fiber::ptr  Scheduler::t_main_fiber = nullptr;
thread_local PerThread*  t_per_thread = nullptr;

// Global: per-thread contexts for work-stealing
static std::vector<PerThread*> g_thread_ctxs;
static Mutex g_ctx_mutex;

// ====================================================================
Scheduler::Scheduler(size_t threads, bool use_caller, const std::string& name)
    : name_(name) {
    ZERO_ASSERT(threads > 0);

    if (use_caller) {
        caller_thread_id_ = GetThreadId();
        if (threads > 0) --threads;
        Fiber::GetThis();
        ZERO_ASSERT(GetThis() == nullptr);
        t_this_scheduler = this;
        Thread::SetName(name_);

        // 为 caller 线程创建 PerThread
        t_per_thread = new PerThread();
        MutexType::Lock lock(g_ctx_mutex);
        t_per_thread->thread_index = static_cast<int>(g_thread_ctxs.size());
        g_thread_ctxs.push_back(t_per_thread);
    }
    thread_count_ = threads;
}

Scheduler::~Scheduler() {
    if (!stopping_) stop();
    if (GetThis() == this) t_this_scheduler = nullptr;
}

// ====================================================================
void Scheduler::start() {
    MutexType::Lock lock(mutex_);
    if (!stopping_) return;

    stopping_ = false;
    ZERO_ASSERT(threads_.empty());

    threads_.resize(thread_count_);
    for (size_t i = 0; i < thread_count_; ++i) {
        threads_[i].reset(new Thread(
            std::bind(&Scheduler::run, this),
            name_ + "_" + std::to_string(i)));
        thread_ids_.push_back(threads_[i]->getId());
    }
}

void Scheduler::stop() {
    auto_stop_ = true;
    if (caller_thread_id_ != -1) ZERO_ASSERT(GetThis() == this);
    else ZERO_ASSERT(GetThis() != this);

    stopping_ = true;
    for (size_t i = 0; i < thread_count_; ++i) tickle();
    if (caller_thread_id_ != -1) run();

    std::vector<Thread::ptr> thrs;
    { MutexType::Lock lock(mutex_); thrs.swap(threads_); }
    for (auto& thr : thrs) thr->join();

    // Cleanup per-thread contexts
    if (t_per_thread) {
        MutexType::Lock lock(g_ctx_mutex);
        auto it = std::find(g_thread_ctxs.begin(), g_thread_ctxs.end(), t_per_thread);
        if (it != g_thread_ctxs.end()) g_thread_ctxs.erase(it);
        delete t_per_thread;
        t_per_thread = nullptr;
    }
}

// ====================================================================
// 调度核心 — 每个工作线程执行
// ====================================================================
void Scheduler::run() {
    setThis();
    SetHookEnabled(true);

    // 初始化 PerThread (非 caller 线程)
    if (!t_per_thread) {
        t_per_thread = new PerThread();
        MutexType::Lock lock(g_ctx_mutex);
        t_per_thread->thread_index = static_cast<int>(g_thread_ctxs.size());
        g_thread_ctxs.push_back(t_per_thread);
    }

    // 初始化 Reactor
    Reactor reactor;
    SetCurrentReactor(&reactor);
    {
        MutexType::Lock lock(wakeup_mutex_);
        wakeup_fds_.push_back(reactor.getWakeupFd());
    }

    t_main_fiber = Fiber::GetThis();
    Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this), 0, "idle"));
    Fiber::ptr cb_fiber;

    Task task;
    std::mt19937 rng(GetThreadId());

    while (true) {
        task.reset();

        // ==== Tier 1: 本地 work-stealing 队列 (LIFO — cache hot) ====
        Fiber::ptr fiber = t_per_thread->local_queue.pop();
        if (fiber && fiber->getState() != Fiber::TERM
                 && fiber->getState() != Fiber::EXCEPT) {
            ++active_threads_;
            fiber->resume();
            --active_threads_;

            Fiber::State st = fiber->getState();
            if (st == Fiber::READY) {
                schedule(fiber);
            } else if (st != Fiber::TERM && st != Fiber::EXCEPT && st != Fiber::HOLD) {
                fiber->setState(Fiber::HOLD);
            }
            continue;
        } else if (fiber) {
            // TERM/EXCEPT — skip
            continue;
        }

        // ==== Tier 2: 全局队列 (跨线程调度) ====
        bool has_global = false;
        {
            MutexType::Lock lock(mutex_);
            auto it = global_queue_.begin();
            while (it != global_queue_.end()) {
                if (it->target_thread != -1 && it->target_thread != GetThreadId()) {
                    ++it; continue;
                }
                task = *it;
                global_queue_.erase(it);
                has_global = true;
                break;
            }
        }

        if (has_global) {
            ++active_threads_;
            if (task.fiber && task.fiber->getState() != Fiber::TERM
                           && task.fiber->getState() != Fiber::EXCEPT) {
                task.fiber->resume();
                --active_threads_;
                Fiber::State st = task.fiber->getState();
                if (st == Fiber::READY) schedule(task.fiber);
                else if (st != Fiber::TERM && st != Fiber::EXCEPT && st != Fiber::HOLD)
                    task.fiber->setState(Fiber::HOLD);
            } else if (task.cb) {
                if (cb_fiber && (cb_fiber->getState() == Fiber::TERM
                              || cb_fiber->getState() == Fiber::EXCEPT)) {
                    cb_fiber->reset(task.cb);
                } else {
                    cb_fiber.reset(new Fiber(task.cb));
                }
                task.cb = nullptr;
                cb_fiber->resume();
                --active_threads_;
                Fiber::State st = cb_fiber->getState();
                if (st == Fiber::READY) { schedule(cb_fiber); cb_fiber.reset(); }
                else if (st == Fiber::EXCEPT || st == Fiber::TERM) { cb_fiber->reset(nullptr); }
                else { cb_fiber.reset(); }
            }
            task.reset();
            continue;
        }

        // ==== Tier 3: Work-stealing (从其他线程偷) ====
        {
            MutexType::Lock lock(g_ctx_mutex);
            size_t n = g_thread_ctxs.size();
            if (n > 1) {
                int start = rng() % n;
                for (size_t i = 0; i < n; ++i) {
                    PerThread* target = g_thread_ctxs[(start + i) % n];
                    if (target == t_per_thread) continue;
                    fiber = target->local_queue.steal();
                    if (fiber && fiber->getState() != Fiber::TERM
                             && fiber->getState() != Fiber::EXCEPT) {
                        ++active_threads_;
                        fiber->resume();
                        --active_threads_;
                        Fiber::State st = fiber->getState();
                        if (st == Fiber::READY) schedule(fiber);
                        else if (st != Fiber::TERM && st != Fiber::EXCEPT && st != Fiber::HOLD)
                            fiber->setState(Fiber::HOLD);
                        break;
                    }
                }
                if (fiber) continue;  // stolen task executed
            }
        }

        // ==== Tier 4: 检查停止 ====
        if (stopping()) break;

        // ==== Tier 5: Idle (reactor poll + work-stealing poll) ====
        if (idle_fiber->getState() == Fiber::TERM ||
            idle_fiber->getState() == Fiber::EXCEPT) {
            idle_fiber.reset(new Fiber(std::bind(&Scheduler::idle, this), 0, "idle"));
        }
        ++idle_threads_;
        idle_fiber->resume();
        --idle_threads_;
    }
}

// ====================================================================
void Scheduler::tickle() {
    MutexType::Lock lock(wakeup_mutex_);
    for (int fd : wakeup_fds_) {
        uint64_t val = 1;
        write(fd, &val, sizeof(val));
    }
}

void Scheduler::idle() {
    Reactor* reactor = GetCurrentReactor();
    if (!reactor) {
        while (!stopping()) { Fiber::YieldToHold(); }
        return;
    }

    while (!stopping()) {
        uint64_t next = reactor->nextTimerMs();
        int timeout_ms = 1;   // 1ms poll — 接近零延迟
        if (next != ~0ull) {
            uint64_t now = GetCurrentMS();
            timeout_ms = (next > now) ? static_cast<int>(next - now) : 0;
            if (timeout_ms > 10) timeout_ms = 10;
        }

        std::vector<Fiber::ptr> ready;
        reactor->poll(timeout_ms, ready);
        for (auto& f : ready) {
            if (f && f->getState() != Fiber::TERM && f->getState() != Fiber::EXCEPT) {
                schedule(std::move(f));
            }
        }
        Fiber::YieldToHold();
    }
}

bool Scheduler::stopping() {
    MutexType::Lock lock(mutex_);
    return auto_stop_ && stopping_ && global_queue_.empty() && active_threads_ == 0;
}

// ====================================================================
void Scheduler::setThis() { t_this_scheduler = this; }
Scheduler* Scheduler::GetThis() { return t_this_scheduler; }
Fiber* Scheduler::GetMainFiber() { return t_main_fiber.get(); }

} // namespace zero
