// zero Buffer — zero-copy chain buffer for network I/O
//
// A chain of fixed-size (4KB) memory blocks for scatter/gather I/O.
// Supports:
//   - Zero-copy read/write via reserve/commit pattern
//   - Varint encoding for wire-protocol length prefixes
//   - iovec construction for writev() / readv()
//   - Direct readFromFd / writeToFd using scatter/gather
//
// Memory is allocated in 4KB blocks. Blocks are linked together to form
// an arbitrarily long byte stream. Old blocks are freed when consumed.
//
// Move-only (copy is expensive and usually a bug).
#pragma once

#include <cstdint>
#include <cstddef>
#include <sys/uio.h>
#include <utility>
#include <string>
#include <string_view>

namespace zero {

class Buffer {
public:
    // ============================================================
    // Block structure
    // ============================================================

    struct Block {
        static constexpr size_t kBlockSize = 4096;
        char data[kBlockSize];
        size_t size = 0;         // Amount of data in this block
        Block* next = nullptr;
    };

    // ============================================================
    // Construction
    // ============================================================

    Buffer();
    ~Buffer();

    // Move-only
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    // ============================================================
    // Writing (appending data)
    // ============================================================

    // Append raw bytes to the buffer
    void append(const void* data, size_t len);

    // Append a C-string (excluding null terminator)
    void append(const char* str);

    // Append a std::string
    void append(const std::string& str);

    // Append a single character
    void append(char c);

    // Append a string_view
    void append(std::string_view sv);

    // Write integers in network byte order
    void write_int8(int8_t v);
    void write_uint8(uint8_t v);
    void write_int16(int16_t v);
    void write_uint16(uint16_t v);
    void write_int32(int32_t v);
    void write_uint32(uint32_t v);
    void write_int64(int64_t v);
    void write_uint64(uint64_t v);

    // Write floating point values
    void write_float(float v);
    void write_double(double v);

    // Prepend data to the front of the buffer (for headers)
    void prepend(const void* data, size_t len);

    // ============================================================
    // Reading (consuming data)
    // ============================================================

    // Read up to `len` bytes into `buf`. Returns actual bytes read.
    size_t read(void* buf, size_t len);

    // Consume (discard) `len` bytes from the front of the buffer.
    void consume(size_t len);

    // Consume all readable bytes.
    void consume_all();

    // Peek at the next byte without consuming. Returns nullptr if empty.
    const char* peek() const noexcept;

    // Read integers in network byte order
    int8_t   read_int8();
    uint8_t  read_uint8();
    int16_t  read_int16();
    uint16_t read_uint16();
    int32_t  read_int32();
    uint32_t read_uint32();
    int64_t  read_int64();
    uint64_t read_uint64();

    // Read floating point
    float  read_float();
    double read_double();

    // Read a line (up to \n, including the \n). Returns the line.
    std::string read_line();

    // Read all remaining data as a string
    std::string read_all();

    // ============================================================
    // Observers
    // ============================================================

    // Total readable bytes in the buffer
    size_t readable_size() const noexcept { return total_size_; }

    // Whether the buffer is empty
    bool empty() const noexcept { return total_size_ == 0; }

    // Number of blocks in the chain
    size_t block_count() const noexcept { return block_count_; }

    // ============================================================
    // Zero-copy reserve/commit
    // ============================================================

    // Reserve writable space at the tail.
    // Returns {pointer to writable space, available bytes}.
    // The pointer is valid until the next append/reserve call.
    std::pair<char*, size_t> reserve(size_t n);

    // Commit `n` bytes as written after a reserve() call.
    void commit(size_t n);

    // ============================================================
    // I/O operations
    // ============================================================

    // Build an iovec array for writev().
    // Returns the number of iovecs used (<= max_iov).
    size_t to_iovec(struct iovec* iov, size_t max_iov) const;

    // Read from fd into the buffer using readv().
    // Fills multiple blocks for efficient scatter/gather.
    // Returns bytes read, or -1 on error.
    ssize_t read_from_fd(int fd);

    // Write to fd from the buffer using writev().
    // Drains readable bytes and advances the read pointer.
    // Returns bytes written, or -1 on error.
    ssize_t write_to_fd(int fd);

    // ============================================================
    // Varint encoding (protobuf-style length prefixes)
    // ============================================================

    // Write a uint64_t as a variable-length integer.
    void write_varint(uint64_t value);

    // Read a varint. Returns true if a complete varint was decoded.
    bool read_varint(uint64_t& value);

    // ============================================================
    // Utility
    // ============================================================

    // Swap contents with another Buffer (constant time)
    void swap(Buffer& other) noexcept;

    // Convert all readable bytes to a string (copies data)
    std::string to_string() const;

    // Convert all readable bytes to a string_view (may be fragmented,
    // use to_string() for a contiguous copy)
    std::string_view to_string_view() const;

    // Clear all data and free blocks
    void clear();

private:
    // Allocate a new block and append to the chain
    Block* new_block();

    // Free blocks from head up to (but not including) the block
    // containing the read position.
    void free_read_blocks();

    // Check if any blocks can be freed (all data consumed from them)
    void compact();

    Block* head_ = nullptr;
    Block* tail_ = nullptr;  // For O(1) append to end
    size_t total_size_ = 0;  // Total readable bytes
    size_t block_count_ = 0; // Number of blocks in the chain
};

} // namespace zero
