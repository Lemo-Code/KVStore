#include "buffer/ring_buffer.h"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <sys/uio.h>
#include <unistd.h>

namespace net {

namespace {

size_t clampRegionsTotal(BufferRegion* regions, size_t count,
                         size_t max_total) {
  size_t used = 0;
  size_t kept = 0;
  for (size_t i = 0; i < count; ++i) {
    if (used >= max_total) {
      break;
    }
    const size_t take = std::min(regions[i].len, max_total - used);
    regions[kept].data = regions[i].data;
    regions[kept].len = take;
    used += take;
    if (take > 0) {
      ++kept;
    }
  }
  return kept;
}

}  // namespace

size_t RingBuffer::roundUpPow2(size_t n) {
  if (n < 2) {
    return 2;
  }
  size_t p = 1;
  while (p < n && p < kMaxCapacity) {
    p <<= 1;
  }
  return std::min(p, kMaxCapacity);
}

RingBuffer::RingBuffer(size_t capacity) {
  capacity_ = roundUpPow2(capacity);
  mask_ = capacity_ - 1;
  storage_.reset(new uint8_t[capacity_]);
}

RingBuffer::~RingBuffer() = default;

RingBuffer::RingBuffer(RingBuffer&& other) noexcept
    : head_(other.head_),
      tail_(other.tail_),
      capacity_(other.capacity_),
      mask_(other.mask_),
      storage_(std::move(other.storage_)) {
  other.head_ = 0;
  other.tail_ = 0;
  other.capacity_ = 0;
  other.mask_ = 0;
}

RingBuffer& RingBuffer::operator=(RingBuffer&& other) noexcept {
  if (this != &other) {
    head_ = other.head_;
    tail_ = other.tail_;
    capacity_ = other.capacity_;
    mask_ = other.mask_;
    storage_ = std::move(other.storage_);
    other.head_ = 0;
    other.tail_ = 0;
    other.capacity_ = 0;
    other.mask_ = 0;
  }
  return *this;
}

size_t RingBuffer::readable_regions(BufferRegion* out, size_t max_regions,
                                    size_t skip) const {
  if (!out || max_regions == 0) {
    return 0;
  }
  const size_t avail = readable();
  if (skip >= avail) {
    return 0;
  }
  const size_t total = avail - skip;
  const uint64_t start = head_ + skip;
  const size_t first_off = offset(start);
  const size_t first_len = std::min(total, capacity_ - first_off);

  out[0].data = storage_.get() + first_off;
  out[0].len = first_len;
  size_t count = 1;

  if (first_len < total && max_regions > 1) {
    out[1].data = storage_.get();
    out[1].len = total - first_len;
    ++count;
  }
  return count;
}

size_t RingBuffer::writable_regions(BufferRegion* out, size_t max_regions) const {
  if (!out || max_regions == 0) {
    return 0;
  }
  const size_t total = writable();
  if (total == 0) {
    return 0;
  }
  const size_t tail_off = offset(tail_);
  const size_t first_len = std::min(total, capacity_ - tail_off);

  out[0].data = storage_.get() + tail_off;
  out[0].len = first_len;
  size_t count = 1;

  if (first_len < total && max_regions > 1) {
    out[1].data = storage_.get();
    out[1].len = total - first_len;
    ++count;
  }
  return count;
}

const uint8_t* RingBuffer::peek_ptr(size_t skip) const {
  if (skip >= readable()) {
    return nullptr;
  }
  return storage_.get() + offset(head_ + skip);
}

size_t RingBuffer::peek_contiguous(size_t skip) const {
  if (skip >= readable()) {
    return 0;
  }
  const size_t total = readable() - skip;
  const size_t off = offset(head_ + skip);
  return std::min(total, capacity_ - off);
}

size_t RingBuffer::peek(void* dst, size_t len) const {
  if (!dst || len == 0) {
    return 0;
  }
  const size_t n = std::min(len, readable());
  BufferRegion regions[2];
  const size_t count = readable_regions(regions, 2, 0);
  size_t copied = 0;
  uint8_t* out = static_cast<uint8_t*>(dst);
  for (size_t i = 0; i < count && copied < n; ++i) {
    const size_t take = std::min(regions[i].len, n - copied);
    std::memcpy(out + copied, regions[i].data, take);
    copied += take;
  }
  return copied;
}

size_t RingBuffer::read(void* dst, size_t len) {
  const size_t n = peek(dst, len);
  consume(n);
  return n;
}

size_t RingBuffer::write(const void* src, size_t len) {
  if (!src || len == 0) {
    return 0;
  }
  reserve(len);
  const size_t n = std::min(len, writable());
  BufferRegion regions[2];
  const size_t count = writable_regions(regions, 2);
  size_t written = 0;
  const uint8_t* in = static_cast<const uint8_t*>(src);
  for (size_t i = 0; i < count && written < n; ++i) {
    const size_t take = std::min(regions[i].len, n - written);
    std::memcpy(regions[i].data, in + written, take);
    written += take;
  }
  commit(written);
  return written;
}

void RingBuffer::consume(size_t len) {
  const size_t n = std::min(len, readable());
  head_ += n;
  if (head_ == tail_) {
    head_ = 0;
    tail_ = 0;
  }
}

void RingBuffer::commit(size_t len) {
  const size_t n = std::min(len, writable());
  tail_ += n;
}

void RingBuffer::clear() {
  head_ = 0;
  tail_ = 0;
}

void RingBuffer::compact() {
  const size_t n = readable();
  if (n == 0) {
    head_ = 0;
    tail_ = 0;
    return;
  }
  if (offset(head_) == 0) {
    return;
  }
  std::memmove(storage_.get(), storage_.get() + offset(head_), n);
  head_ = 0;
  tail_ = n;
}

void RingBuffer::resetStorage(size_t new_capacity) {
  capacity_ = new_capacity;
  mask_ = capacity_ - 1;
  std::unique_ptr<uint8_t[]> new_storage(new uint8_t[capacity_]);
  const size_t n = readable();
  if (n > 0) {
    BufferRegion regions[2];
    const size_t count = readable_regions(regions, 2, 0);
    size_t copied = 0;
    for (size_t i = 0; i < count; ++i) {
      std::memcpy(new_storage.get() + copied, regions[i].data, regions[i].len);
      copied += regions[i].len;
    }
  }
  storage_ = std::move(new_storage);
  head_ = 0;
  tail_ = n;
}

void RingBuffer::grow(size_t min_writable) {
  const size_t need = readable() + min_writable;
  size_t new_cap = capacity_;
  while (new_cap < need && new_cap < kMaxCapacity) {
    new_cap <<= 1;
  }
  new_cap = std::min(new_cap, kMaxCapacity);
  if (new_cap <= capacity_) {
    return;
  }
  resetStorage(new_cap);
}

void RingBuffer::reserve(size_t min_writable) {
  if (writable() >= min_writable) {
    return;
  }
  if (readable() + min_writable <= capacity_) {
    compact();
    if (writable() >= min_writable) {
      return;
    }
  }
  grow(min_writable);
}

ssize_t RingBuffer::readFd(int fd, size_t max_bytes, int* saved_errno) {
  if (max_bytes == 0) {
    return 0;
  }
  reserve(std::min(max_bytes, capacity_));

  BufferRegion regions[2];
  size_t count = writable_regions(regions, 2);
  if (count == 0) {
    return 0;
  }
  count = clampRegionsTotal(regions, count, max_bytes);

  struct iovec iov[2];
  for (size_t i = 0; i < count; ++i) {
    iov[i].iov_base = regions[i].data;
    iov[i].iov_len = regions[i].len;
  }

  const ssize_t n = ::readv(fd, iov, static_cast<int>(count));
  if (n > 0) {
    commit(static_cast<size_t>(n));
  } else if (n < 0 && saved_errno) {
    *saved_errno = errno;
  }
  return n;
}

ssize_t RingBuffer::writeFd(int fd, size_t max_bytes, int* saved_errno) {
  if (readable() == 0) {
    return 0;
  }

  BufferRegion regions[2];
  size_t count = readable_regions(regions, 2, 0);
  count = clampRegionsTotal(regions, count, max_bytes);

  struct iovec iov[2];
  for (size_t i = 0; i < count; ++i) {
    iov[i].iov_base = regions[i].data;
    iov[i].iov_len = regions[i].len;
  }

  const ssize_t n = ::writev(fd, iov, static_cast<int>(count));
  if (n > 0) {
    consume(static_cast<size_t>(n));
  } else if (n < 0 && saved_errno) {
    *saved_errno = errno;
  }
  return n;
}

const uint8_t* RingBuffer::find_byte(uint8_t byte, size_t skip) const {
  const size_t avail = readable();
  for (size_t i = skip; i < avail; ++i) {
    const uint8_t* p = peek_ptr(i);
    if (p && *p == byte) {
      return p;
    }
  }
  return nullptr;
}

const uint8_t* RingBuffer::find(const void* pattern, size_t pattern_len,
                                size_t skip) const {
  if (!pattern || pattern_len == 0) {
    return nullptr;
  }
  const size_t avail = readable();
  if (skip + pattern_len > avail) {
    return nullptr;
  }

  const uint8_t* pat = static_cast<const uint8_t*>(pattern);
  const size_t limit = avail - pattern_len + 1;
  for (size_t i = skip; i < limit; ++i) {
    bool match = true;
    for (size_t j = 0; j < pattern_len; ++j) {
      const uint8_t* p = peek_ptr(i + j);
      if (!p || *p != pat[j]) {
        match = false;
        break;
      }
    }
    if (match) {
      return peek_ptr(i);
    }
  }
  return nullptr;
}

}  // namespace net
