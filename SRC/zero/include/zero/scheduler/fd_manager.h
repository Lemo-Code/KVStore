// zero FdManager — tracks all file descriptors and their states
//
// Each open file descriptor in a fiber-aware application has an FdContext
// that records:
//   - Whether the fd is a socket (affects I/O behavior)
//   - Non-blocking status (set by user via fcntl or by hook)
//   - Timeout values for recv/send
//   - The fiber currently waiting on this fd (for read/write events)
//   - Registered event callbacks
//
// FdManager is a process-wide singleton. When a fiber blocks on I/O,
// the hook system registers the fiber on the fd's context. When the
// reactor detects the fd is ready, it looks up the fd context and
// resumes the waiting fiber.
//
// The fd_context also holds a reference to the reactor that is
// monitoring this fd (set when the fd is registered with epoll).
#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <functional>
#include <memory>

#include "zero/base/noncopyable.h"
#include "zero/base/macro.h"

namespace zero {

class Fiber;
class Reactor;

class FdManager : public Noncopyable {
public:
    // ============================================================
    // FD Context — per-fd metadata
    // ============================================================

    struct FdContext {
        int fd = -1;
        bool is_socket = false;
        bool sys_nonblock = false;   // User set O_NONBLOCK via fcntl
        bool user_nonblock = false;  // System forced O_NONBLOCK via hook

        // Timeouts in milliseconds; -1 = infinite
        int64_t recv_timeout_ms = -1;
        int64_t send_timeout_ms = -1;

        // Fibers waiting on this fd
        Fiber* read_fiber = nullptr;   // Fiber waiting for EPOLLIN
        Fiber* write_fiber = nullptr;  // Fiber waiting for EPOLLOUT

        // Event callbacks (alternative to fibers)
        using EventCallback = std::function<void(int fd, uint32_t events)>;
        EventCallback read_callback;
        EventCallback write_callback;
        EventCallback error_callback;

        // The reactor that is monitoring this fd
        Reactor* reactor = nullptr;

        // Registered epoll events (last known set)
        uint32_t registered_events = 0;

        // Whether this fd has been closed
        bool closed = false;

        // Reset the context for reuse
        void reset() noexcept;
    };

    // ============================================================
    // Singleton
    // ============================================================

    static FdManager& instance();

    // ============================================================
    // Operations
    // ============================================================

    // Get or create the FdContext for an fd.
    // If auto_create is true and no context exists, creates one.
    // Returns nullptr if fd is out of valid range.
    FdContext* get(int fd, bool auto_create = true);

    // Remove and free the context for an fd (called on close).
    void remove(int fd);

    // Set the reactor responsible for monitoring an fd.
    void set_reactor(int fd, Reactor* reactor);

    // Register a fiber as waiting for read events on this fd.
    // When the reactor gets EPOLLIN, it calls trigger_read.
    void wait_for_read(int fd, Fiber* fiber);

    // Register a fiber as waiting for write events on this fd.
    void wait_for_write(int fd, Fiber* fiber);

    // Trigger read event on fd (called by reactor).
    // Returns the fiber that was waiting, or nullptr.
    Fiber* trigger_read(int fd);

    // Trigger write event on fd (called by reactor).
    // Returns the fiber that was waiting, or nullptr.
    Fiber* trigger_write(int fd);

    // Trigger error event on fd (called by reactor).
    void trigger_error(int fd);

    // Set socket timeout values
    void set_recv_timeout(int fd, int64_t ms);
    void set_send_timeout(int fd, int64_t ms);
    int64_t recv_timeout(int fd) const;
    int64_t send_timeout(int fd) const;

    // ============================================================
    // Statistics
    // ============================================================

    size_t active_fd_count() const noexcept;

private:
    FdManager();
    ~FdManager();

    void ensure_context(int fd);

    // Maximum fd number we track
    static constexpr int kMaxFd = 65536;

    // Array of pointers for O(1) fd lookup
    // contexts are allocated on first use and freed on close
    FdContext* contexts_[kMaxFd] = {};

    // Simple spinlock for fd context operations
    mutable std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
    void lock() const noexcept;
    void unlock() const noexcept;
};

} // namespace zero
