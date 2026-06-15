#pragma once

#include <atomic>
#include <memory>
#include <vector>
#include <cstdint>

#include "zero/base/noncopyable.h"
#include "zero/base/singleton.h"
#include "zero/thread/mutex.h"

namespace zero {

// ============ FdCtx (文件描述符上下文) ============
// 跟踪 fd 的属性: 是否 socket, 是否用户设置 nonblock, 超时等
class FdCtx : public std::enable_shared_from_this<FdCtx> {
public:
    using ptr = std::shared_ptr<FdCtx>;

    explicit FdCtx(int fd);
    ~FdCtx();

    bool isInit()         const { return is_init_; }
    bool isSocket()       const { return is_socket_; }
    bool isClosed()       const { return is_closed_.load(std::memory_order_acquire); }
    void setClosed(bool v)      { is_closed_.store(v, std::memory_order_release); }

    void setUserNonblock(bool v) { user_nonblock_ = v; }
    bool getUserNonblock() const { return user_nonblock_; }

    void setSysNonblock(bool v)  { sys_nonblock_ = v; }
    bool getSysNonblock()  const { return sys_nonblock_; }

    void     setTimeout(int type, uint64_t v);
    uint64_t getTimeout(int type);

    int  fd() const { return fd_; }

    // SO_RCVTIMEO / SO_SNDTIMEO
    static constexpr int RECV_TIMEOUT = 0;
    static constexpr int SEND_TIMEOUT = 1;

private:
    bool is_init_ : 1;
    bool is_socket_ : 1;
    bool sys_nonblock_ : 1;
    bool user_nonblock_ : 1;
    std::atomic<bool> is_closed_{false};  // atomic: 跨线程 close 可见
    int  fd_;
    uint64_t recv_timeout_ = ~0ull;
    uint64_t send_timeout_ = ~0ull;
};

// ============ FdManager (全局 fd 管理器, 线程安全) ============
class FdManager : public Noncopyable {
public:
    FdManager();

    // 获取 fd 上下文 (线程安全)
    FdCtx::ptr get(int fd, bool auto_create = false);
    void del(int fd);

private:
    RWMutex mutex_;                   // 读写锁: 读多写少
    std::vector<FdCtx::ptr> datas_;
};

using FdMgr = Singleton<FdManager>;

} // namespace zero
