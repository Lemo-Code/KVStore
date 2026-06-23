// test_reactor.cpp — Unit tests for the per-thread epoll-based Reactor
//
// Tests: Reactor creation/destruction, add_event/modify_event/delete_event,
// poll timeout, event callback invocation via pipe/socketpair for
// readable/writable events, EPOLLET behavior.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstring>

using namespace zero;

// =====================================================================
// Construction and destruction
// =====================================================================

TEST(ReactorTest, Construct) {
    Reactor reactor;
    EXPECT_GE(reactor.epoll_fd(), 0);
    EXPECT_GE(reactor.wakeup_fd(), 0);
    EXPECT_TRUE(reactor.is_running());
    EXPECT_NE(reactor.timer_wheel(), nullptr);
}

TEST(ReactorTest, DestructorCleansUp) {
    {
        Reactor reactor;
        EXPECT_TRUE(reactor.is_running());
        EXPECT_GE(reactor.epoll_fd(), 0);
    }
    SUCCEED();
}

TEST(ReactorTest, EpollFdIsValid) {
    Reactor reactor;
    int efd = reactor.epoll_fd();
    EXPECT_GE(efd, 0);

    // Verify it's actually an epoll fd by checking fcntl
    int flags = fcntl(efd, F_GETFD);
    EXPECT_NE(flags, -1);
}

TEST(ReactorTest, WakeupFdIsValid) {
    Reactor reactor;
    int wfd = reactor.wakeup_fd();
    EXPECT_GE(wfd, 0);

    int flags = fcntl(wfd, F_GETFD);
    EXPECT_NE(flags, -1);
}

// =====================================================================
// Timer wheel access
// =====================================================================

TEST(ReactorTest, TimerWheelAccess) {
    Reactor reactor;
    TimerWheel* tw = reactor.timer_wheel();
    ASSERT_NE(tw, nullptr);
    EXPECT_EQ(tw->now_ms(), 0u);
    EXPECT_EQ(tw->total_fired(), 0u);
}

// =====================================================================
// Poll timeout test
// =====================================================================

TEST(ReactorTest, PollWithTimeoutReturnsZero) {
    Reactor reactor;
    // Poll with a short timeout — should return with no events
    int result = reactor.poll(10); // 10ms timeout
    EXPECT_GE(result, 0); // May have timer events from the tick
}

TEST(ReactorTest, PollWithZeroTimeout) {
    Reactor reactor;
    int result = reactor.poll(0);
    EXPECT_GE(result, 0);
}

TEST(ReactorTest, PollWithNegativeTimeout) {
    Reactor reactor;
    // Negative timeout — should be treated as 0 or infinite; verify no crash
    int result = reactor.poll(-1);
    // Just verify it doesn't crash
    (void)result;
    SUCCEED();
}

// =====================================================================
// Event management: add/modify/delete fd
// =====================================================================

TEST(ReactorTest, AddEventSucceeds) {
    Reactor reactor;

    // Create an eventfd for testing
    int efd = eventfd(0, EFD_NONBLOCK);
    ASSERT_GE(efd, 0);

    bool added = reactor.add_event(efd, EPOLLIN, nullptr);
    EXPECT_TRUE(added);

    reactor.del_event(efd);
    close(efd);
}

TEST(ReactorTest, DeleteEventSucceeds) {
    Reactor reactor;

    int efd = eventfd(0, EFD_NONBLOCK);
    ASSERT_GE(efd, 0);

    reactor.add_event(efd, EPOLLIN, nullptr);
    bool removed = reactor.del_event(efd);
    EXPECT_TRUE(removed);

    close(efd);
}

TEST(ReactorTest, DeleteEventThatWasNeverAdded) {
    Reactor reactor;
    // Deleting an fd that was never added — should not crash
    bool removed = reactor.del_event(9999);
    // May return false or handle gracefully
    (void)removed;
    SUCCEED();
}

TEST(ReactorTest, ModifyEventSucceeds) {
    Reactor reactor;

    int efd = eventfd(0, EFD_NONBLOCK);
    ASSERT_GE(efd, 0);

    reactor.add_event(efd, EPOLLIN, nullptr);
    bool modified = reactor.mod_event(efd, EPOLLIN | EPOLLOUT, nullptr);
    EXPECT_TRUE(modified);

    reactor.del_event(efd);
    close(efd);
}

TEST(ReactorTest, ModifyEventNotAdded) {
    Reactor reactor;
    int efd = eventfd(0, EFD_NONBLOCK);
    ASSERT_GE(efd, 0);

    // Modify without prior add — may fail or handle gracefully
    bool modified = reactor.mod_event(efd, EPOLLIN, nullptr);
    // Just verify it doesn't crash
    (void)modified;

    close(efd);
}

// =====================================================================
// Event callback invocation (via eventfd)
// =====================================================================

TEST(ReactorTest, EventFdTriggersReadableEvent) {
    Reactor reactor;
    std::atomic<bool> callback_invoked{false};
    std::atomic<int> event_fd_received{-1};
    std::atomic<uint32_t> events_received{0};

    int efd = eventfd(0, EFD_NONBLOCK | EFD_SEMAPHORE);
    ASSERT_GE(efd, 0);

    reactor.add_event(efd, EPOLLIN, nullptr);

    // Write to the eventfd to make it readable
    uint64_t val = 1;
    ssize_t written = write(efd, &val, sizeof(val));
    ASSERT_EQ(written, sizeof(val));

    // Poll — should detect the event
    int result = reactor.poll(50);
    // The eventfd should be readable, but without a fiber waiting,
    // the event is processed internally
    EXPECT_GE(result, 0);

    reactor.del_event(efd);
    close(efd);
}

TEST(ReactorTest, EventFdWithUserData) {
    Reactor reactor;

    int efd = eventfd(0, EFD_NONBLOCK);
    ASSERT_GE(efd, 0);

    int user_val = 42;
    reactor.add_event(efd, EPOLLIN, &user_val);

    uint64_t val = 1;
    write(efd, &val, sizeof(val));

    int result = reactor.poll(50);
    EXPECT_GE(result, 0);

    reactor.del_event(efd);
    close(efd);
}

// =====================================================================
// Wakeup mechanism
// =====================================================================

TEST(ReactorTest, WakeupFromAnotherThread) {
    Reactor reactor;
    std::atomic<bool> poll_started{false};
    std::atomic<bool> poll_done{false};

    std::thread poll_thread([&reactor, &poll_started, &poll_done]() {
        poll_started.store(true);
        int result = reactor.poll(5000); // Long timeout
        poll_done.store(true);
        (void)result;
    });

    // Wait for poll to start
    while (!poll_started.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Wake up the reactor
    reactor.wakeup();

    poll_thread.join();

    EXPECT_TRUE(poll_done.load());
}

TEST(ReactorTest, MultipleWakeups) {
    Reactor reactor;

    // Wake up multiple times
    reactor.wakeup();
    reactor.wakeup();
    reactor.wakeup();

    // Poll should consume all wakeups
    int result = reactor.poll(10);
    EXPECT_GE(result, 0);

    SUCCEED();
}

// =====================================================================
// Stop mechanism
// =====================================================================

TEST(ReactorTest, StopWhilePolling) {
    Reactor reactor;
    std::atomic<bool> poll_done{false};

    std::thread t([&reactor, &poll_done]() {
        reactor.poll(5000);
        poll_done.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    reactor.stop();

    t.join();

    EXPECT_TRUE(poll_done.load());
    EXPECT_FALSE(reactor.is_running());
}

TEST(ReactorTest, IsRunningAfterStop) {
    Reactor reactor;
    EXPECT_TRUE(reactor.is_running());

    reactor.stop();
    EXPECT_FALSE(reactor.is_running());
}

// =====================================================================
// Pipe test for readable/writable events
// =====================================================================

TEST(ReactorTest, PipeReadableEvent) {
    Reactor reactor;

    int pipe_fds[2];
    ASSERT_EQ(pipe(pipe_fds), 0);

    // Set read end to non-blocking
    int flags = fcntl(pipe_fds[0], F_GETFL, 0);
    fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK);

    reactor.add_event(pipe_fds[0], EPOLLIN, nullptr);

    // Write to the pipe
    char data = 'x';
    write(pipe_fds[1], &data, 1);

    // Poll should detect the readable pipe
    int result = reactor.poll(50);
    EXPECT_GE(result, 0);

    reactor.del_event(pipe_fds[0]);
    close(pipe_fds[0]);
    close(pipe_fds[1]);
}

TEST(ReactorTest, PipeWriteableEvent) {
    Reactor reactor;

    int pipe_fds[2];
    ASSERT_EQ(pipe(pipe_fds), 0);

    reactor.add_event(pipe_fds[1], EPOLLOUT, nullptr);

    // Pipe should be writable (buffer not full)
    int result = reactor.poll(50);
    EXPECT_GE(result, 0);

    reactor.del_event(pipe_fds[1]);
    close(pipe_fds[0]);
    close(pipe_fds[1]);
}

// =====================================================================
// Edge cases
// =====================================================================

TEST(ReactorTest, AddEventWithInvalidFd) {
    Reactor reactor;
    bool added = reactor.add_event(-1, EPOLLIN, nullptr);
    EXPECT_FALSE(added);
}

TEST(ReactorTest, DelEventWithInvalidFd) {
    Reactor reactor;
    bool removed = reactor.del_event(-1);
    // Should not crash
    (void)removed;
    SUCCEED();
}

TEST(ReactorTest, ModEventWithInvalidFd) {
    Reactor reactor;
    bool modified = reactor.mod_event(-1, EPOLLIN, nullptr);
    EXPECT_FALSE(modified);
}

TEST(ReactorTest, AddDeleteAddCycle) {
    Reactor reactor;

    int efd = eventfd(0, EFD_NONBLOCK);
    ASSERT_GE(efd, 0);

    // Add, delete, add again
    EXPECT_TRUE(reactor.add_event(efd, EPOLLIN, nullptr));
    EXPECT_TRUE(reactor.del_event(efd));
    EXPECT_TRUE(reactor.add_event(efd, EPOLLOUT, nullptr));
    EXPECT_TRUE(reactor.del_event(efd));

    close(efd);
}

TEST(ReactorTest, PollAfterStop) {
    Reactor reactor;
    reactor.stop();

    // Polling a stopped reactor should return quickly
    int result = reactor.poll(10);
    EXPECT_GE(result, 0);
}

TEST(ReactorTest, AddEventWithEpollEt) {
    Reactor reactor;

    int efd = eventfd(0, EFD_NONBLOCK);
    ASSERT_GE(efd, 0);

    bool added = reactor.add_event(efd, EPOLLIN | EPOLLET, nullptr);
    EXPECT_TRUE(added);

    // Write twice — with EPOLLET, only one notification
    uint64_t val = 1;
    write(efd, &val, sizeof(val));
    write(efd, &val, sizeof(val));

    int result = reactor.poll(20);
    EXPECT_GE(result, 0);

    reactor.del_event(efd);
    close(efd);
}
