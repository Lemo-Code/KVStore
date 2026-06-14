#include "lemo/buffer/chain_buffer.h"

#include "lemo/buffer/ring_buffer.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <sys/uio.h>
#include <unistd.h>
#include <vector>

namespace lemo::buffer {

namespace {

size_t normalizeChunkSize(size_t n) {
  if (n < 256) {
    return 256;
  }
  return std::min(n, ChainBuffer::kMaxChunkSize);
}

}  // namespace

ChainBuffer::ChainBuffer(size_t chunk_size)
    : chunk_size_(normalizeChunkSize(chunk_size)), total_readable_(0) {
  head_.reset(allocChunk(chunk_size_));
  tail_ = head_.get();
}

ChainBuffer::~ChainBuffer() = default;

ChainBuffer::ChainBuffer(ChainBuffer&& other) noexcept
    : chunk_size_(other.chunk_size_),
      total_readable_(other.total_readable_),
      head_(std::move(other.head_)),
      tail_(other.tail_) {
  other.total_readable_ = 0;
  other.tail_ = nullptr;
}

ChainBuffer& ChainBuffer::operator=(ChainBuffer&& other) noexcept {
  if (this != &other) {
    chunk_size_ = other.chunk_size_;
    total_readable_ = other.total_readable_;
    head_ = std::move(other.head_);
    tail_ = other.tail_;
    other.total_readable_ = 0;
    other.tail_ = nullptr;
  }
  return *this;
}

ChainBuffer::Chunk* ChainBuffer::allocChunk(size_t capacity) {
  Chunk* chunk = new Chunk();
  chunk->capacity = capacity;
  chunk->data.reset(new uint8_t[capacity]);
  chunk->read_pos = 0;
  chunk->write_pos = 0;
  chunk->next.reset();
  return chunk;
}

ChainBuffer::Chunk* ChainBuffer::tailChunk() {
  if (!head_) {
    head_.reset(allocChunk(chunk_size_));
    tail_ = head_.get();
  }
  return tail_;
}

const ChainBuffer::Chunk* ChainBuffer::headChunk() const { return head_.get(); }

size_t ChainBuffer::writable() const {
  if (!tail_) {
    return chunk_size_;
  }
  size_t space = 0;
  for (const Chunk* c = tail_; c != nullptr; c = c->next.get()) {
    space += c->capacity - c->write_pos;
  }
  return space;
}

void ChainBuffer::ensureWritable(size_t min_writable) {
  Chunk* tail = tailChunk();
  if (tail->capacity - tail->write_pos >= min_writable) {
    return;
  }
  tail->next.reset(allocChunk(chunk_size_));
  tail_ = tail->next.get();
}

void ChainBuffer::trim() {
  while (head_ && head_->read_pos >= head_->write_pos) {
    if (!head_->next) {
      head_->read_pos = 0;
      head_->write_pos = 0;
      tail_ = head_.get();
      return;
    }
    head_ = std::move(head_->next);
  }
}

size_t ChainBuffer::append(const void* src, size_t len) {
  if (!src || len == 0) {
    return 0;
  }
  const uint8_t* in = static_cast<const uint8_t*>(src);
  size_t written = 0;
  while (written < len) {
    ensureWritable(1);
    Chunk* tail = tail_;
    const size_t space = tail->capacity - tail->write_pos;
    const size_t take = std::min(space, len - written);
    std::memcpy(tail->data.get() + tail->write_pos, in + written, take);
    tail->write_pos += take;
    written += take;
  }
  total_readable_ += written;
  return written;
}

size_t ChainBuffer::readable_regions(BufferRegion* out, size_t max_regions,
                                     size_t skip) const {
  if (!out || max_regions == 0 || skip >= total_readable_) {
    return 0;
  }
  size_t count = 0;
  size_t skipped = 0;
  for (const Chunk* c = headChunk(); c && count < max_regions; c = c->next.get()) {
    const size_t chunk_len = c->write_pos - c->read_pos;
    if (chunk_len == 0) {
      continue;
    }
    if (skipped + chunk_len <= skip) {
      skipped += chunk_len;
      continue;
    }
    size_t off = c->read_pos;
    size_t len = chunk_len;
    if (skipped < skip) {
      off += skip - skipped;
      len -= skip - skipped;
      skipped = skip;
    }
    out[count].data = c->data.get() + off;
    out[count].len = len;
    ++count;
  }
  return count;
}

const uint8_t* ChainBuffer::peek_ptr(size_t skip) const {
  if (skip >= total_readable_) {
    return nullptr;
  }
  size_t passed = 0;
  for (const Chunk* c = headChunk(); c; c = c->next.get()) {
    const size_t chunk_len = c->write_pos - c->read_pos;
    if (passed + chunk_len <= skip) {
      passed += chunk_len;
      continue;
    }
    return c->data.get() + c->read_pos + (skip - passed);
  }
  return nullptr;
}

size_t ChainBuffer::peek_contiguous(size_t skip) const {
  if (skip >= total_readable_) {
    return 0;
  }
  size_t passed = 0;
  for (const Chunk* c = headChunk(); c; c = c->next.get()) {
    const size_t chunk_len = c->write_pos - c->read_pos;
    if (passed + chunk_len <= skip) {
      passed += chunk_len;
      continue;
    }
    return chunk_len - (skip - passed);
  }
  return 0;
}

size_t ChainBuffer::peek(void* dst, size_t len) const {
  if (!dst || len == 0) {
    return 0;
  }
  const size_t n = std::min(len, total_readable_);
  BufferRegion regions[kMaxRegions];
  const size_t count = readable_regions(regions, kMaxRegions, 0);
  size_t copied = 0;
  uint8_t* out = static_cast<uint8_t*>(dst);
  for (size_t i = 0; i < count && copied < n; ++i) {
    const size_t take = std::min(regions[i].len, n - copied);
    std::memcpy(out + copied, regions[i].data, take);
    copied += take;
  }
  return copied;
}

void ChainBuffer::consume(size_t len) {
  const size_t n = std::min(len, total_readable_);
  size_t left = n;
  while (left > 0 && head_) {
    Chunk* c = head_.get();
    const size_t chunk_len = c->write_pos - c->read_pos;
    if (chunk_len <= left) {
      left -= chunk_len;
      c->read_pos = c->write_pos;
      trim();
      if (left == 0) {
        break;
      }
      continue;
    }
    c->read_pos += left;
    left = 0;
  }
  total_readable_ -= n;
  trim();
}

size_t ChainBuffer::read(void* dst, size_t len) {
  const size_t n = peek(dst, len);
  consume(n);
  return n;
}

void ChainBuffer::clear() {
  head_.reset(allocChunk(chunk_size_));
  tail_ = head_.get();
  total_readable_ = 0;
}

void ChainBuffer::reserve(size_t min_writable) {
  ensureWritable(min_writable);
}

ssize_t ChainBuffer::readFd(int fd, size_t max_bytes, int* saved_errno) {
  if (max_bytes == 0) {
    return 0;
  }

  std::vector<Chunk*> chunks;
  std::vector<iovec> iov;
  chunks.reserve(8);
  iov.reserve(8);

  size_t wanted = max_bytes;
  Chunk* c = tailChunk();
  while (c && wanted > 0) {
    const size_t space = c->capacity - c->write_pos;
    if (space > 0) {
      iovec v;
      v.iov_base = c->data.get() + c->write_pos;
      v.iov_len = std::min(space, wanted);
      iov.push_back(v);
      chunks.push_back(c);
      wanted -= v.iov_len;
    }
    if (wanted > 0) {
      if (!c->next) {
        c->next.reset(allocChunk(chunk_size_));
        tail_ = c->next.get();
      }
      c = c->next.get();
    }
  }

  if (iov.empty()) {
    return 0;
  }

  const ssize_t n = ::readv(fd, iov.data(), static_cast<int>(iov.size()));
  if (n > 0) {
    size_t left = static_cast<size_t>(n);
    for (size_t i = 0; i < chunks.size() && left > 0; ++i) {
      const size_t cap = static_cast<size_t>(iov[i].iov_len);
      const size_t take = std::min(cap, left);
      chunks[i]->write_pos += take;
      left -= take;
    }
    total_readable_ += static_cast<size_t>(n);
  } else if (n < 0 && saved_errno) {
    *saved_errno = errno;
  }
  return n;
}

ssize_t ChainBuffer::writeFd(int fd, size_t max_bytes, int* saved_errno) {
  if (total_readable_ == 0) {
    return 0;
  }

  BufferRegion regions[kMaxRegions];
  size_t count = readable_regions(regions, kMaxRegions, 0);
  size_t total = 0;
  for (size_t i = 0; i < count; ++i) {
    total += regions[i].len;
  }
  const size_t limit = std::min(max_bytes, total);

  std::vector<iovec> iov;
  iov.reserve(count);
  size_t packed = 0;
  for (size_t i = 0; i < count && packed < limit; ++i) {
    iovec v;
    v.iov_base = regions[i].data;
    v.iov_len = std::min(regions[i].len, limit - packed);
    iov.push_back(v);
    packed += v.iov_len;
  }

  const ssize_t n = ::writev(fd, iov.data(), static_cast<int>(iov.size()));
  if (n > 0) {
    consume(static_cast<size_t>(n));
  } else if (n < 0 && saved_errno) {
    *saved_errno = errno;
  }
  return n;
}

const uint8_t* ChainBuffer::find_byte(uint8_t byte, size_t skip) const {
  const size_t avail = total_readable_;
  for (size_t i = skip; i < avail; ++i) {
    const uint8_t* p = peek_ptr(i);
    if (p && *p == byte) {
      return p;
    }
  }
  return nullptr;
}

const uint8_t* ChainBuffer::find(const void* pattern, size_t pattern_len,
                                   size_t skip) const {
  if (!pattern || pattern_len == 0) {
    return nullptr;
  }
  const size_t avail = total_readable_;
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

}  // namespace lemo::buffer
