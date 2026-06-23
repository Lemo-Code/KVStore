// zero Buffer implementation
//
// A chain-of-blocks buffer designed for efficient network I/O.
// Each block is a fixed 4KB allocation. Data is appended to the
// tail block(s) and consumed from the head. Empty head blocks are
// freed automatically.
//
// Supports:
//   - scatter/gather via iovec (for writev/readv)
//   - zero-copy peek at head data
//   - reserve/commit for direct writes
//   - varint encoding for wire-protocol length prefixes
#include "zero/net/buffer.h"
#include <cstring>
#include <unistd.h>
#include <sys/uio.h>
#include <algorithm>
#include <utility>

namespace zero {

// ============================================================
// Construction / Destruction
// ============================================================
Buffer::Buffer() = default;

Buffer::Buffer(Buffer&& other) noexcept
    : head_(other.head_)
    , tail_(other.tail_)
    , total_size_(other.total_size_) {
    other.head_ = nullptr;
    other.tail_ = nullptr;
    other.total_size_ = 0;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        clear();
        head_ = other.head_;
        tail_ = other.tail_;
        total_size_ = other.total_size_;
        other.head_ = nullptr;
        other.tail_ = nullptr;
        other.total_size_ = 0;
    }
    return *this;
}

Buffer::~Buffer() {
    clear();
}

// ============================================================
// Internal: allocate a new block
// ============================================================
Buffer::Block* Buffer::new_block() {
    auto* block = new Block();
    block->size = 0;
    block->next = nullptr;
    return block;
}

// ============================================================
// Write operations
// ============================================================
void Buffer::append(const void* data, size_t len) {
    if (len == 0 || !data) return;

    const char* src = static_cast<const char*>(data);
    while (len > 0) {
        // Ensure we have a tail block with space
        if (!tail_ || tail_->size >= Block::kBlockSize) {
            Block* block = new_block();
            if (!head_) {
                head_ = tail_ = block;
            } else {
                tail_->next = block;
                tail_ = block;
            }
        }

        size_t space = Block::kBlockSize - tail_->size;
        size_t to_copy = std::min(space, len);
        std::memcpy(tail_->data + tail_->size, src, to_copy);
        tail_->size += to_copy;
        total_size_ += to_copy;
        src += to_copy;
        len -= to_copy;
    }
}

void Buffer::append(const char* str) {
    if (str) {
        append(str, std::strlen(str));
    }
}

void Buffer::append(char c) {
    append(&c, 1);
}

// ============================================================
// Reserve / Commit (for direct writes)
// ============================================================
std::pair<char*, size_t> Buffer::reserve(size_t n) {
    if (!tail_ || tail_->size + n > Block::kBlockSize) {
        Block* block = new_block();
        if (!head_) {
            head_ = tail_ = block;
        } else {
            tail_->next = block;
            tail_ = block;
        }
    }
    size_t available = Block::kBlockSize - tail_->size;
    return {tail_->data + tail_->size, available};
}

void Buffer::commit(size_t n) {
    if (tail_ && n > 0) {
        size_t available = Block::kBlockSize - tail_->size;
        size_t actual = std::min(n, available);
        tail_->size += actual;
        total_size_ += actual;
    }
}

// ============================================================
// Read operations
// ============================================================
size_t Buffer::read(void* buf, size_t len) {
    if (len == 0 || !buf || total_size_ == 0) return 0;

    size_t total = 0;
    char* dst = static_cast<char*>(buf);

    while (len > 0 && head_ && head_->size > 0) {
        size_t to_copy = std::min(head_->size, len);
        std::memcpy(dst, head_->data, to_copy);

        // Shift remaining data down in this block
        if (to_copy < head_->size) {
            std::memmove(head_->data, head_->data + to_copy,
                         head_->size - to_copy);
        }
        head_->size -= to_copy;
        total_size_ -= to_copy;
        dst += to_copy;
        len -= to_copy;
        total += to_copy;

        // Free the block if it's empty and not the only block
        if (head_->size == 0) {
            Block* old = head_;
            head_ = head_->next;
            if (!head_) tail_ = nullptr;
            delete old;
        }
    }
    return total;
}

void Buffer::consume(size_t len) {
    while (len > 0 && head_) {
        size_t to_consume = std::min(head_->size, len);
        if (to_consume < head_->size) {
            std::memmove(head_->data, head_->data + to_consume,
                         head_->size - to_consume);
        }
        head_->size -= to_consume;
        total_size_ -= to_consume;
        len -= to_consume;

        if (head_->size == 0) {
            Block* old = head_;
            head_ = head_->next;
            if (!head_) tail_ = nullptr;
            delete old;
        }
    }
}

const char* Buffer::peek() const noexcept {
    return (head_ && head_->size > 0) ? head_->data : nullptr;
}

// ============================================================
// Scatter/gather I/O
// ============================================================
size_t Buffer::to_iovec(struct iovec* iov, size_t max_iov) const {
    size_t count = 0;
    for (const Block* b = head_; b && count < max_iov; b = b->next) {
        if (b->size > 0) {
            iov[count].iov_base = const_cast<char*>(b->data);
            iov[count].iov_len = b->size;
            ++count;
        }
    }
    return count;
}

ssize_t Buffer::read_from_fd(int fd) {
    constexpr size_t kMaxIov = 16;
    struct iovec iov[kMaxIov];
    Block* blocks[kMaxIov];
    size_t iov_count = 0;

    // Build iovec from blocks that have space
    Block* b = tail_;
    while (b && iov_count < kMaxIov) {
        if (b->size < Block::kBlockSize) {
            iov[iov_count].iov_base = b->data + b->size;
            iov[iov_count].iov_len = Block::kBlockSize - b->size;
            blocks[iov_count] = b;
            ++iov_count;
        }
        b = b->next;
    }

    if (iov_count == 0) return 0;

    ssize_t n = ::readv(fd, iov, static_cast<int>(iov_count));
    if (n > 0) {
        total_size_ += static_cast<size_t>(n);
        // Distribute the read bytes across the iovec blocks
        size_t remaining = static_cast<size_t>(n);
        for (size_t i = 0; i < iov_count && remaining > 0; ++i) {
            size_t added = std::min(remaining,
                                    static_cast<size_t>(iov[i].iov_len));
            blocks[i]->size += added;
            remaining -= added;
        }
    }
    return n;
}

ssize_t Buffer::write_to_fd(int fd) {
    constexpr size_t kMaxIov = 16;
    struct iovec iov[kMaxIov];
    size_t count = to_iovec(iov, kMaxIov);
    if (count == 0) return 0;

    ssize_t n = ::writev(fd, iov, static_cast<int>(count));
    if (n > 0) {
        consume(static_cast<size_t>(n));
    }
    return n;
}

// ============================================================
// Varint encoding
// ============================================================
void Buffer::write_varint(uint64_t value) {
    uint8_t buf[10];
    int len = 0;
    do {
        buf[len] = static_cast<uint8_t>(value & 0x7F);
        value >>= 7;
        if (value != 0) {
            buf[len] |= 0x80;
        }
        ++len;
    } while (value != 0 && len < 10);

    append(buf, static_cast<size_t>(len));
}

bool Buffer::read_varint(uint64_t& value) {
    value = 0;
    int shift = 0;

    for (int i = 0; i < 10; ++i) {
        if (empty()) return false;

        uint8_t byte = static_cast<uint8_t>(*peek());
        consume(1);

        value |= static_cast<uint64_t>(byte & 0x7F) << shift;

        if ((byte & 0x80) == 0) {
            return true;
        }
        shift += 7;
    }
    // Varint too long (>10 bytes)
    return false;
}

// ============================================================
// Clear
// ============================================================
void Buffer::clear() {
    while (head_) {
        Block* next = head_->next;
        delete head_;
        head_ = next;
    }
    tail_ = nullptr;
    total_size_ = 0;
}

} // namespace zero
