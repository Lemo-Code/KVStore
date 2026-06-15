#include "zero/scheduler/hook.h"
#include "zero/scheduler/reactor.h"
#include "zero/scheduler/fd_manager.h"
#include "zero/fiber/fiber.h"

#include <dlfcn.h>
#include <cstdarg>
#include <cerrno>
#include <cstring>
#include <atomic>

namespace zero {

// 线程级 hook 开关
static thread_local bool t_hook_enabled = false;

bool IsHookEnabled() { return t_hook_enabled; }
void SetHookEnabled(bool enabled) { t_hook_enabled = enabled; }

// 当前线程的 Reactor (由 Scheduler 设置) — 定义在 reactor.cc 中

// ============================================================
// Hook 初始化 (dlsym)
// ============================================================
#define HOOK_FUN(XX) \
    XX(sleep) XX(usleep) XX(nanosleep) \
    XX(socket) XX(connect) XX(accept) \
    XX(read) XX(readv) XX(recv) XX(recvfrom) XX(recvmsg) \
    XX(write) XX(writev) XX(send) XX(sendto) XX(sendmsg) \
    XX(close) XX(fcntl) XX(ioctl) \
    XX(getsockopt) XX(setsockopt)

static void hook_init() {
    static bool initialized = false;
    if (initialized) return;

    // RTLD_NEXT 可能对主程序中的符号无效, 先试 RTLD_NEXT, 失败则试 RTLD_DEFAULT
#define XX(name) \
    name##_f = (name##_func_t)dlsym(RTLD_NEXT, #name); \
    if (!name##_f) { \
        name##_f = (name##_func_t)dlsym(RTLD_DEFAULT, #name); \
    } \
    if (!name##_f) { \
        fprintf(stderr, "hook_init: failed to find %s\n", #name); \
    }
    HOOK_FUN(XX)
#undef XX
    initialized = true;
}

// 静态初始化器: 在 main 之前完成 hook
static struct HookInitializer {
    HookInitializer() { hook_init(); }
} s_hook_initializer;

// ============================================================
// do_io — 通用异步 IO 原语
// ============================================================
struct timer_info {
    int cancelled = 0;
};

template<typename OriginFunc, typename... Args>
static ssize_t do_io(int fd, OriginFunc real_func,
                     const char* func_name,
                     uint32_t event, int timeout_type,
                     Args&&... args) {
    // 1. 检查 hook 状态
    if (!t_hook_enabled) {
        return real_func(fd, std::forward<Args>(args)...);
    }

    // 2. 获取 fd 上下文
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(fd);
    if (!ctx) {
        return real_func(fd, std::forward<Args>(args)...);
    }

    if (ctx->isClosed()) {
        errno = EBADF;
        return -1;
    }

    // 3. 如果不是 socket 或用户设置了 nonblock → 直接调用原始函数
    if (!ctx->isSocket() || ctx->getUserNonblock()) {
        return real_func(fd, std::forward<Args>(args)...);
    }

    // 4. 获取超时时间
    uint64_t timeout = ctx->getTimeout(timeout_type);

    // 5. 异步 IO 循环
    auto tinfo = std::make_shared<timer_info>();

    int loop_count = 0;
    while (true) {
        ssize_t n = real_func(fd, std::forward<Args>(args)...);

        // 处理 EINTR
        while (n == -1 && errno == EINTR) {
            n = real_func(fd, std::forward<Args>(args)...);
        }

        // 成功或非 EAGAIN 错误 → 返回
        if (n != -1 || errno != EAGAIN) {
            return n;
        }

        // EAGAIN: 需要等待
        Reactor* reactor = GetCurrentReactor();
        if (!reactor) return -1;

        // 注册 IO 事件 + 超时定时器
        uint64_t timer_id = 0;
        if (timeout != ~0ull) {
            auto weak_tinfo = std::weak_ptr<timer_info>(tinfo);
            timer_id = reactor->addTimer(timeout, [weak_tinfo, reactor, fd, event]() {
                auto ti = weak_tinfo.lock();
                if (!ti || ti->cancelled) return;
                ti->cancelled = ETIMEDOUT;
                reactor->cancelEvent(fd, static_cast<Reactor::Event>(event));
            });
        }

        Fiber::ptr current = Fiber::GetThis();

        // timeout=~0ull 表示无超时 (永不超时)
        if (reactor->addEvent(fd, static_cast<Reactor::Event>(event), current, ~0ull) < 0) {
            if (timer_id) reactor->cancelTimer(timer_id);
            return -1;
        }

        // yield: 等待 IO 就绪
        Fiber::YieldToHold();

        // 被唤醒 — 清理定时器
        if (timer_id) {
            reactor->cancelTimer(timer_id);
        }

        // 检查是否超时
        if (tinfo->cancelled) {
            errno = tinfo->cancelled;
            return -1;
        }

        // 重试: 回到 while 循环顶部
    }
}

} // namespace zero

// ============================================================
// Hooked 函数实现
// ============================================================
extern "C" {

// ---- 函数指针定义 ----
#define XX(name) name##_func_t name##_f = nullptr;
HOOK_FUN(XX)
#undef XX

// ---- sleep 族 ----
unsigned int sleep(unsigned int seconds) {
    if (!zero::t_hook_enabled) return sleep_f(seconds);

    zero::Reactor* reactor = zero::GetCurrentReactor();
    if (reactor) {
        reactor->addTimer(seconds * 1000, []() {}, false);
    }
    zero::Fiber::YieldToHold();
    return 0;
}

int usleep(useconds_t usec) {
    if (!zero::t_hook_enabled) return usleep_f(usec);

    zero::Reactor* reactor = zero::GetCurrentReactor();
    if (reactor) {
        reactor->addTimer(usec / 1000 + 1, []() {}, false);
    }
    zero::Fiber::YieldToHold();
    return 0;
}

int nanosleep(const struct timespec* req, struct timespec* rem) {
    if (!zero::t_hook_enabled) return nanosleep_f(req, rem);

    uint64_t ms = req->tv_sec * 1000 + req->tv_nsec / 1000000;
    zero::Reactor* reactor = zero::GetCurrentReactor();
    if (reactor) {
        reactor->addTimer(ms + 1, []() {}, false);
    }
    zero::Fiber::YieldToHold();
    if (rem) { rem->tv_sec = 0; rem->tv_nsec = 0; }
    return 0;
}

// ---- socket ----
int socket(int domain, int type, int protocol) {
    if (!zero::t_hook_enabled) return socket_f(domain, type, protocol);

    int fd = socket_f(domain, type, protocol);
    if (fd >= 0) {
        zero::FdMgr::GetInstance()->get(fd, true);
    }
    return fd;
}

// ---- connect (带超时) ----
int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    return zero::do_io(sockfd, connect_f, "connect",
                       EPOLLOUT, zero::FdCtx::SEND_TIMEOUT,
                       addr, addrlen);
}

// ---- accept ----
int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    int fd = zero::do_io(sockfd, accept_f, "accept",
                         EPOLLIN, zero::FdCtx::RECV_TIMEOUT,
                         addr, addrlen);
    if (fd >= 0) {
        zero::FdMgr::GetInstance()->get(fd, true);
    }
    return fd;
}

// ---- read 族 ----
ssize_t read(int fd, void* buf, size_t count) {
    return zero::do_io(fd, read_f, "read", EPOLLIN, zero::FdCtx::RECV_TIMEOUT, buf, count);
}

ssize_t readv(int fd, const struct iovec* iov, int iovcnt) {
    return zero::do_io(fd, readv_f, "readv", EPOLLIN, zero::FdCtx::RECV_TIMEOUT, iov, iovcnt);
}

ssize_t recv(int sockfd, void* buf, size_t len, int flags) {
    return zero::do_io(sockfd, recv_f, "recv", EPOLLIN, zero::FdCtx::RECV_TIMEOUT, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags,
                 struct sockaddr* src_addr, socklen_t* addrlen) {
    return zero::do_io(sockfd, recvfrom_f, "recvfrom", EPOLLIN, zero::FdCtx::RECV_TIMEOUT,
                       buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags) {
    return zero::do_io(sockfd, recvmsg_f, "recvmsg", EPOLLIN, zero::FdCtx::RECV_TIMEOUT, msg, flags);
}

// ---- write 族 ----
ssize_t write(int fd, const void* buf, size_t count) {
    return zero::do_io(fd, write_f, "write", EPOLLOUT, zero::FdCtx::SEND_TIMEOUT, buf, count);
}

ssize_t writev(int fd, const struct iovec* iov, int iovcnt) {
    return zero::do_io(fd, writev_f, "writev", EPOLLOUT, zero::FdCtx::SEND_TIMEOUT, iov, iovcnt);
}

ssize_t send(int sockfd, const void* buf, size_t len, int flags) {
    return zero::do_io(sockfd, send_f, "send", EPOLLOUT, zero::FdCtx::SEND_TIMEOUT, buf, len, flags);
}

ssize_t sendto(int sockfd, const void* buf, size_t len, int flags,
               const struct sockaddr* dest_addr, socklen_t addrlen) {
    return zero::do_io(sockfd, sendto_f, "sendto", EPOLLOUT, zero::FdCtx::SEND_TIMEOUT,
                       buf, len, flags, dest_addr, addrlen);
}

ssize_t sendmsg(int sockfd, const struct msghdr* msg, int flags) {
    return zero::do_io(sockfd, sendmsg_f, "sendmsg", EPOLLOUT, zero::FdCtx::SEND_TIMEOUT, msg, flags);
}

// ---- close ----
int close(int fd) {
    if (!zero::t_hook_enabled) return close_f(fd);

    zero::Reactor* reactor = zero::GetCurrentReactor();
    if (reactor) {
        reactor->cancelAll(fd);
    }
    zero::FdMgr::GetInstance()->del(fd);
    return close_f(fd);
}

// ---- fcntl (nonblock 追踪) ----
int fcntl(int fd, int cmd, ...) {
    va_list va;
    va_start(va, cmd);

    switch (cmd) {
        case F_SETFL: {
            int arg = va_arg(va, int);
            va_end(va);

            if (!zero::t_hook_enabled) return fcntl_f(fd, cmd, arg);

            zero::FdCtx::ptr ctx = zero::FdMgr::GetInstance()->get(fd);
            if (!ctx || ctx->isClosed() || !ctx->isSocket()) {
                return fcntl_f(fd, cmd, arg);
            }

            ctx->setUserNonblock(arg & O_NONBLOCK);
            if (ctx->getSysNonblock()) {
                arg |= O_NONBLOCK;
            } else {
                arg &= ~O_NONBLOCK;
            }
            return fcntl_f(fd, cmd, arg);
        }
        case F_GETFL: {
            va_end(va);
            int arg = fcntl_f(fd, cmd);

            if (!zero::t_hook_enabled) return arg;

            zero::FdCtx::ptr ctx = zero::FdMgr::GetInstance()->get(fd);
            if (!ctx || ctx->isClosed() || !ctx->isSocket()) return arg;

            if (ctx->getUserNonblock()) {
                return arg | O_NONBLOCK;
            }
            return arg & ~O_NONBLOCK;
        }
        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
        case F_SETFD:
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
        case F_SETPIPE_SZ: {
            int arg = va_arg(va, int);
            va_end(va);
            return fcntl_f(fd, cmd, arg);
        }
        case F_GETFD:
        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
        case F_GETPIPE_SZ: {
            va_end(va);
            return fcntl_f(fd, cmd);
        }
        case F_SETLK:
        case F_SETLKW:
        case F_GETLK: {
            struct flock* arg = va_arg(va, struct flock*);
            va_end(va);
            return fcntl_f(fd, cmd, arg);
        }
        case F_GETOWN_EX:
        case F_SETOWN_EX: {
            struct f_owner_exlock* arg = va_arg(va, struct f_owner_exlock*);
            va_end(va);
            return fcntl_f(fd, cmd, arg);
        }
        default: {
            va_end(va);
            return fcntl_f(fd, cmd);
        }
    }
}

// ---- ioctl (nonblock 追踪) ----
int ioctl(int fd, unsigned long request, ...) {
    va_list va;
    va_start(va, request);
    void* arg = va_arg(va, void*);
    va_end(va);

    if (request == FIONBIO) {
        if (!zero::t_hook_enabled) return ioctl_f(fd, request, arg);

        bool nonblock = !!(*static_cast<int*>(arg));
        zero::FdCtx::ptr ctx = zero::FdMgr::GetInstance()->get(fd);
        if (!ctx || ctx->isClosed() || !ctx->isSocket()) {
            return ioctl_f(fd, request, arg);
        }
        ctx->setUserNonblock(nonblock);
    }
    return ioctl_f(fd, request, arg);
}

// ---- sockopt (passthrough) ----
int getsockopt(int sockfd, int level, int optname, void* optval, socklen_t* optlen) {
    return getsockopt_f(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen) {
    return setsockopt_f(sockfd, level, optname, optval, optlen);
}

} // extern "C"
