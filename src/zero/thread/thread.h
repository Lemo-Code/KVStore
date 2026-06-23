#pragma once

#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <functional>
#include <memory>
#include <string>
#include <atomic>
#include <stdexcept>

#include "zero/base/noncopyable.h"
#include "zero/thread/semaphore.h"
#include "zero/log/log.h"

namespace zero {

// 线程包装器
// 特点:
//   1. 基于 pthread (非 std::thread) — 更精细控制
//   2. 支持命名的线程 (便于调试和日志)
//   3. 信号量保证构造完成性 (线程 start 后才返回)
class Thread : public Noncopyable {
public:
    using ptr = std::shared_ptr<Thread>;
    using Callback = std::function<void()>;

    Thread(Callback cb, const std::string& name = "")
        : cb_(std::move(cb))
        , name_(name) {
        // 使用默认名称如果为空
        if (name_.empty()) {
            name_ = "thread_" + std::to_string(s_thread_count.fetch_add(1));
        }
        // 创建线程
        int ret = pthread_create(&thread_, nullptr, &Thread::Run, this);
        if (ret) {
            throw std::runtime_error("pthread_create failed: " + std::to_string(ret));
        }
        // 等待线程函数开始执行 (确保 thread_ 已初始化)
        sem_.wait();
    }

    ~Thread() {
        if (joinable_) {
            // 注意: 不自动 join — 调用者应显式 join 或 detach
            pthread_detach(thread_);
        }
    }

    // 等待线程结束
    void join() {
        if (joinable_) {
            pthread_join(thread_, nullptr);
            joinable_ = false;
        }
    }

    // 分离线程 (后台运行)
    void detach() {
        if (joinable_) {
            pthread_detach(thread_);
            joinable_ = false;
        }
    }

    // 获取线程 ID (tid, 内核可见的 LWP ID)
    pid_t getId() const { return id_; }

    // 获取线程名称
    const std::string& getName() const { return name_; }

    // ---- 线程局部信息 ----
    // 获取当前线程的 Thread 对象指针 (仅在由 zero::Thread 创建的线程中有效)
    static Thread* GetThis();

    // 获取当前线程名称
    static const std::string& GetName();

    // 设置当前线程名称 (同时更新 pthread name)
    static void SetName(const std::string& name);

private:
    static void* Run(void* arg) {
        Thread* t = static_cast<Thread*>(arg);
        t->id_ = syscall(SYS_gettid);  // 获取内核线程 ID

        // 设置 pthread 名称 (在 /proc/PID/task/TID/comm 中可见, gdb 可读)
        if (!t->name_.empty()) {
            pthread_setname_np(pthread_self(), t->name_.substr(0, 15).c_str());
        }

        // 保存到 thread-local (GetThis() 可用)
        t_thread_ptr = t;
        t_thread_name = t->name_;

        // 通知构造函数: 线程已启动
        t->sem_.notify();

        // 执行用户回调
        try {
            t->cb_();
        } catch (std::exception& e) {
            ZERO_LOG_ERROR(ZERO_LOG_ROOT()) << "Thread [" << t->name_
                << "] unhandled exception: " << e.what();
        } catch (...) {
            ZERO_LOG_ERROR(ZERO_LOG_ROOT()) << "Thread [" << t->name_
                << "] unhandled unknown exception";
        }

        return nullptr;
    }

private:
    pthread_t thread_ = 0;
    pid_t id_ = -1;
    Callback cb_;
    std::string name_;
    Semaphore sem_;         // 启动同步
    bool joinable_ = true;

    static std::atomic<uint64_t> s_thread_count;

    static thread_local Thread* t_thread_ptr;
    static thread_local std::string t_thread_name;
};

// 静态成员定义
inline std::atomic<uint64_t> Thread::s_thread_count{0};
inline thread_local Thread* Thread::t_thread_ptr = nullptr;
inline thread_local std::string Thread::t_thread_name = "main";

inline Thread* Thread::GetThis() {
    return t_thread_ptr;
}

inline const std::string& Thread::GetName() {
    return t_thread_name;
}

inline void Thread::SetName(const std::string& name) {
    t_thread_name = name;
    if (!name.empty()) {
        pthread_setname_np(pthread_self(), name.substr(0, 15).c_str());
    }
}

} // namespace zero
