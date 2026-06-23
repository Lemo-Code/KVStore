// zero — High-performance C++ network library umbrella header
//
// zero is a production-quality, fiber-based network framework for building
// high-throughput, low-latency servers. It provides:
//
//   - Stackful fibers (asymmetric coroutines) with M:N scheduling
//   - Transparent async I/O via syscall hooks (existing sync code runs
//     without modification)
//   - Work-stealing scheduler for automatic load balancing across CPU cores
//   - Epoll and io_uring I/O backends
//   - Full TCP/UDP networking stack with buffers, timers, and channels
//   - Hierarchical logging with async dispatch
//   - Type-safe YAML configuration with change listeners
//
// Include this single header for all zero functionality, or include
// individual headers from the subdirectories for finer granularity.
#pragma once

// ============================================================
// Base utilities
// ============================================================
#include "zero/base/noncopyable.h"
#include "zero/base/macro.h"
#include "zero/base/endian.h"
#include "zero/base/singleton.h"
#include "zero/base/lexical_cast.h"
#include "zero/base/any.h"
#include "zero/base/optional.h"
#include "zero/base/expected.h"

// ============================================================
// Thread primitives
// ============================================================
#include "zero/thread/thread.h"
#include "zero/thread/spinlock.h"
#include "zero/thread/mutex.h"
#include "zero/thread/rwlock.h"
#include "zero/thread/semaphore.h"
#include "zero/thread/condition_variable.h"
#include "zero/thread/barrier.h"
#include "zero/thread/cpu_affinity.h"

// ============================================================
// Fiber runtime
// ============================================================
#include "zero/fiber/fiber.h"
#include "zero/fiber/context.h"
#include "zero/fiber/fiber_pool.h"
#include "zero/fiber/stack_pool.h"
#include "zero/fiber/fiber_local.h"
#include "zero/fiber/channel.h"
#include "zero/fiber/fiber_mutex.h"

// ============================================================
// Scheduler
// ============================================================
#include "zero/scheduler/scheduler.h"
#include "zero/scheduler/reactor.h"
#include "zero/scheduler/timer_wheel.h"
#include "zero/scheduler/work_stealing_queue.h"
#include "zero/scheduler/fd_manager.h"
#include "zero/scheduler/hook.h"

// ============================================================
// Networking
// ============================================================
#include "zero/net/buffer.h"
#include "zero/net/address.h"
#include "zero/net/socket.h"
#include "zero/net/stream.h"
#include "zero/net/socket_stream.h"
#include "zero/net/tcp_server.h"
#include "zero/net/tcp_client.h"
#include "zero/net/udp_socket.h"

// ============================================================
// I/O Engines
// ============================================================
#include "zero/io/io_engine.h"
#include "zero/io/epoll_engine.h"
#include "zero/io/uring_engine.h"

// ============================================================
// Logging
// ============================================================
#include "zero/log/log.h"

// ============================================================
// Configuration
// ============================================================
#include "zero/config/config.h"

namespace zero {

class Scheduler;
class Config;

// ============================================================
// Library initialization
// ============================================================

// Initialize the zero library. Must be called once before using any
// other zero features. Creates the default scheduler, initializes
// the fd manager, enables hooks, and loads default configuration.
//
// Parameters:
//   argc, argv: Pass through to parse command-line configuration
//               arguments (--key=value format with ZERO_ prefix).
//
// Returns the default Scheduler instance (ready for start()).
Scheduler& InitZero(int argc = 0, char** argv = nullptr);

// Get the global Config instance (available after InitZero)
Config& GetConfig();

// Get the process-global Scheduler instance
Scheduler& GetScheduler();

// Shutdown the zero library gracefully.
void ShutdownZero();

} // namespace zero
