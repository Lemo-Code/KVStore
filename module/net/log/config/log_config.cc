/**
 * @file log_config.cc
 */
#include "log/config/log_config.h"

namespace net {

namespace {

AsyncLogSettings MakeDefaultAsync() {
  AsyncLogSettings s;
  s.flush_interval_ms = static_cast<uint32_t>(NET_LOG_ASYNC_FLUSH_MS);
  s.buf_bytes = static_cast<size_t>(NET_LOG_ASYNC_BUF_BYTES);
  s.flush_threshold = static_cast<size_t>(NET_LOG_ASYNC_FLUSH_THRESHOLD);
  s.soft_cap = static_cast<size_t>(NET_LOG_ASYNC_SOFT_CAP);
  s.degrade_mode = NET_LOG_DEGRADE_MODE;
  s.sample_rate = static_cast<uint32_t>(NET_LOG_DEGRADE_SAMPLE_RATE);
  if (s.sample_rate == 0) {
    s.sample_rate = 1;
  }
  return s;
}

LogModuleSettings MakeDefaults() {
  LogModuleSettings s;
  s.default_level = NET_LOG_DEFAULT_LEVEL;
  s.default_pattern = NET_LOG_DEFAULT_PATTERN;
  s.async = MakeDefaultAsync();
  s.file = FileLogSettings();
  return s;
}

}  // namespace

FileLogSettings::FileLogSettings()
    : reopen_sec(NET_LOG_FILE_REOPEN_SEC),
      roll_max_bytes(NET_LOG_ROLL_DEFAULT_MAX_BYTES),
      roll_max_files(NET_LOG_ROLL_DEFAULT_MAX_FILES) {}

AsyncLogSettings::AsyncLogSettings()
    : flush_interval_ms(static_cast<uint32_t>(NET_LOG_ASYNC_FLUSH_MS)),
      buf_bytes(static_cast<size_t>(NET_LOG_ASYNC_BUF_BYTES)),
      flush_threshold(static_cast<size_t>(NET_LOG_ASYNC_FLUSH_THRESHOLD)),
      soft_cap(static_cast<size_t>(NET_LOG_ASYNC_SOFT_CAP)),
      degrade_mode(NET_LOG_DEGRADE_MODE),
      sample_rate(static_cast<uint32_t>(NET_LOG_DEGRADE_SAMPLE_RATE)) {
  if (sample_rate == 0) {
    sample_rate = 1;
  }
}

LogModuleSettings::LogModuleSettings()
    : default_level(NET_LOG_DEFAULT_LEVEL),
      default_pattern(NET_LOG_DEFAULT_PATTERN),
      async(),
      file() {}

LogConfig::LogConfig()
    : settings_(MakeDefaults()),
      accepted_(0),
      dropped_(0),
      sampled_(0),
      sample_seq_(0) {}

LogConfig& LogConfig::instance() {
  static LogConfig cfg;
  return cfg;
}

LogModuleSettings LogConfig::settings() const {
  Spinlock::Lock lock(mtx_);
  return settings_;
}

void LogConfig::apply(const LogModuleSettings& settings) {
  Spinlock::Lock lock(mtx_);
  settings_ = settings;
  if (settings_.async.sample_rate == 0) {
    settings_.async.sample_rate = 1;
  }
}

void LogConfig::applyAsync(const AsyncLogSettings& async) {
  Spinlock::Lock lock(mtx_);
  settings_.async = async;
  if (settings_.async.sample_rate == 0) {
    settings_.async.sample_rate = 1;
  }
}

void LogConfig::resetToDefaults() {
  apply(MakeDefaults());
  resetStats();
}

uint32_t LogConfig::flushIntervalMs() const {
  Spinlock::Lock lock(mtx_);
  return settings_.async.flush_interval_ms;
}

size_t LogConfig::bufBytes() const {
  Spinlock::Lock lock(mtx_);
  return settings_.async.buf_bytes;
}

size_t LogConfig::flushThreshold() const {
  Spinlock::Lock lock(mtx_);
  return settings_.async.flush_threshold;
}

size_t LogConfig::softCap() const {
  Spinlock::Lock lock(mtx_);
  return settings_.async.soft_cap;
}

int LogConfig::degradeMode() const {
  Spinlock::Lock lock(mtx_);
  return settings_.async.degrade_mode;
}

uint32_t LogConfig::sampleRate() const {
  Spinlock::Lock lock(mtx_);
  return settings_.async.sample_rate;
}

int LogConfig::defaultLevel() const {
  Spinlock::Lock lock(mtx_);
  return settings_.default_level;
}

const std::string& LogConfig::defaultPattern() const {
  Spinlock::Lock lock(mtx_);
  return settings_.default_pattern;
}

int LogConfig::fileReopenSec() const {
  Spinlock::Lock lock(mtx_);
  return settings_.file.reopen_sec;
}

uint64_t LogConfig::rollMaxBytes() const {
  Spinlock::Lock lock(mtx_);
  return settings_.file.roll_max_bytes;
}

uint32_t LogConfig::rollMaxFiles() const {
  Spinlock::Lock lock(mtx_);
  return settings_.file.roll_max_files;
}

bool LogConfig::allowAsyncEnqueue(size_t pending_count) {
  const int mode = degradeMode();
  if (mode == 0) {
    return true;
  }

  const size_t cap = softCap();
  if (pending_count < cap) {
    return true;
  }

  if (mode == 1) {
    recordEnqueueDropped();
    return false;
  }

  if (mode == 2) {
    const uint32_t rate = sampleRate();
    const uint64_t seq = sample_seq_.fetch_add(1, std::memory_order_relaxed);
    if (rate <= 1 || (seq % rate) == 0) {
      recordEnqueueSampled();
      return true;
    }
    recordEnqueueDropped();
    return false;
  }

  return true;
}

void LogConfig::recordEnqueueAccepted() {
  accepted_.fetch_add(1, std::memory_order_relaxed);
}

void LogConfig::recordEnqueueDropped() {
  dropped_.fetch_add(1, std::memory_order_relaxed);
}

void LogConfig::recordEnqueueSampled() {
  sampled_.fetch_add(1, std::memory_order_relaxed);
}

uint64_t LogConfig::acceptedCount() const {
  return accepted_.load(std::memory_order_relaxed);
}

uint64_t LogConfig::droppedCount() const {
  return dropped_.load(std::memory_order_relaxed);
}

uint64_t LogConfig::sampledCount() const {
  return sampled_.load(std::memory_order_relaxed);
}

void LogConfig::resetStats() {
  accepted_.store(0, std::memory_order_relaxed);
  dropped_.store(0, std::memory_order_relaxed);
  sampled_.store(0, std::memory_order_relaxed);
  sample_seq_.store(0, std::memory_order_relaxed);
}

}  // namespace net
