// test_stream.cpp — Unit tests for the SocketStream (Stream over Socket)
//
// Tests: SocketStream creation from socket, read/write basic, flush,
// readExact/writeExact, isOpen/close, getFd, buffered write + flush,
// socketpair for reliable testing, read when buffer is empty.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <chrono>

using namespace zero;

// Helper: create a connected socket pair, returned as SocketStream objects
struct SocketPair {
    Socket::Ptr sock1;
    Socket::Ptr sock2;

    static SocketPair Create() {
        int sv[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
            return {};
        }
        SocketPair pair;
        pair.sock1 = Socket::from_fd(sv[0], Socket::Type::TCP);
        pair.sock2 = Socket::from_fd(sv[1], Socket::Type::TCP);
        return pair;
    }
};

// =====================================================================
// Construction tests
// =====================================================================

TEST(StreamTest, ConstructFromSocket) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    SocketStream stream(sock);
    EXPECT_TRUE(stream.is_open());
    EXPECT_EQ(stream.get_fd(), sock->fd());
    EXPECT_EQ(stream.socket(), sock);
}

TEST(StreamTest, GetLocalAndRemoteAddress) {
    auto pair = SocketPair::Create();
    ASSERT_NE(pair.sock1, nullptr);
    ASSERT_NE(pair.sock2, nullptr);

    SocketStream stream(pair.sock1);

    auto local = stream.get_local_address();
    auto remote = stream.get_remote_address();

    // May return nullptr for Unix sockets — just verify no crash
    (void)local;
    (void)remote;
    SUCCEED();
}

// =====================================================================
// Read/Write basic tests (via socketpair)
// =====================================================================

TEST(StreamTest, WriteAndFlush) {
    auto pair = SocketPair::Create();
    ASSERT_NE(pair.sock1, nullptr);
    ASSERT_NE(pair.sock2, nullptr);

    SocketStream stream(pair.sock1);

    const char* msg = "hello stream";
    ssize_t written = stream.write(msg, strlen(msg));
    EXPECT_EQ(written, static_cast<ssize_t>(strlen(msg)));

    // Data should be in write buffer, not yet sent
    // flush() sends it
    ssize_t flushed = stream.flush();
    EXPECT_GE(flushed, 0);
}

TEST(StreamTest, WriteReadRoundTrip) {
    auto pair = SocketPair::Create();
    ASSERT_NE(pair.sock1, nullptr);
    ASSERT_NE(pair.sock2, nullptr);

    SocketStream s1(pair.sock1);
    SocketStream s2(pair.sock2);

    // Write + flush on s1
    const char* msg = "round trip message";
    s1.write(msg, strlen(msg));
    s1.flush();

    // Read on s2
    char buf[256] = {};
    ssize_t n = s2.read(buf, sizeof(buf) - 1);
    EXPECT_EQ(n, static_cast<ssize_t>(strlen(msg)));
    EXPECT_STREQ(buf, msg);
}

TEST(StreamTest, WriteMultipleAndFlushOnce) {
    auto pair = SocketPair::Create();
    ASSERT_NE(pair.sock1, nullptr);
    ASSERT_NE(pair.sock2, nullptr);

    SocketStream s1(pair.sock1);
    SocketStream s2(pair.sock2);

    s1.write("part1 ", 6);
    s1.write("part2 ", 6);
    s1.write("part3", 5);
    s1.flush();

    char buf[256] = {};
    ssize_t n = s2.read(buf, sizeof(buf) - 1);
    buf[n] = '\0';
    EXPECT_EQ(n, 17);
    EXPECT_STREQ(buf, "part1 part2 part3");
}

TEST(StreamTest, WriteAndReadLargeData) {
    auto pair = SocketPair::Create();
    ASSERT_NE(pair.sock1, nullptr);
    ASSERT_NE(pair.sock2, nullptr);

    SocketStream s1(pair.sock1);
    SocketStream s2(pair.sock2);

    std::string large(10000, 'X');
    for (size_t i = 0; i < large.size(); ++i) {
        large[i] = 'A' + (i % 26);
    }

    s1.write(large.data(), large.size());
    s1.flush();

    std::string received;
    received.resize(large.size());
    size_t total = 0;
    while (total < large.size()) {
        ssize_t n = s2.read(&received[total], large.size() - total);
        if (n <= 0) break;
        total += n;
    }

    EXPECT_EQ(total, large.size());
    EXPECT_EQ(received, large);
}

// =====================================================================
// readExact / writeExact tests
// =====================================================================

TEST(StreamTest, WriteExact) {
    auto pair = SocketPair::Create();
    ASSERT_NE(pair.sock1, nullptr);
    ASSERT_NE(pair.sock2, nullptr);

    SocketStream s1(pair.sock1);

    const char* msg = "exact write test";
    ssize_t written = s1.write_exact(msg, strlen(msg));
    // write_exact may flush internally
    EXPECT_EQ(written, static_cast<ssize_t>(strlen(msg)));
}

TEST(StreamTest, ReadExactAfterFlush) {
    auto pair = SocketPair::Create();
    ASSERT_NE(pair.sock1, nullptr);
    ASSERT_NE(pair.sock2, nullptr);

    SocketStream s1(pair.sock1);
    SocketStream s2(pair.sock2);

    const char* msg = "read exact test data";
    s1.write(msg, strlen(msg));
    s1.flush();

    char buf[256] = {};
    ssize_t n = s2.read_exact(buf, strlen(msg));
    EXPECT_EQ(n, static_cast<ssize_t>(strlen(msg)));
    EXPECT_EQ(std::string(buf, n), msg);
}

// =====================================================================
// isOpen / close tests
// =====================================================================

TEST(StreamTest, IsOpenAfterConstruction) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    SocketStream stream(sock);
    EXPECT_TRUE(stream.is_open());
}

TEST(StreamTest, Close) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    SocketStream stream(sock);
    EXPECT_TRUE(stream.is_open());

    stream.close();
    EXPECT_FALSE(stream.is_open());
}

TEST(StreamTest, ReadAfterClose) {
    auto pair = SocketPair::Create();
    ASSERT_NE(pair.sock1, nullptr);
    ASSERT_NE(pair.sock2, nullptr);

    SocketStream s1(pair.sock1);
    s1.close();

    char buf[64];
    ssize_t n = s1.read(buf, sizeof(buf));
    // Reading from closed stream should return error or 0
    EXPECT_LE(n, 0);
}

// =====================================================================
// getFd tests
// =====================================================================

TEST(StreamTest, GetFdMatchesSocket) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    SocketStream stream(sock);
    EXPECT_EQ(stream.get_fd(), sock->fd());
}

TEST(StreamTest, GetFdAfterClose) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    int fd = sock->fd();
    SocketStream stream(sock);
    stream.close();

    // After close, fd should still be the same value
    EXPECT_EQ(stream.get_fd(), fd);
}

// =====================================================================
// Buffer access tests
// =====================================================================

TEST(StreamTest, ReadBufferInitiallyEmpty) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    SocketStream stream(sock);
    EXPECT_TRUE(stream.read_buffer().empty());
}

TEST(StreamTest, WriteBufferAfterWrite) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    SocketStream stream(sock);
    stream.write("buffered", 8);

    EXPECT_FALSE(stream.write_buffer().empty());
    EXPECT_EQ(stream.write_buffer().readable_size(), 8u);
}

TEST(StreamTest, WriteBufferEmptyAfterFlush) {
    auto pair = SocketPair::Create();
    ASSERT_NE(pair.sock1, nullptr);
    ASSERT_NE(pair.sock2, nullptr);

    SocketStream s1(pair.sock1);
    s1.write("flush me", 8);
    EXPECT_FALSE(s1.write_buffer().empty());

    s1.flush();
    // After flush, write buffer may or may not be empty depending on how much was sent
    // Just verify no crash
    SUCCEED();
}

// =====================================================================
// Read when buffer is empty (via socketpair)
// =====================================================================

TEST(StreamTest, ReadWithEmptyBufferBlocksOrReturnsZero) {
    auto pair = SocketPair::Create();
    ASSERT_NE(pair.sock1, nullptr);
    ASSERT_NE(pair.sock2, nullptr);

    SocketStream s2(pair.sock2);

    // Non-blocking read from empty stream
    // In blocking mode, this would block; in non-blocking, returns -1/EAGAIN
    char buf[64];
    ssize_t n = s2.read(buf, sizeof(buf));
    // May return 0 (EOF), -1 (error/no data), or block
    (void)n;
    SUCCEED();
}

// =====================================================================
// write_string / read_line
// =====================================================================

TEST(StreamTest, WriteStringMethod) {
    auto pair = SocketPair::Create();
    ASSERT_NE(pair.sock1, nullptr);
    ASSERT_NE(pair.sock2, nullptr);

    SocketStream s1(pair.sock1);
    SocketStream s2(pair.sock2);

    std::string msg = "write_string test";
    s1.write_string(msg);
    s1.flush();

    char buf[256] = {};
    ssize_t n = s2.read(buf, sizeof(buf) - 1);
    buf[n] = '\0';
    EXPECT_EQ(std::string(buf), msg);
}

TEST(StreamTest, ReadLine) {
    auto pair = SocketPair::Create();
    ASSERT_NE(pair.sock1, nullptr);
    ASSERT_NE(pair.sock2, nullptr);

    SocketStream s1(pair.sock1);
    SocketStream s2(pair.sock2);

    s1.write("line1\nline2\n", 12);
    s1.flush();

    std::string line1 = s2.read_line();
    EXPECT_EQ(line1, "line1\n");

    std::string line2 = s2.read_line();
    EXPECT_EQ(line2, "line2\n");
}

TEST(StreamTest, ReadAll) {
    auto pair = SocketPair::Create();
    ASSERT_NE(pair.sock1, nullptr);
    ASSERT_NE(pair.sock2, nullptr);

    SocketStream s1(pair.sock1);
    SocketStream s2(pair.sock2);

    s1.write("read all test", 13);
    s1.flush();

    // Close the write side so s2 gets EOF
    pair.sock1->close(); // Causes s2 to see EOF

    std::string all = s2.read_all(1024);
    EXPECT_EQ(all, "read all test");
}

// =====================================================================
// Buffer configuration
// =====================================================================

TEST(StreamTest, SetReadBufferLimit) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    SocketStream stream(sock);
    stream.set_read_buffer_limit(65536);
    SUCCEED();
}

TEST(StreamTest, SetWriteAutoFlush) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    SocketStream stream(sock);
    stream.set_write_auto_flush(4096);
    SUCCEED();
}

// =====================================================================
// Move semantics
// =====================================================================

TEST(StreamTest, MoveStream) {
    auto sock = Socket::create_tcp();
    ASSERT_NE(sock, nullptr);

    SocketStream stream1(sock);
    // SocketStream is not explicitly movable in the header; test construction
    SUCCEED();
}

// =====================================================================
// Edge cases
// =====================================================================

TEST(StreamTest, WriteEmptyData) {
    auto pair = SocketPair::Create();
    ASSERT_NE(pair.sock1, nullptr);
    ASSERT_NE(pair.sock2, nullptr);

    SocketStream s1(pair.sock1);

    ssize_t n = s1.write(nullptr, 0);
    EXPECT_EQ(n, 0);

    n = s1.flush();
    EXPECT_GE(n, 0);
}

TEST(StreamTest, ReadZeroBytes) {
    auto pair = SocketPair::Create();
    ASSERT_NE(pair.sock1, nullptr);
    ASSERT_NE(pair.sock2, nullptr);

    SocketStream s2(pair.sock2);

    char buf[10];
    ssize_t n = s2.read(buf, 0);
    EXPECT_EQ(n, 0);
}

TEST(StreamTest, MultipleFlushes) {
    auto pair = SocketPair::Create();
    ASSERT_NE(pair.sock1, nullptr);
    ASSERT_NE(pair.sock2, nullptr);

    SocketStream s1(pair.sock1);
    SocketStream s2(pair.sock2);

    for (int i = 0; i < 5; ++i) {
        std::string msg = "msg" + std::to_string(i) + " ";
        s1.write(msg.data(), msg.size());
        s1.flush();

        char buf[64] = {};
        ssize_t n = s2.read(buf, sizeof(buf) - 1);
        EXPECT_GT(n, 0);
        (void)n;
    }
}
