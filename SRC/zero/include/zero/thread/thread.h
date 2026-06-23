// zero Thread — OS thread wrapper with naming and CPU affinity support
// Wraps pthread (or std::thread) with start/join/detach semantics,
// semaphore-based start synchronization, and thread naming.
#pragma once

#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <string>
#include <functional>
#include <memory>
#include <utility>

#include "zero/base/noncopyable.h"
#include "zero/thread/semaphore.h"

namespace zero {

class Thread : public Noncopyable {
public:
    using Callback = std::function<void()>;

    // Construct a thread with a callback and optional name.
    // The thread does NOT start until start() is called.
    explicit Thread(Callback cb, const std::string& name = "")
        : cb_(std::move(cb))
        , name_(name)
        , started_(false)
        , joined_(false) {}

    // Destroy the thread object. If the thread is running and not joined,
    // it will be detached automatically.
    ~Thread() {
        if (started_ && !joined_) {
            pthread_detach(thread_);
        }
    }

    // Start the thread. Returns true on success, false on failure.
    // Blocks until the thread has actually started (via internal semaphore).
    bool start() {
        if (started_) {
            return false;  // Already started
        }
        if (pthread_create(&thread_, nullptr, &Thread::run, this) != 0) {
            return false;
        }
        // Wait for the thread to signal that it is ready
        sem_.wait();
        started_ = true;
        return true;
    }

    // Join the thread (wait for it to finish). No-op if already joined
    // or not started.
    void join() {
        if (started_ && !joined_) {
            pthread_join(thread_, nullptr);
            joined_ = true;
        }
    }

    // Detach the thread (let it run independently). No-op if already
    // joined or not started.
    void detach() {
        if (started_ && !joined_) {
            pthread_detach(thread_);
            joined_ = true;
        }
    }

    // Whether the thread can be joined
    bool joinable() const noexcept {
        return started_ && !joined_;
    }

    // Get the native pthread_t handle
    pthread_t native_handle() const noexcept { return thread_; }

    // Get the thread's configured name (not necessarily the OS name)
    const std::string& name() const noexcept { return name_; }

    // Get the OS-level thread ID of this thread
    uint64_t get_id() const noexcept {
        return static_cast<uint64_t>(thread_);
    }

    // Get the number of hardware threads on this system
    static unsigned hardware_concurrency() noexcept {
        long n = sysconf(_SC_NPROCESSORS_ONLN);
        return (n > 0) ? static_cast<unsigned>(n) : 1;
    }

    // Set the name of the CALLING thread (OS-level)
    static void set_current_name(const std::string& name) {
#if defined(__APPLE__)
        pthread_setname_np(name.c_str());
#else
        pthread_setname_np(pthread_self(), name.c_str());
#endif
    }

    // Get the current thread's pthread_t
    static pthread_t current_thread() noexcept {
        return pthread_self();
    }

    // Yield the calling thread's time slice
    static void yield() noexcept {
        sched_yield();
    }

    // Sleep the calling thread for a duration in milliseconds
    static void sleep_ms(unsigned ms) noexcept {
        struct timespec ts;
        ts.tv_sec = ms / 1000;
        ts.tv_nsec = (ms % 1000) * 1000000L;
        ::nanosleep(&ts, nullptr);
    }

private:
    // Static entry point passed to pthread_create
    static void* run(void* arg) {
        Thread* self = static_cast<Thread*>(arg);

        // Set thread name if provided
        if (!self->name_.empty()) {
            set_current_name(self->name_);
        }

        // Signal the creator that we are started
        self->sem_.post();

        // Execute the user callback
        self->cb_();

        return nullptr;
    }

    pthread_t thread_;
    Callback cb_;
    std::string name_;
    Semaphore sem_;
    bool started_;
    bool joined_;
};

// A self-joining thread — automatically joins on destruction.
// Suitable for scoped thread management.
class ScopedThread : public Noncopyable {
public:
    template <typename F, typename... Args>
    explicit ScopedThread(F&& f, Args&&... args)
        : thread_(std::forward<F>(f), std::forward<Args>(args)...) {
        thread_.start();
    }

    ~ScopedThread() {
        thread_.join();
    }

    Thread& thread() noexcept { return thread_; }
    const Thread& thread() const noexcept { return thread_; }

private:
    Thread thread_;
};

} // namespace zero
