// zero IoUringEngine — io_uring-based I/O backend (Linux 5.1+)
//
// io_uring is a modern Linux kernel interface for asynchronous I/O
// that can significantly outperform epoll for applications with high
// I/O concurrency. It avoids syscall overhead through shared memory
// submission and completion rings (SQ and CQ).
//
// This is an OPTIONAL backend. It compiles only when liburing is
// available and ZERO_HAS_IOURING is defined. The CMake build system
// uses find_package(LibUring) to detect availability.
//
// When both are available, EpollEngine is the default (wider kernel
// support). IoUringEngine can be selected via configuration.
//
// Reference: https://kernel.dk/io_uring.pdf
#pragma once

#include "zero/io/io_engine.h"

// liburing is an optional dependency
#ifdef ZERO_HAS_IOURING
#include <liburing.h>
#include <liburing/io_uring.h>
#endif

#include <vector>
#include <atomic>
#include <memory>

namespace zero {

class IoUringEngine : public IIOEngine {
public:
    // Maximum number of entries in the submission queue ring
    static constexpr unsigned kDefaultQueueDepth = 256;

    explicit IoUringEngine(unsigned queue_depth = kDefaultQueueDepth);
    ~IoUringEngine() override;

    IoUringEngine(const IoUringEngine&) = delete;
    IoUringEngine& operator=(const IoUringEngine&) = delete;

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

    int native_fd() const noexcept override;
    void wakeup() noexcept override;

    // ============================================================
    // io_uring-specific
    // ============================================================

    // Whether the engine was successfully initialized
    bool is_initialized() const noexcept { return initialized_; }

    // Get the submission queue entries remaining
    unsigned sq_available() const noexcept;

    // Total completions processed
    uint64_t get_total_completions() const noexcept {
#ifdef ZERO_HAS_IOURING
        return total_completions_.load(std::memory_order_relaxed);
#else
        return 0;
#endif
    }

private:
#ifdef ZERO_HAS_IOURING
    struct io_uring ring_;
    bool initialized_ = false;
    unsigned queue_depth_;

    // Completion queue entry buffer
    std::vector<struct io_uring_cqe*> cqes_;

    // Event tracking: for each fd, what events are registered
    struct FdState {
        uint32_t events = 0;
        void* user_data = nullptr;
    };
    std::vector<FdState> fd_states_;  // Indexed by fd (limited range)

    std::atomic<uint64_t> total_completions_{0};
#else
    bool initialized_ = false;
#endif
};

} // namespace zero
