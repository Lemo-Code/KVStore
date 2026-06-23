// test_fd_manager.cpp — Unit tests for the process-wide FdManager singleton
//
// Tests: add/get/remove fd context, auto-create on get, is_socket detection,
// nonblock status tracking, thread-safe access, stdin/stdout/stderr fds,
// removing non-existent fd, max fd limit.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include <atomic>

using namespace zero;

// =====================================================================
// Singleton and basic access
// =====================================================================

TEST(FdManagerTest, SingletonReturnsSameInstance) {
    FdManager& m1 = FdManager::instance();
    FdManager& m2 = FdManager::instance();
    EXPECT_EQ(&m1, &m2);
}

TEST(FdManagerTest, GetFdContextAutoCreate) {
    auto& mgr = FdManager::instance();
    int fd = 100; // Use a likely-unused fd number
    FdManager::FdContext* ctx = mgr.get(fd, true);
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->fd, fd);
    EXPECT_FALSE(ctx->is_socket);
    EXPECT_FALSE(ctx->sys_nonblock);
    EXPECT_FALSE(ctx->user_nonblock);
    EXPECT_EQ(ctx->read_fiber, nullptr);
    EXPECT_EQ(ctx->write_fiber, nullptr);
    EXPECT_EQ(ctx->reactor, nullptr);
    EXPECT_FALSE(ctx->closed);

    // Clean up
    mgr.remove(fd);
}

TEST(FdManagerTest, GetFdContextNoAutoCreate) {
    auto& mgr = FdManager::instance();
    int fd = 101;
    // Without auto_create, should return nullptr for non-existent
    FdManager::FdContext* ctx = mgr.get(fd, false);
    EXPECT_EQ(ctx, nullptr);
}

TEST(FdManagerTest, GetFdContextAfterCreation) {
    auto& mgr = FdManager::instance();
    int fd = 102;

    // First access with auto_create
    FdManager::FdContext* ctx1 = mgr.get(fd, true);
    ASSERT_NE(ctx1, nullptr);

    // Second access should return the same context
    FdManager::FdContext* ctx2 = mgr.get(fd, false);
    EXPECT_EQ(ctx1, ctx2);

    mgr.remove(fd);
}

// =====================================================================
// Remove tests
// =====================================================================

TEST(FdManagerTest, RemoveExistingFd) {
    auto& mgr = FdManager::instance();
    int fd = 103;

    mgr.get(fd, true);
    mgr.remove(fd);

    // After removal, should return nullptr (without auto_create)
    FdManager::FdContext* ctx = mgr.get(fd, false);
    EXPECT_EQ(ctx, nullptr);
}

TEST(FdManagerTest, RemoveNonExistentFdDoesNotCrash) {
    auto& mgr = FdManager::instance();
    // Removing a fd that was never added should be safe
    mgr.remove(99999);
    SUCCEED();
}

TEST(FdManagerTest, RemoveThenRecreate) {
    auto& mgr = FdManager::instance();
    int fd = 104;

    mgr.get(fd, true);
    mgr.remove(fd);

    // Recreate — should get a fresh context
    FdManager::FdContext* ctx = mgr.get(fd, true);
    ASSERT_NE(ctx, nullptr);
    EXPECT_FALSE(ctx->closed);

    mgr.remove(fd);
}

// =====================================================================
// Close tracking
// =====================================================================

TEST(FdManagerTest, ClosedFlagIsTracked) {
    auto& mgr = FdManager::instance();
    int fd = 105;

    FdManager::FdContext* ctx = mgr.get(fd, true);
    ASSERT_NE(ctx, nullptr);
    EXPECT_FALSE(ctx->closed);

    ctx->closed = true;
    EXPECT_TRUE(ctx->closed);

    mgr.remove(fd);
}

// =====================================================================
// Nonblock status tracking
// =====================================================================

TEST(FdManagerTest, SysNonblockTracking) {
    auto& mgr = FdManager::instance();
    int fd = 106;

    FdManager::FdContext* ctx = mgr.get(fd, true);
    ASSERT_NE(ctx, nullptr);

    ctx->sys_nonblock = true;
    EXPECT_TRUE(ctx->sys_nonblock);

    ctx->sys_nonblock = false;
    EXPECT_FALSE(ctx->sys_nonblock);

    mgr.remove(fd);
}

TEST(FdManagerTest, UserNonblockTracking) {
    auto& mgr = FdManager::instance();
    int fd = 107;

    FdManager::FdContext* ctx = mgr.get(fd, true);
    ASSERT_NE(ctx, nullptr);

    ctx->user_nonblock = true;
    EXPECT_TRUE(ctx->user_nonblock);

    mgr.remove(fd);
}

// =====================================================================
// Real fd tests (stdin, stdout, stderr)
// =====================================================================

TEST(FdManagerTest, GetContextForStdin) {
    auto& mgr = FdManager::instance();
    FdManager::FdContext* ctx = mgr.get(STDIN_FILENO, true);
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->fd, STDIN_FILENO);

    mgr.remove(STDIN_FILENO);
}

TEST(FdManagerTest, GetContextForStdout) {
    auto& mgr = FdManager::instance();
    FdManager::FdContext* ctx = mgr.get(STDOUT_FILENO, true);
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->fd, STDOUT_FILENO);

    mgr.remove(STDOUT_FILENO);
}

TEST(FdManagerTest, GetContextForStderr) {
    auto& mgr = FdManager::instance();
    FdManager::FdContext* ctx = mgr.get(STDERR_FILENO, true);
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->fd, STDERR_FILENO);

    mgr.remove(STDERR_FILENO);
}

// =====================================================================
// Timeout tracking
// =====================================================================

TEST(FdManagerTest, RecvSendTimeoutDefaults) {
    auto& mgr = FdManager::instance();
    int fd = 108;

    FdManager::FdContext* ctx = mgr.get(fd, true);
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->recv_timeout_ms, -1); // Infinite by default
    EXPECT_EQ(ctx->send_timeout_ms, -1);

    mgr.remove(fd);
}

TEST(FdManagerTest, SetRecvTimeout) {
    auto& mgr = FdManager::instance();
    int fd = 109;

    FdManager::FdContext* ctx = mgr.get(fd, true);
    ASSERT_NE(ctx, nullptr);

    ctx->recv_timeout_ms = 5000;
    EXPECT_EQ(ctx->recv_timeout_ms, 5000);

    mgr.remove(fd);
}

TEST(FdManagerTest, SetSendTimeout) {
    auto& mgr = FdManager::instance();
    int fd = 110;

    FdManager::FdContext* ctx = mgr.get(fd, true);
    ASSERT_NE(ctx, nullptr);

    ctx->send_timeout_ms = 3000;
    EXPECT_EQ(ctx->send_timeout_ms, 3000);

    mgr.remove(fd);
}

// =====================================================================
// Thread safety tests
// =====================================================================

TEST(FdManagerTest, ThreadSafeAccess) {
    auto& mgr = FdManager::instance();
    std::atomic<bool> done{false};
    std::atomic<int> errors{0};

    auto worker = [&mgr, &done, &errors](int base_fd) {
        for (int i = 0; i < 100 && !done.load(); ++i) {
            int fd = base_fd + i;
            FdManager::FdContext* ctx = mgr.get(fd, true);
            if (!ctx) {
                errors.fetch_add(1);
                continue;
            }
            ctx->recv_timeout_ms = i * 10;
            if (ctx->recv_timeout_ms != i * 10) {
                errors.fetch_add(1);
            }
            mgr.remove(fd);
        }
    };

    std::thread t1(worker, 200);
    std::thread t2(worker, 400);
    std::thread t3(worker, 600);
    std::thread t4(worker, 800);

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    EXPECT_EQ(errors.load(), 0);
}

// =====================================================================
// Reset context
// =====================================================================

TEST(FdManagerTest, ResetContext) {
    auto& mgr = FdManager::instance();
    int fd = 120;

    FdManager::FdContext* ctx = mgr.get(fd, true);
    ASSERT_NE(ctx, nullptr);

    // Modify state
    ctx->is_socket = true;
    ctx->sys_nonblock = true;
    ctx->recv_timeout_ms = 1000;
    ctx->closed = true;

    ctx->reset();

    // All should be back to defaults
    EXPECT_FALSE(ctx->is_socket);
    EXPECT_FALSE(ctx->sys_nonblock);
    EXPECT_FALSE(ctx->user_nonblock);
    EXPECT_EQ(ctx->recv_timeout_ms, -1);
    EXPECT_EQ(ctx->send_timeout_ms, -1);
    EXPECT_EQ(ctx->read_fiber, nullptr);
    EXPECT_EQ(ctx->write_fiber, nullptr);
    EXPECT_FALSE(ctx->closed);

    mgr.remove(fd);
}

// =====================================================================
// Edge cases
// =====================================================================

TEST(FdManagerTest, LargeFdNumbers) {
    auto& mgr = FdManager::instance();

    // Test with fd numbers across a range
    for (int fd : {0, 1, 2, 100, 1000, 10000}) {
        FdManager::FdContext* ctx = mgr.get(fd, true);
        ASSERT_NE(ctx, nullptr);
        EXPECT_EQ(ctx->fd, fd);
        mgr.remove(fd);
    }

    // After removal, get without auto_create should return null
    for (int fd : {100, 1000, 10000}) {
        EXPECT_EQ(mgr.get(fd, false), nullptr);
    }
}

TEST(FdManagerTest, ActiveFdCount) {
    auto& mgr = FdManager::instance();

    size_t before = mgr.active_fd_count();

    mgr.get(150, true);
    mgr.get(151, true);
    mgr.get(152, true);

    size_t after_add = mgr.active_fd_count();
    EXPECT_GE(after_add, before + 3);

    mgr.remove(150);
    mgr.remove(151);
    mgr.remove(152);

    size_t after_remove = mgr.active_fd_count();
    EXPECT_LE(after_remove, after_add);
}

TEST(FdManagerTest, ReactorAssociation) {
    auto& mgr = FdManager::instance();
    int fd = 160;

    FdManager::FdContext* ctx = mgr.get(fd, true);
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->reactor, nullptr);

    // We can set the reactor pointer — manual association
    // In real use, this would be set by the scheduler
    ctx->reactor = reinterpret_cast<Reactor*>(0x1);
    EXPECT_NE(ctx->reactor, nullptr);

    ctx->reactor = nullptr;
    EXPECT_EQ(ctx->reactor, nullptr);

    mgr.remove(fd);
}

TEST(FdManagerTest, RegisteredEventsTracking) {
    auto& mgr = FdManager::instance();
    int fd = 170;

    FdManager::FdContext* ctx = mgr.get(fd, true);
    ASSERT_NE(ctx, nullptr);

    ctx->registered_events = EPOLLIN | EPOLLOUT;
    EXPECT_EQ(ctx->registered_events, static_cast<uint32_t>(EPOLLIN | EPOLLOUT));

    ctx->registered_events = 0;
    EXPECT_EQ(ctx->registered_events, 0u);

    mgr.remove(fd);
}
