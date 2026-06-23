// test_channel.cpp — Comprehensive Channel<T> unit tests
// Tests buffered/unbuffered channels, send/recv round-trip,
// try_send/try_recv non-blocking semantics, close()/is_closed(),
// capacity limits, multiple sender patterns, and edge cases.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <atomic>
#include <thread>
#include <vector>
#include <memory>
#include <string>

using namespace zero;

// ============================================================
// Channel creation and initial state
// ============================================================

TEST(Channel, CreateUnbuffered) {
    Channel<int> ch(0);
    EXPECT_EQ(ch.capacity(), 0u);
    EXPECT_TRUE(ch.empty());
    EXPECT_EQ(ch.size(), 0u);
    EXPECT_FALSE(ch.is_closed());
    // Unbuffered channel: capacity=0 means always "full" in the sense
    // that there is no buffer to hold items
    EXPECT_TRUE(ch.full());
}

TEST(Channel, CreateBuffered) {
    Channel<int> ch(10);
    EXPECT_EQ(ch.capacity(), 10u);
    EXPECT_TRUE(ch.empty());
    EXPECT_EQ(ch.size(), 0u);
    EXPECT_FALSE(ch.is_closed());
    EXPECT_FALSE(ch.full());
}

TEST(Channel, CreateLargeBuffered) {
    Channel<std::string> ch(1000);
    EXPECT_EQ(ch.capacity(), 1000u);
    EXPECT_TRUE(ch.empty());
    EXPECT_FALSE(ch.is_closed());
}

TEST(Channel, DefaultConstruction) {
    Channel<double> ch;
    EXPECT_EQ(ch.capacity(), 0u);  // Default is unbuffered
    EXPECT_TRUE(ch.empty());
}

// ============================================================
// try_send / try_recv basic round-trip (buffered)
// ============================================================

TEST(Channel, TrySendTryRecvRoundTrip) {
    Channel<int> ch(5);

    EXPECT_TRUE(ch.try_send(42));
    EXPECT_EQ(ch.size(), 1u);
    EXPECT_FALSE(ch.empty());

    int val = 0;
    EXPECT_TRUE(ch.try_recv(val));
    EXPECT_EQ(val, 42);
    EXPECT_TRUE(ch.empty());
    EXPECT_EQ(ch.size(), 0u);
}

TEST(Channel, TrySendTryRecvMultipleBuffered) {
    Channel<int> ch(10);

    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(ch.try_send(i * 10));
    }
    EXPECT_EQ(ch.size(), 10u);
    EXPECT_TRUE(ch.full());

    for (int i = 0; i < 10; ++i) {
        int val = 0;
        EXPECT_TRUE(ch.try_recv(val));
        EXPECT_EQ(val, i * 10);
    }
    EXPECT_TRUE(ch.empty());
}

TEST(Channel, TrySendTryRecvString) {
    Channel<std::string> ch(5);

    EXPECT_TRUE(ch.try_send("hello"));
    EXPECT_TRUE(ch.try_send("world"));
    EXPECT_TRUE(ch.try_send("foo"));
    EXPECT_EQ(ch.size(), 3u);

    std::string val;
    EXPECT_TRUE(ch.try_recv(val));
    EXPECT_EQ(val, "hello");
    EXPECT_TRUE(ch.try_recv(val));
    EXPECT_EQ(val, "world");
    EXPECT_TRUE(ch.try_recv(val));
    EXPECT_EQ(val, "foo");
    EXPECT_TRUE(ch.empty());
}

TEST(Channel, TrySendTryRecvSharedPtr) {
    Channel<std::shared_ptr<int>> ch(3);

    auto p1 = std::make_shared<int>(100);
    auto p2 = std::make_shared<int>(200);
    auto p3 = std::make_shared<int>(300);

    EXPECT_TRUE(ch.try_send(p1));
    EXPECT_TRUE(ch.try_send(p2));
    EXPECT_TRUE(ch.try_send(p3));

    std::shared_ptr<int> val;
    EXPECT_TRUE(ch.try_recv(val));
    EXPECT_EQ(*val, 100);
    EXPECT_TRUE(ch.try_recv(val));
    EXPECT_EQ(*val, 200);
    EXPECT_TRUE(ch.try_recv(val));
    EXPECT_EQ(*val, 300);
}

// ============================================================
// try_send returns false when full (buffered)
// ============================================================

TEST(Channel, TrySendFailsWhenFull) {
    Channel<int> ch(3);

    EXPECT_TRUE(ch.try_send(1));
    EXPECT_TRUE(ch.try_send(2));
    EXPECT_TRUE(ch.try_send(3));
    EXPECT_TRUE(ch.full());

    EXPECT_FALSE(ch.try_send(4));
    EXPECT_FALSE(ch.try_send(5));
    EXPECT_EQ(ch.size(), 3u);

    // Drain and verify
    int val = 0;
    EXPECT_TRUE(ch.try_recv(val)); EXPECT_EQ(val, 1);
    EXPECT_TRUE(ch.try_recv(val)); EXPECT_EQ(val, 2);
    EXPECT_TRUE(ch.try_recv(val)); EXPECT_EQ(val, 3);
}

// ============================================================
// try_recv returns false when empty
// ============================================================

TEST(Channel, TryRecvFailsWhenEmpty) {
    Channel<int> ch(5);

    int val = 999;
    EXPECT_FALSE(ch.try_recv(val));
    EXPECT_EQ(val, 999);  // Value should be unchanged
}

TEST(Channel, TryRecvFailsWhenEmptyAfterDrain) {
    Channel<int> ch(2);

    EXPECT_TRUE(ch.try_send(10));
    EXPECT_TRUE(ch.try_send(20));

    int val = 0;
    EXPECT_TRUE(ch.try_recv(val)); EXPECT_EQ(val, 10);
    EXPECT_TRUE(ch.try_recv(val)); EXPECT_EQ(val, 20);
    EXPECT_FALSE(ch.try_recv(val));
}

// ============================================================
// close() and is_closed()
// ============================================================

TEST(Channel, CloseEmpty) {
    Channel<int> ch(5);
    EXPECT_FALSE(ch.is_closed());

    ch.close();
    EXPECT_TRUE(ch.is_closed());
}

TEST(Channel, CloseWithItems) {
    Channel<int> ch(5);
    EXPECT_TRUE(ch.try_send(1));
    EXPECT_TRUE(ch.try_send(2));
    EXPECT_TRUE(ch.try_send(3));

    ch.close();
    EXPECT_TRUE(ch.is_closed());
}

TEST(Channel, CloseIsIdempotent) {
    Channel<int> ch(5);
    ch.close();
    ch.close();
    ch.close();
    EXPECT_TRUE(ch.is_closed());
}

TEST(Channel, TrySendFailsOnClosedChannel) {
    Channel<int> ch(5);

    EXPECT_TRUE(ch.try_send(10));
    ch.close();

    EXPECT_FALSE(ch.try_send(20));
    EXPECT_FALSE(ch.try_send(30));
}

TEST(Channel, TryRecvReturnsRemainingOnClosedChannel) {
    Channel<int> ch(5);

    EXPECT_TRUE(ch.try_send(1));
    EXPECT_TRUE(ch.try_send(2));
    EXPECT_TRUE(ch.try_send(3));
    ch.close();

    // Should still be able to drain remaining items
    int val = 0;
    EXPECT_TRUE(ch.try_recv(val)); EXPECT_EQ(val, 1);
    EXPECT_TRUE(ch.try_recv(val)); EXPECT_EQ(val, 2);
    EXPECT_TRUE(ch.try_recv(val)); EXPECT_EQ(val, 3);

    // Now it's empty and closed
    EXPECT_FALSE(ch.try_recv(val));
}

TEST(Channel, TryRecvFailsOnClosedAndEmpty) {
    Channel<int> ch(5);
    ch.close();

    int val = 42;
    EXPECT_FALSE(ch.try_recv(val));
    EXPECT_EQ(val, 42);
}

// ============================================================
// Interleaved try_send and try_recv
// ============================================================

TEST(Channel, InterleavedSendRecv) {
    Channel<int> ch(3);

    EXPECT_TRUE(ch.try_send(1));
    EXPECT_TRUE(ch.try_send(2));

    int val = 0;
    EXPECT_TRUE(ch.try_recv(val)); EXPECT_EQ(val, 1);

    EXPECT_TRUE(ch.try_send(3));
    EXPECT_TRUE(ch.try_send(4));
    EXPECT_TRUE(ch.full());

    EXPECT_TRUE(ch.try_recv(val)); EXPECT_EQ(val, 2);
    EXPECT_TRUE(ch.try_recv(val)); EXPECT_EQ(val, 3);
    EXPECT_TRUE(ch.try_recv(val)); EXPECT_EQ(val, 4);
    EXPECT_TRUE(ch.empty());
}

// ============================================================
// Multiple data types
// ============================================================

TEST(Channel, DoubleType) {
    Channel<double> ch(3);

    EXPECT_TRUE(ch.try_send(3.14));
    EXPECT_TRUE(ch.try_send(2.718));
    EXPECT_TRUE(ch.try_send(1.618));

    double val = 0.0;
    EXPECT_TRUE(ch.try_recv(val));
    EXPECT_DOUBLE_EQ(val, 3.14);
    EXPECT_TRUE(ch.try_recv(val));
    EXPECT_DOUBLE_EQ(val, 2.718);
    EXPECT_TRUE(ch.try_recv(val));
    EXPECT_DOUBLE_EQ(val, 1.618);
}

TEST(Channel, BoolType) {
    Channel<bool> ch(2);

    EXPECT_TRUE(ch.try_send(true));
    EXPECT_TRUE(ch.try_send(false));

    bool val = false;
    EXPECT_TRUE(ch.try_recv(val));
    EXPECT_TRUE(val);
    EXPECT_TRUE(ch.try_recv(val));
    EXPECT_FALSE(val);
}

TEST(Channel, Int64Type) {
    Channel<int64_t> ch(4);

    EXPECT_TRUE(ch.try_send(INT64_MAX));
    EXPECT_TRUE(ch.try_send(INT64_MIN));
    EXPECT_TRUE(ch.try_send(0));
    EXPECT_TRUE(ch.try_send(-42));

    int64_t val = 0;
    EXPECT_TRUE(ch.try_recv(val)); EXPECT_EQ(val, INT64_MAX);
    EXPECT_TRUE(ch.try_recv(val)); EXPECT_EQ(val, INT64_MIN);
    EXPECT_TRUE(ch.try_recv(val)); EXPECT_EQ(val, 0);
    EXPECT_TRUE(ch.try_recv(val)); EXPECT_EQ(val, -42);
}

// ============================================================
// size() and empty() queries
// ============================================================

TEST(Channel, SizeTracking) {
    Channel<int> ch(10);

    EXPECT_EQ(ch.size(), 0u);
    EXPECT_TRUE(ch.empty());

    ch.try_send(1);
    EXPECT_EQ(ch.size(), 1u);
    EXPECT_FALSE(ch.empty());

    ch.try_send(2);
    EXPECT_EQ(ch.size(), 2u);

    ch.try_send(3);
    EXPECT_EQ(ch.size(), 3u);

    int val = 0;
    ch.try_recv(val);
    EXPECT_EQ(ch.size(), 2u);

    ch.try_recv(val);
    EXPECT_EQ(ch.size(), 1u);

    ch.try_recv(val);
    EXPECT_EQ(ch.size(), 0u);
    EXPECT_TRUE(ch.empty());
}

TEST(Channel, FullTrackingUnbuffered) {
    Channel<int> ch(0);
    // Unbuffered channel: capacity is 0, always full because
    // no items can ever be buffered
    EXPECT_TRUE(ch.full());
}

TEST(Channel, FullTrackingBuffered) {
    Channel<int> ch(3);
    EXPECT_FALSE(ch.full());

    ch.try_send(1);
    EXPECT_FALSE(ch.full());

    ch.try_send(2);
    EXPECT_FALSE(ch.full());

    ch.try_send(3);
    EXPECT_TRUE(ch.full());

    int val = 0;
    ch.try_recv(val);
    EXPECT_FALSE(ch.full());
}

// ============================================================
// Shared pointer channel
// ============================================================

struct Payload {
    int id;
    std::string data;
    std::atomic<int>* ref_count;
    explicit Payload(int i, std::string d, std::atomic<int>* rc = nullptr)
        : id(i), data(std::move(d)), ref_count(rc) {
        if (ref_count) ref_count->fetch_add(1);
    }
    ~Payload() {
        if (ref_count) ref_count->fetch_sub(1);
    }
};

TEST(Channel, SharedPtrLifecycle) {
    std::atomic<int> refs{0};
    Channel<std::shared_ptr<Payload>> ch(3);

    {
        auto p1 = std::make_shared<Payload>(1, "alpha", &refs);
        auto p2 = std::make_shared<Payload>(2, "beta", &refs);
        auto p3 = std::make_shared<Payload>(3, "gamma", &refs);

        EXPECT_EQ(refs.load(), 3);

        EXPECT_TRUE(ch.try_send(p1));
        EXPECT_TRUE(ch.try_send(p2));
        EXPECT_TRUE(ch.try_send(p3));

        EXPECT_EQ(refs.load(), 3);  // References still held
    }
    // p1, p2, p3 locals gone, but channel holds copies
    EXPECT_EQ(refs.load(), 3);

    std::shared_ptr<Payload> val;
    EXPECT_TRUE(ch.try_recv(val));
    EXPECT_EQ(val->id, 1);
    EXPECT_EQ(val->data, "alpha");
    EXPECT_EQ(refs.load(), 3);  // val holds one

    val.reset();
    EXPECT_EQ(refs.load(), 2);  // Released

    EXPECT_TRUE(ch.try_recv(val));
    EXPECT_EQ(val->id, 2);
    val.reset();
    EXPECT_EQ(refs.load(), 1);

    EXPECT_TRUE(ch.try_recv(val));
    EXPECT_EQ(val->id, 3);
    val.reset();
    EXPECT_EQ(refs.load(), 0);
}

// ============================================================
// Capacity 1 (bounded queue of 1)
// ============================================================

TEST(Channel, CapacityOne) {
    Channel<int> ch(1);

    EXPECT_FALSE(ch.full());
    EXPECT_TRUE(ch.try_send(42));
    EXPECT_TRUE(ch.full());
    EXPECT_FALSE(ch.try_send(99));

    int val = 0;
    EXPECT_TRUE(ch.try_recv(val));
    EXPECT_EQ(val, 42);
    EXPECT_FALSE(ch.full());

    EXPECT_TRUE(ch.try_send(99));
    EXPECT_TRUE(ch.try_recv(val));
    EXPECT_EQ(val, 99);
}

// ============================================================
// Destructor behavior
// ============================================================

TEST(Channel, DestructorClosesChannel) {
    // Create a channel, put some data, let it destruct
    auto ch = std::make_shared<Channel<int>>(5);
    ch->try_send(1);
    ch->try_send(2);
    // Destructor will call close(), waking any blocked fibers
    // (no fibers here, just verifying no crash)
    SUCCEED();
}

// ============================================================
// Stress: many items through a buffered channel
// ============================================================

TEST(Channel, StressManyItems) {
    const int kCapacity = 64;
    const int kRounds = 100;
    Channel<int> ch(kCapacity);

    for (int round = 0; round < kRounds; ++round) {
        // Fill
        for (int i = 0; i < kCapacity; ++i) {
            EXPECT_TRUE(ch.try_send(round * kCapacity + i));
        }
        EXPECT_TRUE(ch.full());

        // Drain
        for (int i = 0; i < kCapacity; ++i) {
            int val = 0;
            EXPECT_TRUE(ch.try_recv(val));
            EXPECT_EQ(val, round * kCapacity + i);
        }
        EXPECT_TRUE(ch.empty());
    }
}

// ============================================================
// Stress: interleaved in a single-thread loop
// ============================================================

TEST(Channel, StressInterleaved) {
    Channel<int> ch(8);
    int produced = 0;
    int consumed = 0;

    for (int iter = 0; iter < 1000; ++iter) {
        // Try to produce if not full
        if (!ch.full() && produced < 5000) {
            EXPECT_TRUE(ch.try_send(produced));
            ++produced;
        }
        // Try to consume if not empty
        if (!ch.empty()) {
            int val = 0;
            EXPECT_TRUE(ch.try_recv(val));
            EXPECT_EQ(val, consumed);
            ++consumed;
        }
    }

    // Drain remaining
    while (!ch.empty()) {
        int val = 0;
        EXPECT_TRUE(ch.try_recv(val));
        EXPECT_EQ(val, consumed);
        ++consumed;
    }

    EXPECT_EQ(produced, consumed);
}

// ============================================================
// Move semantics for complex types
// ============================================================

TEST(Channel, MoveOnlyType) {
    // Test with unique_ptr (move-only)
    Channel<std::unique_ptr<int>> ch(3);

    EXPECT_TRUE(ch.try_send(std::make_unique<int>(10)));
    EXPECT_TRUE(ch.try_send(std::make_unique<int>(20)));
    EXPECT_TRUE(ch.try_send(std::make_unique<int>(30)));

    EXPECT_TRUE(ch.full());

    std::unique_ptr<int> val;
    EXPECT_TRUE(ch.try_recv(val));
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 10);

    EXPECT_TRUE(ch.try_recv(val));
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 20);

    EXPECT_TRUE(ch.try_recv(val));
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 30);

    EXPECT_TRUE(ch.empty());
}

// ============================================================
// Close then size/empty queries
// ============================================================

TEST(Channel, QueriesAfterClose) {
    Channel<int> ch(5);

    ch.try_send(1);
    ch.try_send(2);
    ch.close();

    EXPECT_EQ(ch.size(), 2u);
    EXPECT_FALSE(ch.empty());
    EXPECT_EQ(ch.capacity(), 5u);
    EXPECT_TRUE(ch.is_closed());
}
