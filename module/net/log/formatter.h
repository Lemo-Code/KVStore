#ifndef NET_LOG_FORMATTER_H
#define NET_LOG_FORMATTER_H

#include "log/event.h"
#include "log/level.h"

#include <memory>
#include <string>
#include <vector>

namespace net {

class Logger;

/**
 * @brief 日志格式器，将 LogEvent 按模式字符串渲染为最终输出文本。
 *
 * 模式语法与 Sylar 兼容，例如：
 *   %d{%Y-%m-%d %H:%M:%S}%T[%p]%T%m%n
 */
class LogFormatter {
 public:
  typedef std::shared_ptr<LogFormatter> ptr;

  // 根据模式字符串构造格式器并立即解析
  explicit LogFormatter(const std::string& pattern);

  // 将事件格式化为完整日志行
  std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level,
                     LogEvent::ptr event);

  // 直接追加到 out，减少临时对象（FormatLine 热路径）
  void formatTo(std::string& out, std::shared_ptr<Logger> logger,
                LogLevel::Level level, LogEvent::ptr event);

  // 获得日志的格式
  const std::string& getPattern() const { return pattern_; }

  // 设置格式器模式并重新解析
  void setPattern(const std::string& pattern);

  // 格式是否为空
  bool isPatternEmpty() const { return pattern_.empty(); }

  // 格式解析是否有错
  bool hasError() const { return has_error_; }

  // 格式解析是否正确
  bool isValid() const { return !has_error_; }

  // 格式项抽象基类，每个占位符对应一个子类实例
  class FormatItem {
   public:
    typedef std::shared_ptr<FormatItem> ptr;

    explicit FormatItem(const std::string& /*str*/ = "") {}
    virtual ~FormatItem() {}

    virtual void appendTo(std::string& out, std::shared_ptr<Logger> logger,
                          LogLevel::Level level, LogEvent::ptr event) = 0;
  };

 private:
  // 解析 pattern_ 并填充 items_
  void init();

  std::string pattern_;                  // 原始模式字符串
  std::vector<FormatItem::ptr> items_;   // 解析后的格式项序列
  bool has_error_;                       // 解析是否出错
};

}  // namespace net

#endif  // NET_LOG_FORMATTER_H
