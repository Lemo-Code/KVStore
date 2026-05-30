/**
 * @file file_writer.cc
 * @brief 异步文件/标准输出批量写入。
 */
#include "log/file_writer.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

namespace net {

namespace {

bool WriteAll(int fd, const char* data, size_t len) {
  while (len > 0) {
    const ssize_t n = ::write(fd, data, len);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    if (n == 0) {
      return false;
    }
    data += static_cast<size_t>(n);
    len -= static_cast<size_t>(n);
  }
  return true;
}

}  // namespace

FileWriter::FileWriter(size_t buf_capacity, size_t flush_threshold)
    : fd_(-1), buf_(buf_capacity), flush_threshold_(flush_threshold) {}

FileWriter::~FileWriter() {
  close();
}

bool FileWriter::open(const std::string& path) {
  if (path == path_ && fd_ >= 0) {
    return true;
  }
  close();
  path_ = path;
  fd_ = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
  return fd_ >= 0;
}

void FileWriter::append(const char* data, size_t len) {
  if (len == 0) {
    return;
  }
  if (fd_ < 0) {
    return;
  }
  if (!buf_.append(data, len)) {
    flush_buffer();
    if (!buf_.append(data, len)) {
      append_large(data, len);
      return;
    }
  }
  if (buf_.size() >= flush_threshold_) {
    flush_buffer();
  }
}

void FileWriter::append_large(const char* data, size_t len) {
  flush_buffer();
  WriteAll(fd_, data, len);
}

void FileWriter::flush_buffer() {
  if (fd_ < 0 || buf_.empty()) {
    buf_.clear();
    return;
  }
  if (!WriteAll(fd_, buf_.data(), buf_.size())) {
    std::cerr << "net log: write failed: " << path_ << " (" << std::strerror(errno)
              << ")\n";
  }
  buf_.clear();
}

void FileWriter::close() {
  if (fd_ >= 0) {
    flush_buffer();
    ::close(fd_);
    fd_ = -1;
  }
  path_.clear();
}

StdoutWriter::StdoutWriter(size_t buf_capacity, size_t flush_threshold)
    : buf_(buf_capacity), flush_threshold_(flush_threshold) {}

void StdoutWriter::append(const char* data, size_t len) {
  if (len == 0) {
    return;
  }
  if (!buf_.append(data, len)) {
    flush_buffer();
    if (!buf_.append(data, len)) {
      std::cout.write(data, static_cast<std::streamsize>(len));
      std::cout.flush();
      return;
    }
  }
  if (buf_.size() >= flush_threshold_) {
    flush_buffer();
  }
}

void StdoutWriter::flush_buffer() {
  if (buf_.empty()) {
    return;
  }
  std::cout.write(buf_.data(), static_cast<std::streamsize>(buf_.size()));
  std::cout.flush();
  buf_.clear();
}

}  // namespace net
