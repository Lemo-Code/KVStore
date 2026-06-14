#pragma once

#include "lemo/log/appender.h"

#include <fstream>
#include <string>

namespace lemo {
namespace log {

class FileAppender : public Appender {
 public:
  typedef std::shared_ptr<FileAppender> ptr;
  explicit FileAppender(const std::string& path);

  /** 按 logger 名解析 path 后创建，推荐业务侧使用。 */
  static ptr ForLogger(const std::string& logger_name,
                       const std::string& base_path);

  void Append(const LogRecord& record) override;
  void Flush() override;
  const char* Type() const override;
  bool Reopen();

 private:
  std::string path_;
  std::ofstream stream_;
  uint64_t last_reopen_sec_;
};

}  // namespace log
}  // namespace lemo
