// zero EpollEngine — Linux epoll-based I/O backend
//
// The primary I/O backend for zero on Linux. Uses epoll_create1(EPOLL_CLOEXEC)
// to create an epoll instance, and epoll_ctl / epoll_wait for fd management
// and event polling.
//
// epoll is level-triggered by default. Use EPOLLET flag for edge-triggered
// mode (recommended for high-throughput TCP servers).
//
// Thread-safe for concurrent add/mod/del from different threads (epoll_ctl
// is internally synchronized by the kernel). However, wait() should be
// called from a single thread (the reactor thread).
#pragma once

#include "zero/io/io_engine.h"
#include <vector>
#include <atomic>

namespace zero {

class EpollEngine : public IIOEngine {
public:
    EpollEngine();
    ~EpollEngine() override;

    EpollEngine(const EpollEngine&) = delete;
    EpollEngine& operator=(const EpollEngine&) = delete;

    // ============================================================
    // IIOEngine interface
    // ============================================================

    bool add_fd(int fd, uint32_t events,
                 void* user_data = nullptr) override;
    bool mod_fd(int fd, uint32_t events,
                 void* user_data = nullptr) override;
    bool del_fd(int fd) override;
    int  wait(IoEvent* events, int max_events,
               int timeout_ms) override;

    int native_fd() const noexcept override { return epoll_fd_; }
    void wakeup() noexcept override;

    // ============================================================
    // Epoll-specific
    // ============================================================

    // Get the underlying epoll file descriptor (for epoll-in-epoll etc.)
    int epoll_fd() const noexcept { return epoll_fd_; }

    // Maximum number of events returned per wait() call
    static constexpr int kMaxEvents = 256;

    // Total events processed (for diagnostics)
    uint64_t total_events() const noexcept {
        return total_events_.load(std::memory_order_relaxed);
    }

    // Whether the engine is active
    bool is_running() const noexcept { return running_; }
    void stop() noexcept { running_ = false; }

private:
    // Convert a raw epoll_event to a zero IoEvent
    static IoEvent convert_event(const epoll_event& ev) noexcept;

    int epoll_fd_ = -1;
    int wakeup_fd_ = -1;  // eventfd for cross-thread wakeup

    // Pre-allocated buffer for epoll_wait (avoids per-call allocation)
    std::vector<epoll_event> raw_events_;

    std::atomic<uint64_t> total_events_{0};
    bool running_ = true;
};

} // namespace zero
