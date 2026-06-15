#pragma once

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <random>

#include "zero/base/noncopyable.h"
#include "zero/fiber/fiber.h"
#include "zero/scheduler/work_stealing_queue.h"
#include "zero/thread/mutex.h"
#include "zero/thread/thread.h"

namespace zero {

// Per-thread context (供 schedule() 无锁快路径使用)
struct PerThread {
    WorkStealingQueue local_queue;
    int thread_index = -1;
};
extern thread_local PerThread* t_per_thread;

// ============ Scheduler (M:N 协程调度器) ============
//
// 调度循环 (每线程的 run()):
//   1. 从全局队列取任务
//   2. 有任务 → 执行 fiber->resume()
//   3. 无任务 → idle()
//   4. 检查是否可停止
//
// Phase 2: idle = 简单 yield / 短暂 sleep
// Phase 3: idle = epoll_wait (由 Reactor 重写)
//
class Scheduler : public Noncopyable {
public:
    using ptr = std::shared_ptr<Scheduler>;
    using MutexType = Mutex;

    Scheduler(size_t threads = 1, bool use_caller = false,
              const std::string& name = "scheduler");
    virtual ~Scheduler();

    const std::string& getName() const { return name_; }

    // ---- 生命周期 ----
    void start();
    void stop();

    // ---- 任务调度 ----

    // 调度 fiber
    void schedule(Fiber::ptr fiber, int thread = -1) {
        if (thread == -1 && t_per_thread) {
            if (t_per_thread->local_queue.push(std::move(fiber))) return;
        }
        // 本地队列满 或 指定线程 → 全局队列
        bool need = false;
        { MutexType::Lock lock(mutex_); need = scheduleNoLock(std::move(fiber), thread); }
        if (need) tickle();
    }

    // 调度 callback
    void schedule(std::function<void()> cb, int thread = -1) {
        if (thread == -1 && t_per_thread) {
            auto fiber = std::make_shared<Fiber>(std::move(cb));
            if (t_per_thread->local_queue.push(std::move(fiber))) return;
        }
        bool need = false;
        { MutexType::Lock lock(mutex_); need = scheduleNoLock(std::move(cb), thread); }
        if (need) tickle();
    }

    // 批量调度 (保持兼容)
    template<typename InputIterator>
    void schedule(InputIterator begin, InputIterator end) {
        bool need = false;
        { MutexType::Lock lock(mutex_);
          while (begin != end) { need = scheduleNoLock(std::move(*begin), -1) || need; ++begin; } }
        if (need) tickle();
    }

    // ---- 线程局部访问 ----
    static Scheduler* GetThis();
    static Fiber*     GetMainFiber();

    // ---- 查询 ----
    size_t threadCount()   const { return thread_count_; }
    size_t activeThreads() const { return active_threads_.load(); }
    size_t idleThreads()   const { return idle_threads_.load(); }
    bool   isStopping()    const { return stopping_; }

protected:
    // 子类可重写
    virtual void tickle();    // 唤醒休眠线程
    virtual void idle();      // 无任务时等待
    virtual bool stopping();  // 可停止条件

    // 调度循环
    void run();

    void setThis();

    // 任务类型
    struct Task {
        Fiber::ptr fiber;
        std::function<void()> cb;
        int target_thread = -1;

        Task() = default;
        Task(Fiber::ptr f, int thr) : fiber(std::move(f)), target_thread(thr) {}
        Task(std::function<void()> c, int thr) : cb(std::move(c)), target_thread(thr) {}

        void reset() {
            fiber.reset();
            cb = nullptr;
            target_thread = -1;
        }
    };

    MutexType mutex_;
    std::vector<Thread::ptr> threads_;       // 工作线程
    std::vector<int>         thread_ids_;    // 线程 ID

    std::list<Task> global_queue_;           // 全局任务队列

    std::string name_;
    size_t thread_count_ = 0;
    std::atomic<size_t> active_threads_{0};
    std::atomic<size_t> idle_threads_{0};

    bool stopping_ = true;
    bool auto_stop_ = false;
    int  caller_thread_id_ = -1;

    // Per-thread eventfd for cross-thread wakeup
    std::vector<int> wakeup_fds_;
    MutexType wakeup_mutex_;  // use_caller 时调用线程的 tid

    // 线程局部
    static thread_local Scheduler*  t_this_scheduler;
    static thread_local Fiber::ptr  t_main_fiber;  // 必须持有 shared_ptr 保证 Fiber 不被销毁

private:
    // 不加锁调度 (必须在 mutex_ 已锁定时调用)
    template<typename FiberOrCb>
    bool scheduleNoLock(FiberOrCb fc, int thread) {
        Task task(std::move(fc), thread);
        bool was_empty = global_queue_.empty();
        global_queue_.push_back(std::move(task));
        return was_empty;
    }
};

} // namespace zero
