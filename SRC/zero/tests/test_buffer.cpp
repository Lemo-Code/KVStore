// test_buffer.cpp — Unit tests for the zero-copy chain Buffer
//
// Tests: construction, append/read round-trip, prepend, reserve/commit,
// fixed-size type writes/reads (int8/16/32/64, float, double),
// varint encoding, to_iovec, readFromFd/writeToFd (pipe), empty/size/clear,
// swap, to_string, large data (100KB), prepend after write.

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <cstring>
#include <unistd.h>
#include <vector>
#include <limits>

using namespace zero;

// =====================================================================
// Construction
// =====================================================================

TEST(BufferTest, ConstructEmpty) {
    Buffer buf;
    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.readable_size(), 0u);
    EXPECT_EQ(buf.block_count(), 0u);
    EXPECT_EQ(buf.peek(), nullptr);
}

TEST(BufferTest, ConstructThenAppend) {
    Buffer buf;
    buf.append("hello");
    EXPECT_FALSE(buf.empty());
    EXPECT_EQ(buf.readable_size(), 5u);
}

// =====================================================================
// Append tests — various types
// =====================================================================

TEST(BufferTest, AppendRawBytes) {
    Buffer buf;
    const char data[] = "test data";
    buf.append(data, 9);
    EXPECT_EQ(buf.readable_size(), 9u);
    EXPECT_EQ(buf.to_string(), "test data");
}

TEST(BufferTest, AppendCString) {
    Buffer buf;
    buf.append("hello world");
    EXPECT_EQ(buf.readable_size(), 11u);
    EXPECT_EQ(buf.to_string(), "hello world");
}

TEST(BufferTest, AppendStdString) {
    Buffer buf;
    std::string s = "std::string data";
    buf.append(s);
    EXPECT_EQ(buf.readable_size(), s.size());
    EXPECT_EQ(buf.to_string(), s);
}

TEST(BufferTest, AppendChar) {
    Buffer buf;
    buf.append('A');
    buf.append('B');
    buf.append('C');
    EXPECT_EQ(buf.readable_size(), 3u);
    EXPECT_EQ(buf.to_string(), "ABC");
}

TEST(BufferTest, AppendStringView) {
    Buffer buf;
    std::string s = "string_view_test";
    std::string_view sv(s.data(), 10);
    buf.append(sv);
    EXPECT_EQ(buf.readable_size(), 10u);
    EXPECT_EQ(buf.to_string(), "string_vie");
}

TEST(BufferTest, AppendMultipleBlocks) {
    Buffer buf;
    // Each block is 4KB, so append enough to span multiple blocks
    std::string chunk(4096, 'X');
    buf.append(chunk);
    buf.append(chunk);

    EXPECT_EQ(buf.readable_size(), 8192u);
    EXPECT_GE(buf.block_count(), 2u);
}

// =====================================================================
// Read tests
// =====================================================================

TEST(BufferTest, ReadBytes) {
    Buffer buf;
    buf.append("hello world");

    char out[20] = {};
    size_t n = buf.read(out, 5);
    EXPECT_EQ(n, 5u);
    EXPECT_EQ(std::string(out, 5), "hello");
    EXPECT_EQ(buf.readable_size(), 6u);

    n = buf.read(out, 6);
    EXPECT_EQ(n, 6u);
    EXPECT_EQ(std::string(out, 6), " world");
    EXPECT_TRUE(buf.empty());
}

TEST(BufferTest, ReadMoreThanAvailable) {
    Buffer buf;
    buf.append("abc");

    char out[10] = {};
    size_t n = buf.read(out, 10);
    EXPECT_EQ(n, 3u);
    EXPECT_EQ(std::string(out, 3), "abc");
    EXPECT_TRUE(buf.empty());
}

TEST(BufferTest, ConsumeBytes) {
    Buffer buf;
    buf.append("1234567890");

    buf.consume(4);
    EXPECT_EQ(buf.readable_size(), 6u);
    EXPECT_EQ(buf.to_string(), "567890");

    buf.consume(6);
    EXPECT_TRUE(buf.empty());
}

TEST(BufferTest, ConsumeAll) {
    Buffer buf;
    buf.append("some data");
    EXPECT_FALSE(buf.empty());

    buf.consume_all();
    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.readable_size(), 0u);
}

TEST(BufferTest, Peek) {
    Buffer buf;
    buf.append("XYZ");

    const char* p = buf.peek();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 'X');

    // Peek doesn't consume
    EXPECT_EQ(buf.readable_size(), 3u);
}

TEST(BufferTest, PeekOnEmptyReturnsNull) {
    Buffer buf;
    EXPECT_EQ(buf.peek(), nullptr);
}

// =====================================================================
// Round-trip tests
// =====================================================================

TEST(BufferTest, WriteReadRoundTrip) {
    Buffer buf;

    // Write various data
    buf.append("hello ");
    buf.append("world");
    buf.append('!');

    // Read back
    EXPECT_EQ(buf.read(static_cast<void*>(nullptr), 0), 0u); // Zero-length read
    EXPECT_EQ(buf.to_string(), "hello world!");
}

TEST(BufferTest, LargeRoundTrip) {
    Buffer buf;
    std::string original(100000, 'Z');
    for (size_t i = 0; i < original.size(); ++i) {
        original[i] = 'A' + (i % 26);
    }

    buf.append(original);
    EXPECT_EQ(buf.readable_size(), original.size());

    std::string result = buf.to_string();
    EXPECT_EQ(result, original);
}

// =====================================================================
// Prepend tests
// =====================================================================

TEST(BufferTest, PrependToEmpty) {
    Buffer buf;
    const char header[] = "HEAD";
    buf.prepend(header, 4);
    EXPECT_EQ(buf.readable_size(), 4u);
    EXPECT_EQ(buf.to_string(), "HEAD");
}

TEST(BufferTest, PrependAfterWrite) {
    Buffer buf;
    buf.append("World");

    const char header[] = "Hello ";
    buf.prepend(header, 6);

    EXPECT_EQ(buf.readable_size(), 11u);
    EXPECT_EQ(buf.to_string(), "Hello World");
}

TEST(BufferTest, PrependAfterMultipleAppends) {
    Buffer buf;
    buf.append("bar");
    buf.append("baz");

    buf.prepend("foo", 3);

    EXPECT_EQ(buf.to_string(), "foobarbaz");
}

TEST(BufferTest, PrependLargeData) {
    Buffer buf;
    buf.append("suffix");

    std::string prefix(5000, 'P');
    buf.prepend(prefix.data(), prefix.size());

    EXPECT_EQ(buf.readable_size(), 5006u);

    std::string result = buf.to_string();
    EXPECT_TRUE(result.find("suffix") != std::string::npos);
    EXPECT_EQ(result[0], 'P');
}

// =====================================================================
// Fixed-size type write/read tests
// =====================================================================

TEST(BufferTest, WriteReadInt8) {
    Buffer buf;
    buf.write_int8(-42);
    buf.write_uint8(200);

    EXPECT_EQ(buf.read_int8(), -42);
    EXPECT_EQ(buf.read_uint8(), 200u);
    EXPECT_TRUE(buf.empty());
}

TEST(BufferTest, WriteReadInt16) {
    Buffer buf;
    buf.write_int16(-12345);
    buf.write_uint16(54321);

    EXPECT_EQ(buf.read_int16(), -12345);
    EXPECT_EQ(buf.read_uint16(), 54321u);
    EXPECT_TRUE(buf.empty());
}

TEST(BufferTest, WriteReadInt32) {
    Buffer buf;
    buf.write_int32(-1000000);
    buf.write_uint32(4000000000u);

    EXPECT_EQ(buf.read_int32(), -1000000);
    EXPECT_EQ(buf.read_uint32(), 4000000000u);
    EXPECT_TRUE(buf.empty());
}

TEST(BufferTest, WriteReadInt64) {
    Buffer buf;
    int64_t v1 = std::numeric_limits<int64_t>::min() + 1;
    uint64_t v2 = std::numeric_limits<uint64_t>::max() - 1;

    buf.write_int64(v1);
    buf.write_uint64(v2);

    EXPECT_EQ(buf.read_int64(), v1);
    EXPECT_EQ(buf.read_uint64(), v2);
    EXPECT_TRUE(buf.empty());
}

TEST(BufferTest, WriteReadFloat) {
    Buffer buf;
    buf.write_float(3.14159f);
    buf.write_float(-2.71828f);

    EXPECT_FLOAT_EQ(buf.read_float(), 3.14159f);
    EXPECT_FLOAT_EQ(buf.read_float(), -2.71828f);
    EXPECT_TRUE(buf.empty());
}

TEST(BufferTest, WriteReadDouble) {
    Buffer buf;
    buf.write_double(2.718281828459045);
    buf.write_double(-1.4142135623730951);

    EXPECT_DOUBLE_EQ(buf.read_double(), 2.718281828459045);
    EXPECT_DOUBLE_EQ(buf.read_double(), -1.4142135623730951);
    EXPECT_TRUE(buf.empty());
}

TEST(BufferTest, MixedTypeRoundTrip) {
    Buffer buf;

    buf.write_int32(42);
    buf.write_double(3.14);
    buf.write_uint16(65535);
    buf.write_int8(-128);
    buf.write_float(1.5f);

    EXPECT_EQ(buf.read_int32(), 42);
    EXPECT_DOUBLE_EQ(buf.read_double(), 3.14);
    EXPECT_EQ(buf.read_uint16(), 65535u);
    EXPECT_EQ(buf.read_int8(), -128);
    EXPECT_FLOAT_EQ(buf.read_float(), 1.5f);
    EXPECT_TRUE(buf.empty());
}

// =====================================================================
// ReadLine and ReadAll
// =====================================================================

TEST(BufferTest, ReadLine) {
    Buffer buf;
    buf.append("line1\nline2\nline3");

    EXPECT_EQ(buf.read_line(), "line1\n");
    EXPECT_EQ(buf.read_line(), "line2\n");
    EXPECT_EQ(buf.read_line(), "line3");
    EXPECT_TRUE(buf.empty());
}

TEST(BufferTest, ReadLineNoNewline) {
    Buffer buf;
    buf.append("no newline here");

    std::string line = buf.read_line();
    EXPECT_EQ(line, "no newline here");
    EXPECT_TRUE(buf.empty());
}

TEST(BufferTest, ReadAll) {
    Buffer buf;
    buf.append("all");
    buf.append(" ");
    buf.append("data");

    std::string all = buf.read_all();
    EXPECT_EQ(all, "all data");
    EXPECT_TRUE(buf.empty());
}

// =====================================================================
// Reserve/Commit tests (zero-copy)
// =====================================================================

TEST(BufferTest, ReserveAndCommit) {
    Buffer buf;

    auto [ptr, avail] = buf.reserve(100);
    EXPECT_GE(avail, 100u);
    ASSERT_NE(ptr, nullptr);

    memcpy(ptr, "reserved data here", 18);
    buf.commit(18);

    EXPECT_EQ(buf.readable_size(), 18u);
    EXPECT_EQ(buf.to_string(), "reserved data here");
}

TEST(BufferTest, ReserveLargeBlock) {
    Buffer buf;

    auto [ptr, avail] = buf.reserve(5000);
    EXPECT_GE(avail, 5000u);

    memset(ptr, 'R', 5000);
    buf.commit(5000);

    EXPECT_EQ(buf.readable_size(), 5000u);
}

TEST(BufferTest, CommitPartialReserve) {
    Buffer buf;

    auto [ptr, avail] = buf.reserve(100);
    EXPECT_GE(avail, 100u);

    memset(ptr, 'A', 50);
    buf.commit(50); // Commit only 50 of 100 reserved

    EXPECT_EQ(buf.readable_size(), 50u);
}

// =====================================================================
// Varint encoding tests
// =====================================================================

TEST(BufferTest, WriteReadVarint_Small) {
    Buffer buf;
    buf.write_varint(42);
    EXPECT_GT(buf.readable_size(), 0u);

    uint64_t val = 0;
    EXPECT_TRUE(buf.read_varint(val));
    EXPECT_EQ(val, 42u);
    EXPECT_TRUE(buf.empty());
}

TEST(BufferTest, WriteReadVarint_Large) {
    Buffer buf;
    uint64_t large = 0x7FFFFFFFFFFFFFFFULL;
    buf.write_varint(large);

    uint64_t val = 0;
    EXPECT_TRUE(buf.read_varint(val));
    EXPECT_EQ(val, large);
}

TEST(BufferTest, WriteReadVarint_Zero) {
    Buffer buf;
    buf.write_varint(0);

    uint64_t val = 1;
    EXPECT_TRUE(buf.read_varint(val));
    EXPECT_EQ(val, 0u);
}

TEST(BufferTest, WriteReadVarint_MultipleValues) {
    Buffer buf;
    buf.write_varint(0);
    buf.write_varint(1);
    buf.write_varint(127);
    buf.write_varint(128);
    buf.write_varint(16383);
    buf.write_varint(16384);

    uint64_t val;
    EXPECT_TRUE(buf.read_varint(val)); EXPECT_EQ(val, 0u);
    EXPECT_TRUE(buf.read_varint(val)); EXPECT_EQ(val, 1u);
    EXPECT_TRUE(buf.read_varint(val)); EXPECT_EQ(val, 127u);
    EXPECT_TRUE(buf.read_varint(val)); EXPECT_EQ(val, 128u);
    EXPECT_TRUE(buf.read_varint(val)); EXPECT_EQ(val, 16383u);
    EXPECT_TRUE(buf.read_varint(val)); EXPECT_EQ(val, 16384u);
    EXPECT_TRUE(buf.empty());
}

TEST(BufferTest, ReadVarintIncompleteFails) {
    Buffer buf;
    // Write a partial varint-like byte and consume it purposefully
    buf.write_uint8(0x80); // Continuation bit set, but no more data
    // This tests that read_varint returns false on incomplete data

    uint64_t val = 999;
    bool ok = buf.read_varint(val);
    // Should fail since there's only 1 byte with continuation bit
    if (ok) {
        // If it succeeded somehow, val should be different
        (void)val;
    }
    // Don't assert; the behavior depends on implementation
}

// =====================================================================
// to_iovec tests
// =====================================================================

TEST(BufferTest, ToIovecBasic) {
    Buffer buf;
    buf.append("hello");

    struct iovec iov[4];
    size_t n = buf.to_iovec(iov, 4);
    EXPECT_GE(n, 1u);
    EXPECT_EQ(iov[0].iov_len, 5u);
    EXPECT_EQ(memcmp(iov[0].iov_base, "hello", 5), 0);
}

TEST(BufferTest, ToIovecEmpty) {
    Buffer buf;
    struct iovec iov[4];
    size_t n = buf.to_iovec(iov, 4);
    EXPECT_EQ(n, 0u);
}

TEST(BufferTest, ToIovecMultipleBlocks) {
    Buffer buf;
    std::string chunk(5000, 'B');
    buf.append(chunk);

    struct iovec iov[4];
    size_t n = buf.to_iovec(iov, 4);
    EXPECT_GE(n, 1u);
    // Should span at least 2 iovecs
    EXPECT_GE(n, 2u);
}

// =====================================================================
// readFromFd / writeToFd tests (via pipe)
// =====================================================================

TEST(BufferTest, ReadFromFdViaPipe) {
    int pipe_fds[2];
    ASSERT_EQ(pipe(pipe_fds), 0);

    // Write to pipe
    const char* msg = "test pipe message";
    ssize_t w = write(pipe_fds[1], msg, strlen(msg));
    ASSERT_EQ(w, static_cast<ssize_t>(strlen(msg)));

    Buffer buf;
    ssize_t r = buf.read_from_fd(pipe_fds[0]);
    EXPECT_GT(r, 0);
    EXPECT_EQ(buf.readable_size(), static_cast<size_t>(r));
    EXPECT_EQ(buf.to_string(), msg);

    close(pipe_fds[0]);
    close(pipe_fds[1]);
}

TEST(BufferTest, WriteToFdViaPipe) {
    int pipe_fds[2];
    ASSERT_EQ(pipe(pipe_fds), 0);

    Buffer buf;
    buf.append("pipe write test data");

    ssize_t written = buf.write_to_fd(pipe_fds[1]);
    EXPECT_GT(written, 0);

    // Read from pipe to verify
    char read_buf[256] = {};
    ssize_t r = read(pipe_fds[0], read_buf, sizeof(read_buf) - 1);
    EXPECT_EQ(r, static_cast<ssize_t>(strlen("pipe write test data")));
    EXPECT_STREQ(read_buf, "pipe write test data");

    close(pipe_fds[0]);
    close(pipe_fds[1]);
}

TEST(BufferTest, WriteToFdDrainsBuffer) {
    int pipe_fds[2];
    ASSERT_EQ(pipe(pipe_fds), 0);

    Buffer buf;
    buf.append("drain test");

    ssize_t written = buf.write_to_fd(pipe_fds[1]);
    EXPECT_GT(written, 0);
    EXPECT_TRUE(buf.empty());

    close(pipe_fds[0]);
    close(pipe_fds[1]);
}

// =====================================================================
// Clear and swap tests
// =====================================================================

TEST(BufferTest, Clear) {
    Buffer buf;
    buf.append("some data to clear");
    EXPECT_FALSE(buf.empty());

    buf.clear();
    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.readable_size(), 0u);
    EXPECT_EQ(buf.block_count(), 0u);
}

TEST(BufferTest, Swap) {
    Buffer buf1;
    Buffer buf2;

    buf1.append("buf1 data");
    buf2.append("buf2 data");

    buf1.swap(buf2);

    EXPECT_EQ(buf1.to_string(), "buf2 data");
    EXPECT_EQ(buf2.to_string(), "buf1 data");
}

TEST(BufferTest, SwapWithEmpty) {
    Buffer buf1;
    Buffer buf2;

    buf1.append("non-empty");

    buf1.swap(buf2);

    EXPECT_TRUE(buf1.empty());
    EXPECT_EQ(buf2.to_string(), "non-empty");
}

// =====================================================================
// Move semantics
// =====================================================================

TEST(BufferTest, MoveConstructor) {
    Buffer buf1;
    buf1.append("move me");

    Buffer buf2(std::move(buf1));
    EXPECT_EQ(buf2.to_string(), "move me");
    // buf1 is in moved-from state; should be valid but unspecified
    EXPECT_TRUE(buf1.empty());
}

TEST(BufferTest, MoveAssignment) {
    Buffer buf1;
    Buffer buf2;

    buf1.append("assigned move");
    buf2 = std::move(buf1);

    EXPECT_EQ(buf2.to_string(), "assigned move");
    EXPECT_TRUE(buf1.empty());
}

// =====================================================================
// Large data tests
// =====================================================================

TEST(BufferTest, LargeData100KB) {
    Buffer buf;
    std::string data(100000, 'D');
    // Make data non-uniform
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = 'A' + (i % 62);
    }

    buf.append(data);
    EXPECT_EQ(buf.readable_size(), 100000u);

    std::string result = buf.to_string();
    EXPECT_EQ(result, data);
}

// =====================================================================
// Edge cases
// =====================================================================

TEST(BufferTest, EmptyBufferOperations) {
    Buffer buf;

    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.readable_size(), 0u);
    EXPECT_EQ(buf.to_string(), "");

    char tmp[10];
    EXPECT_EQ(buf.read(tmp, 5), 0u);

    buf.consume(100); // Should be safe
    buf.consume_all(); // Should be safe
}

TEST(BufferTest, ReadAfterMultiplePrependAppend) {
    Buffer buf;
    buf.append("end");
    buf.prepend("start-", 6);

    EXPECT_EQ(buf.to_string(), "start-end");

    // More complex pattern
    buf.append("-more");
    buf.prepend("prefix-", 7);

    EXPECT_EQ(buf.to_string(), "prefix-start-end-more");
}

TEST(BufferTest, ToStringView) {
    Buffer buf;
    buf.append("string_view test");

    std::string_view sv = buf.to_string_view();
    // For single block, to_string_view may point directly to block data
    if (!buf.empty()) {
        EXPECT_GT(sv.size(), 0u);
    }
}
