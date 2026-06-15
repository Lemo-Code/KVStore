#pragma once

#include <sys/epoll.h>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include "zero/fiber/fiber.h"
#include "zero/scheduler/timer_wheel.h"
#include "zero/base/noncopyable.h"

namespace zero {

// ============ Reactor (Per-thread epoll 事件循环) ============
//
// 每线程一个 Reactor 实例, 负责:
//   1. epoll 事件监听 (READ/WRITE)
//   2. 定时器管理 (TimerWheel)
//   3. 跨线程唤醒 (eventfd)
//
// poll() 阻塞等待 IO 事件或定时器到期, 返回需要恢复的 fiber 列表。
//
// 线程安全: 所有操作必须在所属线程调用 (单线程模型)
class Reactor : public Noncopyable {
public:
    enum Event : uint32_t {
        NONE  = 0,
        READ  = EPOLLIN,
        WRITE = EPOLLOUT,
        ERROR = EPOLLERR | EPOLLHUP,
    };

    Reactor();
    ~Reactor();

    // ---- IO 事件管理 ----

    // 注册事件: 当 fd 就绪时, 唤醒 waiter fiber
    // 返回 0 成功, -1 失败
    int addEvent(int fd, Event event, Fiber::ptr waiter, uint64_t timeout_ms = ~0ull);

    // 删除事件 (不触发回调)
    bool delEvent(int fd, Event event);

    // 取消事件 (触发一次回调后删除)
    bool cancelEvent(int fd, Event event);

    // 取消 fd 上的所有事件
    bool cancelAll(int fd);

    // ---- 定时器 ----
    uint64_t addTimer(uint64_t delay_ms, TimerWheel::TimerCallback cb,
                      bool recurring = false);
    bool cancelTimer(uint64_t timer_id);

    // ---- 事件循环 ----

    // 阻塞等待事件, 返回就绪的 fiber 列表
    // timeout_ms: 最大等待时间 (0=立即返回, -1=无限等待)
    // 返回: 需要恢复的 fiber 数量
    int poll(int timeout_ms, std::vector<Fiber::ptr>& ready_fibers);

    // 获得下一个定时器的等待时间 (用于 epoll_wait 超时)
    uint64_t nextTimerMs() const;

    // 是否有待处理的 IO 事件
    bool hasPendingEvents() const { return pending_count_ > 0; }

    // ---- 跨线程唤醒 ----
    void wakeup();  // 从其他线程调用: 唤醒 epoll_wait

private:
    struct FdContext {
        int fd = -1;
        uint32_t registered_events = 0;  // 当前 epoll 中注册的事件

        Fiber::ptr read_waiter;          // 等待 READ 的 fiber
        Fiber::ptr write_waiter;         // 等待 WRITE 的 fiber
        uint64_t read_timeout_id = 0;    // 读超时定时器 ID
        uint64_t write_timeout_id = 0;   // 写超时定时器 ID
    };

    FdContext* getFdContext(int fd);
    bool resizeFdContexts(int fd);

    int epoll_fd_;
    int wakeup_fd_;           // eventfd

public:
    int getWakeupFd() const { return wakeup_fd_; }

    std::vector<FdContext*> fd_ctxs_;   // 按 fd 索引
    TimerWheel timer_wheel_;

    std::atomic<size_t> pending_count_{0};

    static constexpr size_t kMaxEvents = 256;
};

// 线程级 Reactor 访问 (由 Scheduler 设置)
Reactor* GetCurrentReactor();
void     SetCurrentReactor(Reactor* r);

} // namespace zero
