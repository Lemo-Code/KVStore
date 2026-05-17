#ifndef NET_LOG_LEVEL_H
#define NET_LOG_LEVEL_H

#include <string>

namespace net {

/**
 * @brief 日志级别工具类。
 *
 * 提供级别枚举以及与字符串的双向转换，供 Logger、Appender、Formatter 共用。
 */
class LogLevel {
 public:
  /**
   * @brief 日志级别枚举，数值越小输出越详细。
   */
  enum Level {
    UNKNOWN = 0,  ///< 未知级别，通常表示未配置
    DEBUG = 1,    ///< 调试信息
    INFO = 2,     ///< 常规运行信息
    WARN = 3,     ///< 潜在问题警告
    ERROR = 4,    ///< 可恢复错误
    FATAL = 5,    ///< 致命错误
  };

  // 将 Level 类型的日志级别转成 string 类型
  static const char* ToString(Level level);

  // 将 string 转成 Level，无法识别返回 UNKNOWN
  static Level FromString(const std::string& str);
};

}  // namespace net

#endif  // NET_LOG_LEVEL_H
