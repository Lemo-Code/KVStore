#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "zero/base/noncopyable.h"
#include "zero/fiber/context.h"

namespace zero {

class Scheduler;  // 前向声明

// ============ Fiber (协程) ============
//
// 非对称、有栈协程 (stackful asymmetric coroutine)
//
// 状态机:
//   INIT → READY → RUNNING → (HOLD | READY | TERM | EXCEPT)
//                  ↑              │
//                  └──────────────┘  (yield 后重新调度)
//
// 线程安全: Fiber 自身的方法大多要求从所属线程调用
//           跨线程操作需通过 Scheduler::schedule()
class Fiber : public std::enable_shared_from_this<Fiber>,
              public Noncopyable {
    friend class Scheduler;

public:
    using ptr = std::shared_ptr<Fiber>;
    using Callback = std::function<void()>;

    // 协程状态
    enum State : uint8_t {
        INIT  = 0,  // 刚创建，还未运行
        READY = 1,  // 就绪，等待调度
        RUNNING = 2,// 正在执行
        HOLD  = 3,  // 挂起 (等待 IO / 锁 / 定时器)
        TERM  = 4,  // 正常结束
        EXCEPT = 5, // 异常结束
    };

    // 创建协程
    // @param cb        用户回调
    // @param stack_size 栈大小 (0 = 默认 128KB)
    // @param name      协程名称 (调试用)
    Fiber(Callback cb, size_t stack_size = 0, std::string name = "");
    ~Fiber();

    // ---- 状态查询 ----
    uint64_t getId()    const { return id_; }
    State    getState() const { return state_; }
    const std::string& getName() const { return name_; }

    // ---- 线程局部: 当前协程 ----
    static Fiber::ptr GetThis();
    static uint64_t    GetFiberId();

    // 当前协程总数 (全局计数, 调试用)
    static uint64_t GetTotalCount();

    // ---- 挂起操作 ----
    // yield → HOLD: 主动让出, 等待事件唤醒
    static void YieldToHold();
    // yield → READY: 让出但立即重新加入就绪队列
    static void YieldToReady();

    // ---- 重置 callback (复用 Fiber 对象) ----
    void reset(Callback cb);

public:
    // 仅调度相关代码可调用 (Scheduler, Reactor, Hook)
    void resume();
    void setState(State s) { state_ = s; }

private:
    // 静态: 设置当前线程的 Fiber 指针
    static void SetThis(Fiber* fiber);

    // 创建主协程 (线程的调度器上下文)
    Fiber();

    // 入口函数 (在 trampoline 之后执行)
    static void MainFunc(void* arg);

private:
    uint64_t  id_ = 0;
    State     state_ = INIT;
    Callback  cb_;              // 用户回调
    std::string name_;          // 协程名

    Context   ctx_;             // 汇编级上下文
    void*     stack_ = nullptr; // 栈基址
    size_t    stack_size_ = 0;  // 栈大小

    // 全局计数
    static std::atomic<uint64_t> s_fiber_id;
    static std::atomic<uint64_t> s_fiber_count;

    // 当前线程的 Fiber 指针 (thread_local)
    static thread_local Fiber* t_this_fiber;
};

// 获取当前协程 ID 的便捷函数
inline uint64_t GetFiberId() {
    return Fiber::GetFiberId();
}

} // namespace zero
