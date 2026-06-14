#ifndef __SYLAR_SIGNAL_H__
#define __SYLAR_SIGNAL_H__

#include <functional>
#include <map>
#include <memory>
#include "sylar/mutex.h"
#include "sylar/thread.h"

namespace sylar {

/**
 * @brief 统一信号处理
 * 注册 SIGINT/SIGTERM/SIGCHLD 等回调，实现优雅退出与子进程回收。
 */
class SignalManager {
public:
    typedef std::shared_ptr<SignalManager> ptr;
    typedef std::function<void(int)> Handler;

    SignalManager();
    ~SignalManager();

    /** 注册信号回调，返回是否成功 */
    bool signal(int signum, Handler cb);
    /** 忽略信号 */
    bool ignore(int signum);
    /** 恢复默认处理 */
    bool reset(int signum);

    /** 启动信号处理线程（读取 pipe 并派发回调），仅需调用一次 */
    void start();
    /** 停止并恢复默认信号 */
    void stop();

    /** 供静态 handler 写入 pipe，内部使用 */
    void notifySignal(int signum);

private:
    void threadLoop();

    sylar::Mutex m_mutex;
    std::map<int, Handler> m_handlers;
    int m_readFd;
    int m_writeFd;
    bool m_running;
    Thread::ptr m_thread;
};

} // namespace sylar

#endif
