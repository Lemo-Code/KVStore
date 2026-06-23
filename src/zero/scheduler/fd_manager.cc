#include "zero/scheduler/fd_manager.h"
#include <sys/socket.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>

namespace zero {

FdCtx::FdCtx(int fd)
    : is_init_(false)
    , is_socket_(false)
    , sys_nonblock_(false)
    , user_nonblock_(false)
    , is_closed_(false)
    , fd_(fd)
    , recv_timeout_(~0ull)
    , send_timeout_(~0ull) {
    // 检测是否是 socket (直接 syscall, 绕过 hook 避免重入)
    int optval;
    socklen_t optlen = sizeof(optval);
    if (syscall(SYS_getsockopt, fd_, SOL_SOCKET, SO_TYPE, &optval, &optlen) == 0) {
        is_socket_ = true;
    }

    // 获取当前 nonblock 状态 (直接 syscall, 绕过 hook 避免重入)
    int flags = syscall(SYS_fcntl, fd_, F_GETFL, 0);
    if (flags >= 0 && (flags & O_NONBLOCK)) {
        sys_nonblock_ = true;
    }

    is_init_ = true;
}

FdCtx::~FdCtx() = default;

void FdCtx::setTimeout(int type, uint64_t v) {
    if (type == RECV_TIMEOUT) recv_timeout_ = v;
    else send_timeout_ = v;
}

uint64_t FdCtx::getTimeout(int type) {
    return type == RECV_TIMEOUT ? recv_timeout_ : send_timeout_;
}

// ---- FdManager (线程安全) ----
FdManager::FdManager() {
    datas_.resize(64);
}

FdCtx::ptr FdManager::get(int fd, bool auto_create) {
    if (fd < 0) return nullptr;

    // 读路径: 多数操作只需要读
    {
        RWMutex::ReadLock lock(mutex_);
        if (static_cast<size_t>(fd) < datas_.size()) {
            auto ctx = datas_[fd];
            if (ctx || !auto_create) return ctx;
        }
    }

    // 写路径: auto_create 且 fd 不存在
    if (!auto_create) return nullptr;

    RWMutex::WriteLock lock(mutex_);
    // 双重检查: 其他线程可能已经创建
    if (static_cast<size_t>(fd) >= datas_.size()) {
        datas_.resize(static_cast<size_t>(fd) * 3 / 2 + 1);
    }
    if (!datas_[fd]) {
        datas_[fd] = std::make_shared<FdCtx>(fd);
    }
    return datas_[fd];
}

void FdManager::del(int fd) {
    if (fd < 0) return;
    RWMutex::WriteLock lock(mutex_);
    if (static_cast<size_t>(fd) < datas_.size()) {
        if (datas_[fd]) {
            datas_[fd]->setClosed(true);
            datas_[fd].reset();
        }
    }
}

} // namespace zero
