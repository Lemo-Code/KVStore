#pragma once

#include "lemo/log/appender.h"

#include <cstdint>
#include <fstream>
#include <string>

namespace lemo {
namespace log {

enum class RollInterval { kNone, kHour, kDay };

class RollingFileAppender : public Appender {
 public:
  typedef std::shared_ptr<RollingFileAppender> ptr;

  RollingFileAppender(const std::string& path, uint64_t max_bytes,
                      uint32_t max_files, RollInterval interval);

  static ptr ForLogger(const std::string& logger_name,
                       const std::string& base_path, uint64_t max_bytes,
                       uint32_t max_files, RollInterval interval);

  void Append(const LogRecord& record) override;
  void Flush() override;
  const char* Type() const override;

 private:
  void OpenCurrentUnlocked();
  void OpenCurrent();
  void RollBySizeUnlocked();
  void RollByTimeUnlocked();
  void RollBySize();
  void RollByTime();

  std::string path_;
  uint64_t max_bytes_;
  uint32_t max_files_;
  RollInterval interval_;
  std::ofstream stream_;
  uint64_t current_bytes_;
  uint64_t last_roll_sec_;
};

}  // namespace log
}  // namespace lemo
