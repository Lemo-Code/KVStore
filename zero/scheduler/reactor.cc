#include "zero/scheduler/reactor.h"
#include "zero/base/macro.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <stdexcept>

namespace zero {

Reactor::Reactor() {
    // 创建 epoll
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ < 0) {
        throw std::runtime_error("epoll_create1 failed: " +
                                 std::string(strerror(errno)));
    }

    // 创建 eventfd 用于跨线程唤醒
    wakeup_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (wakeup_fd_ < 0) {
        close(epoll_fd_);
        throw std::runtime_error("eventfd failed: " +
                                 std::string(strerror(errno)));
    }

    // 将 eventfd 注册到 epoll (边缘触发)
    // 使用 data.u64 标记为 wakeup (避免与 FdContext* 冲突)
    epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLET;
    ev.data.u64 = ~0ull;  // sentinel: wakeup event

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, wakeup_fd_, &ev) < 0) {
        close(wakeup_fd_);
        close(epoll_fd_);
        throw std::runtime_error("epoll_ctl(wakeup) failed: " +
                                 std::string(strerror(errno)));
    }

    // 预分配 FdContext 数组
    fd_ctxs_.resize(64, nullptr);
}

Reactor::~Reactor() {
    // 清理 FdContext
    for (FdContext* ctx : fd_ctxs_) {
        delete ctx;
    }
    fd_ctxs_.clear();

    close(wakeup_fd_);
    close(epoll_fd_);
}

// ====================================================================
// FdContext 管理
// ====================================================================
Reactor::FdContext* Reactor::getFdContext(int fd) {
    if (fd < 0) return nullptr;
    if (static_cast<size_t>(fd) >= fd_ctxs_.size()) {
        if (!resizeFdContexts(fd)) return nullptr;
    }
    return fd_ctxs_[fd];
}

bool Reactor::resizeFdContexts(int fd) {
    size_t new_size = static_cast<size_t>(fd) * 3 / 2 + 1;
    fd_ctxs_.resize(new_size, nullptr);
    return true;
}

// ====================================================================
// IO 事件
// ====================================================================
int Reactor::addEvent(int fd, Event event, Fiber::ptr waiter, uint64_t timeout_ms) {
    FdContext* ctx = getFdContext(fd);
    if (!ctx) {
        ctx = new FdContext();
        ctx->fd = fd;
        fd_ctxs_[fd] = ctx;
    }

    uint32_t targeted = static_cast<uint32_t>(event) & (EPOLLIN | EPOLLOUT);
    if (ctx->registered_events & targeted) {
        // 事件已注册
        return -1;
    }

    // 设置 waiter
    if (event == READ) {
        ctx->read_waiter = std::move(waiter);
    } else if (event == WRITE) {
        ctx->write_waiter = std::move(waiter);
    }

    // 设置超时定时器
    if (timeout_ms != ~0ull) {
        auto timer_cb = [this, fd, event]() {
            cancelEvent(fd, event);
        };
        uint64_t tid = timer_wheel_.addTimer(timeout_ms, timer_cb);
        if (event == READ) {
            ctx->read_timeout_id = tid;
        } else {
            ctx->write_timeout_id = tid;
        }
    }

    // 修改 epoll 注册
    uint32_t new_events = ctx->registered_events | targeted;
    int op = ctx->registered_events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

    epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLET | new_events;
    ev.data.ptr = ctx;

    if (epoll_ctl(epoll_fd_, op, fd, &ev) < 0) {
        return -1;
    }

    ctx->registered_events = new_events;
    ++pending_count_;
    return 0;
}

bool Reactor::delEvent(int fd, Event event) {
    FdContext* ctx = getFdContext(fd);
    if (!ctx) return false;

    uint32_t targeted = static_cast<uint32_t>(event) & (EPOLLIN | EPOLLOUT);
    if (!(ctx->registered_events & targeted)) return false;

    uint32_t new_events = ctx->registered_events & ~targeted;
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;

    epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLET | new_events;
    ev.data.ptr = ctx;

    epoll_ctl(epoll_fd_, op, fd, &ev);

    ctx->registered_events = new_events;

    // 清理 waiter
    if (event == READ) {
        ctx->read_waiter.reset();
        if (ctx->read_timeout_id) {
            timer_wheel_.cancelTimer(ctx->read_timeout_id);
            ctx->read_timeout_id = 0;
        }
    } else {
        ctx->write_waiter.reset();
        if (ctx->write_timeout_id) {
            timer_wheel_.cancelTimer(ctx->write_timeout_id);
            ctx->write_timeout_id = 0;
        }
    }

    --pending_count_;
    return true;
}

bool Reactor::cancelEvent(int fd, Event event) {
    FdContext* ctx = getFdContext(fd);
    if (!ctx) return false;

    uint32_t targeted = static_cast<uint32_t>(event) & (EPOLLIN | EPOLLOUT);
    if (!(ctx->registered_events & targeted)) return false;

    // 先触发 waiter, 再删除事件
    Fiber::ptr waiter;
    if (event == READ) {
        waiter = std::move(ctx->read_waiter);
        if (ctx->read_timeout_id) {
            timer_wheel_.cancelTimer(ctx->read_timeout_id);
            ctx->read_timeout_id = 0;
        }
    } else {
        waiter = std::move(ctx->write_waiter);
        if (ctx->write_timeout_id) {
            timer_wheel_.cancelTimer(ctx->write_timeout_id);
            ctx->write_timeout_id = 0;
        }
    }

    // 从 epoll 中删除该事件
    uint32_t new_events = ctx->registered_events & ~targeted;
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;

    epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLET | new_events;
    ev.data.ptr = ctx;

    epoll_ctl(epoll_fd_, op, fd, &ev);
    ctx->registered_events = new_events;
    --pending_count_;

    // 唤醒 waiter (将 fiber 放回调度器)
    if (waiter && waiter->getState() == Fiber::HOLD) {
        // 由调用者处理 fiber 的重新调度
        // 此处仅做记录, 实际调度由 Scheduler 完成
        waiter->setState(Fiber::READY);
    }

    return true;
}

bool Reactor::cancelAll(int fd) {
    FdContext* ctx = getFdContext(fd);
    if (!ctx || ctx->registered_events == 0) return false;

    // 分别取消 READ 和 WRITE
    if (ctx->registered_events & EPOLLIN) cancelEvent(fd, READ);
    if (ctx->registered_events & EPOLLOUT) cancelEvent(fd, WRITE);

    return true;
}

// ====================================================================
// 定时器
// ====================================================================
uint64_t Reactor::addTimer(uint64_t delay_ms, TimerWheel::TimerCallback cb,
                            bool recurring) {
    return timer_wheel_.addTimer(delay_ms, std::move(cb), recurring);
}

bool Reactor::cancelTimer(uint64_t timer_id) {
    return timer_wheel_.cancelTimer(timer_id);
}

uint64_t Reactor::nextTimerMs() const {
    return timer_wheel_.nextExpireMs();
}

// ====================================================================
// 事件循环
// ====================================================================
int Reactor::poll(int timeout_ms, std::vector<Fiber::ptr>& ready_fibers) {
    // 1. 收集到期的定时器回调
    uint64_t now_ms = GetCurrentMS();
    std::vector<TimerWheel::TimerCallback> timer_cbs;
    timer_wheel_.tick(now_ms, timer_cbs);

    // 执行定时器回调 (可能触发 cancelEvent 修改 fiber 状态)
    for (auto& cb : timer_cbs) {
        cb();
    }

    // 2. 检查是否有定时器触发的 fiber 就绪
    //    cancelEvent 设置了 fiber 状态为 READY
    //    但我们需要收集它们 — 这由 Scheduler 在 poll 返回后统一扫描

    // 3. 如果没有待处理的 IO 事件且无定时器, 可以更快返回
    if (pending_count_ == 0 && timer_wheel_.empty()) {
        // 短暂等待 eventfd (可能被 wakeup)
        timeout_ms = std::min(timeout_ms, 1);
    }

    // 4. epoll_wait
    epoll_event events[kMaxEvents];
    int nfds = epoll_wait(epoll_fd_, events, kMaxEvents, timeout_ms);

    if (nfds < 0) {
        if (errno == EINTR) return 0;
        return -1;
    }

    // 5. 处理 epoll 事件
    for (int i = 0; i < nfds; ++i) {
        const epoll_event& ev = events[i];

        // Wakeup 事件 (标记为 ~0ull)
        if (ev.data.u64 == ~0ull) {
            // 消费 eventfd 中的数据
            uint64_t dummy;
            while (read(wakeup_fd_, &dummy, sizeof(dummy)) > 0) {}
            continue;
        }

        // IO 事件
        FdContext* ctx = static_cast<FdContext*>(ev.data.ptr);

        uint32_t triggered = ev.events;
        // 处理错误/挂断
        if (triggered & (EPOLLERR | EPOLLHUP)) {
            triggered |= (EPOLLIN | EPOLLOUT) & ctx->registered_events;
        }

        uint32_t active = NONE;
        if (triggered & EPOLLIN)  active |= READ;
        if (triggered & EPOLLOUT) active |= WRITE;

        if (active == NONE) continue;

        // 从 epoll 中移除就绪的事件 (EPOLLET 下, 需重新注册)
        uint32_t remaining = ctx->registered_events & ~active;
        int op = remaining ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;

        epoll_event mod_ev;
        memset(&mod_ev, 0, sizeof(mod_ev));
        mod_ev.events = EPOLLET | remaining;
        mod_ev.data.ptr = ctx;

        epoll_ctl(epoll_fd_, op, ctx->fd, &mod_ev);
        ctx->registered_events = remaining;

        // 唤醒等待的 fiber
        if (active & READ) {
            --pending_count_;
            if (ctx->read_waiter && ctx->read_waiter->getState() == Fiber::HOLD) {
                ready_fibers.push_back(std::move(ctx->read_waiter));
                if (ctx->read_timeout_id) {
                    timer_wheel_.cancelTimer(ctx->read_timeout_id);
                    ctx->read_timeout_id = 0;
                }
            }
        }
        if (active & WRITE) {
            --pending_count_;
            if (ctx->write_waiter && ctx->write_waiter->getState() == Fiber::HOLD) {
                ready_fibers.push_back(std::move(ctx->write_waiter));
                if (ctx->write_timeout_id) {
                    timer_wheel_.cancelTimer(ctx->write_timeout_id);
                    ctx->write_timeout_id = 0;
                }
            }
        }
    }

    return static_cast<int>(ready_fibers.size());
}

void Reactor::wakeup() {
    uint64_t val = 1;
    ssize_t n = write(wakeup_fd_, &val, sizeof(val));
    (void)n;
}

// ---- 线程级 Reactor ----
static thread_local Reactor* t_reactor = nullptr;

Reactor* GetCurrentReactor() { return t_reactor; }
void SetCurrentReactor(Reactor* r) { t_reactor = r; }

} // namespace zero
