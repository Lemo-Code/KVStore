// Zero — High-Performance Network Library
// 统一头文件

#pragma once

// Base
#include "zero/base/noncopyable.h"
#include "zero/base/macro.h"
#include "zero/base/endian.h"
#include "zero/base/singleton.h"
#include "zero/base/lexicalcast.h"

// Thread
#include "zero/thread/thread.h"
#include "zero/thread/mutex.h"
#include "zero/thread/semaphore.h"
#include "zero/thread/cpu_affinity.h"

// Fiber
#include "zero/fiber/context.h"
#include "zero/fiber/fiber.h"
#include "zero/fiber/stack_pool.h"
#include "zero/fiber/fiber_pool.h"
#include "zero/fiber/fiber_local.h"

// Scheduler
#include "zero/scheduler/scheduler.h"
#include "zero/scheduler/work_stealing_queue.h"

// Net
#include "zero/net/buffer.h"
#include "zero/net/address.h"
#include "zero/net/socket.h"
#include "zero/net/stream.h"
#include "zero/net/socket_stream.h"
#include "zero/net/tcp_server.h"

// Log
#include "zero/log/log.h"
#include "zero/log/async_log.h"

// Config
#include "zero/config/config.h"

// Framework (in namespace zero)
namespace zero {
    void InitZero(int argc, char** argv);
    void InitConfig();
    void LoadConfig(const std::string& path);

    // 便捷的配置读取 (从 ConfigVar 获取当前值)
    namespace config {
        // Log
        int          LogLevel();
        std::string  LogFile();
        std::string  LogPattern();
        bool         LogAsync();
        int          LogAsyncThreads();

        // Fiber
        uint32_t     FiberStackSize();
        uint32_t     FiberPoolSize();

        // Scheduler
        int          SchedulerThreads();
        bool         SchedulerUseCaller();
        std::string  SchedulerName();

        // Socket
        int64_t      SocketConnectTimeoutMs();
        int64_t      SocketRecvTimeoutMs();
        int64_t      SocketSendTimeoutMs();
        bool         SocketTcpNoDelay();
        bool         SocketSoReuseaddr();
        int          SocketListenBacklog();

        // Reactor
        int          ReactorPollTimeoutMs();
        size_t       ReactorMaxEvents();

        // TcpServer
        int          TcpServerKeepalive();
        int          TcpServerTimeoutMs();
    }
}
