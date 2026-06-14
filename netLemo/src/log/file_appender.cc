#include "lemo/log/file_appender.h"

#include "lemo/log/log_paths.h"

#include <ctime>

namespace lemo {
namespace log {

FileAppender::FileAppender(const std::string& path)
    : path_(path), last_reopen_sec_(0) {
  stream_.open(path_.c_str(), std::ios::app);
}

FileAppender::ptr FileAppender::ForLogger(const std::string& logger_name,
                                          const std::string& base_path) {
  return ptr(new FileAppender(ResolveLoggerFilePath(logger_name, base_path)));
}

void FileAppender::Append(const LogRecord& record) {
  if (!PassesThreshold(record.level)) return;
  const std::string line = Format(record);
  std::lock_guard<std::mutex> lock(mutex_);
  const uint64_t now = static_cast<uint64_t>(time(0));
  if (now != last_reopen_sec_) {
    if (stream_) stream_.close();
    stream_.open(path_.c_str(), std::ios::app);
    last_reopen_sec_ = now;
  }
  if (stream_) {
    stream_ << line;
  }
}

void FileAppender::Flush() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (stream_) stream_.flush();
}

const char* FileAppender::Type() const { return "file"; }

bool FileAppender::Reopen() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (stream_) stream_.close();
  stream_.open(path_.c_str(), std::ios::app);
  return static_cast<bool>(stream_);
}

}  // namespace log
}  // namespace lemo
