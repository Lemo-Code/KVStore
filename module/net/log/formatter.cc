/**
 * @file formatter.cc
 * @brief LogFormatter 模式解析与各 FormatItem 实现。
 *
 * 占位符与 Sylar 兼容：
 *   %m 消息  %p 级别  %r 累计毫秒  %c Logger名  %t 线程ID
 *   %n 换行  %d{fmt} 时间  %f 文件名  %l 行号  %T 制表符
 *   %F 协程ID  %N 线程名
 */
#include "log/formatter.h"
#include "log/logger.h"

#include <ctime>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <tuple>

namespace net {

namespace {

/** %m — 用户消息 */
class MessageFormatItem : public LogFormatter::FormatItem {
 public:
  explicit MessageFormatItem(const std::string& /*str*/ = "") {}
  // 输出用户消息正文
  void format(std::ostream& os, std::shared_ptr<Logger> /*logger*/,
              LogLevel::Level /*level*/, LogEvent::ptr event) override {
    os << event->getSS().str();
  }
};

/** %p — 日志级别 */
class LevelFormatItem : public LogFormatter::FormatItem {
 public:
  explicit LevelFormatItem(const std::string& /*str*/ = "") {}
  // 输出日志级别字符串
  void format(std::ostream& os, std::shared_ptr<Logger> /*logger*/,
              LogLevel::Level level, LogEvent::ptr /*event*/) override {
    os << LogLevel::ToString(level);
  }
};

/** %r — 累计毫秒 */
class ElapseFormatItem : public LogFormatter::FormatItem {
 public:
  explicit ElapseFormatItem(const std::string& /*str*/ = "") {}
  // 输出进程累计毫秒
  void format(std::ostream& os, std::shared_ptr<Logger> /*logger*/,
              LogLevel::Level /*level*/, LogEvent::ptr event) override {
    os << event->getElapse();
  }
};

/** %c — Logger 名称 */
class NameFormatItem : public LogFormatter::FormatItem {
 public:
  explicit NameFormatItem(const std::string& /*str*/ = "") {}
  // 输出 Logger 名称
  void format(std::ostream& os, std::shared_ptr<Logger> /*logger*/,
              LogLevel::Level /*level*/, LogEvent::ptr event) override {
    os << event->getLogger()->getName();
  }
};

/** %t — 线程 ID */
class ThreadIdFormatItem : public LogFormatter::FormatItem {
 public:
  explicit ThreadIdFormatItem(const std::string& /*str*/ = "") {}
  // 输出线程 ID
  void format(std::ostream& os, std::shared_ptr<Logger> /*logger*/,
              LogLevel::Level /*level*/, LogEvent::ptr event) override {
    os << event->getThreadId();
  }
};

/** %F — 协程 ID */
class FiberIdFormatItem : public LogFormatter::FormatItem {
 public:
  explicit FiberIdFormatItem(const std::string& /*str*/ = "") {}
  // 输出协程 ID
  void format(std::ostream& os, std::shared_ptr<Logger> /*logger*/,
              LogLevel::Level /*level*/, LogEvent::ptr event) override {
    os << event->getFiberId();
  }
};

/** %d{fmt} — 日期时间 */
class DateTimeFormatItem : public LogFormatter::FormatItem {
 public:
  explicit DateTimeFormatItem(const std::string& fmt = "%Y-%m-%d %H:%M:%S")
      : fmt_(fmt) {}
  // 按 strftime 子格式输出时间
  void format(std::ostream& os, std::shared_ptr<Logger> /*logger*/,
              LogLevel::Level /*level*/, LogEvent::ptr event) override {
    struct tm t;
    const time_t ts = static_cast<time_t>(event->getTime());
    localtime_r(&ts, &t);
    char buf[64];
    strftime(buf, sizeof(buf), fmt_.c_str(), &t);
    os << buf;
  }

 private:
  std::string fmt_;  ///< strftime 格式
};

/** %f — 源文件名 */
class FileNameFormatItem : public LogFormatter::FormatItem {
 public:
  explicit FileNameFormatItem(const std::string& /*str*/ = "") {}
  // 输出源文件名
  void format(std::ostream& os, std::shared_ptr<Logger> /*logger*/,
              LogLevel::Level /*level*/, LogEvent::ptr event) override {
    os << event->getFile();
  }
};

/** %l — 源行号 */
class LineFormatItem : public LogFormatter::FormatItem {
 public:
  explicit LineFormatItem(const std::string& /*str*/ = "") {}
  // 输出源行号
  void format(std::ostream& os, std::shared_ptr<Logger> /*logger*/,
              LogLevel::Level /*level*/, LogEvent::ptr event) override {
    os << event->getLine();
  }
};

/** %n — 换行 */
class NewLineFormatItem : public LogFormatter::FormatItem {
 public:
  explicit NewLineFormatItem(const std::string& /*str*/ = "") {}
  // 输出换行符
  void format(std::ostream& os, std::shared_ptr<Logger> /*logger*/,
              LogLevel::Level /*level*/, LogEvent::ptr /*event*/) override {
    os << '\n';
  }
};

/** 普通字面量字符 */
class StringFormatItem : public LogFormatter::FormatItem {
 public:
  explicit StringFormatItem(const std::string& str)
      : LogFormatter::FormatItem(str), string_(str) {}
  // 输出普通字面量
  void format(std::ostream& os, std::shared_ptr<Logger> /*logger*/,
              LogLevel::Level /*level*/, LogEvent::ptr /*event*/) override {
    os << string_;
  }

 private:
  std::string string_;
};

/** %T — 制表符 */
class TabFormatItem : public LogFormatter::FormatItem {
 public:
  explicit TabFormatItem(const std::string& /*str*/ = "") {}
  // 输出制表符
  void format(std::ostream& os, std::shared_ptr<Logger> /*logger*/,
              LogLevel::Level /*level*/, LogEvent::ptr /*event*/) override {
    os << '\t';
  }
};

/** %N — 线程名 */
class ThreadNameFormatItem : public LogFormatter::FormatItem {
 public:
  explicit ThreadNameFormatItem(const std::string& /*str*/ = "") {}
  // 输出线程名
  void format(std::ostream& os, std::shared_ptr<Logger> /*logger*/,
              LogLevel::Level /*level*/, LogEvent::ptr event) override {
    os << event->getThreadName();
  }
};

}  // namespace

LogFormatter::LogFormatter(const std::string& pattern)
    : pattern_(pattern), has_error_(false) {
  init();  // 构造时立即解析模式
}

/** 更新模式并重新解析 */
void LogFormatter::setPattern(const std::string& pattern) {
  pattern_ = pattern;
  items_.clear();
  has_error_ = false;
  init();
}

/** 遍历 items_ 按序渲染完整日志行 */
std::string LogFormatter::format(std::shared_ptr<Logger> logger,
                                 LogLevel::Level level, LogEvent::ptr event) {
  std::ostringstream ss;
  for (const auto& item : items_) {
    item->format(ss, logger, level, event);
  }
  return ss.str();
}

/**
 * 解析 pattern_ 为 tokens，再实例化对应 FormatItem。
 *
 * 解析状态机：
 *   - 遇 % 进入占位符解析
 *   - %d{...} 花括号内为 strftime 子格式
 *   - 普通字符累积到 literal 后拆为 StringFormatItem
 */
void LogFormatter::init() {
  // 三元组：(占位符或字面量, 子格式, 是否特殊项)
  std::vector<std::tuple<std::string, std::string, int>> tokens;
  std::string literal;
  std::string spec;
  std::string sub_fmt;

  for (size_t i = 0; i < pattern_.size(); ++i) {
    // 非 % 字符：累积为字面量
    if (pattern_[i] != '%') {
      literal.push_back(pattern_[i]);
      continue;
    }

    // 末尾孤立的 %
    if (i + 1 >= pattern_.size()) {
      literal.push_back('%');
      break;
    }

    // % 后非字母：视为普通字符（如 %%）
    if (!isalpha(pattern_[i + 1])) {
      literal.push_back(pattern_[i + 1]);
      ++i;
      continue;
    }

    // 进入占位符解析，n 指向 % 后首字符
    size_t n = i + 1;
    int brace_begin = 0;   // { 后子格式起始下标
    int brace_state = 0;   // 0=括号外 1=括号内

    while (n < pattern_.size()) {
      // 单字符占位符：%p %m %n 等
      if ((pattern_[n] != '{') && (pattern_[n] != '}') && (brace_state == 0) &&
          isalpha(pattern_[n - 1])) {
        // 无花括号的单字符占位符，如 %p；i 停在占位字母上，由 for 自增跳过
        spec = pattern_.substr(i + 1, 1);
        i = i + 1;
        sub_fmt.clear();
        break;
      }

      // 花括号子格式：%d{%Y-%m-%d %H:%M:%S}
      if (brace_state == 0) {
        if (pattern_[n] == '{') {
          spec = pattern_.substr(n - 1, 1);
          brace_begin = static_cast<int>(n + 1);
          brace_state = 1;
          ++n;
          continue;
        }
      } else if (brace_state == 1) {
        if (pattern_[n] == '}') {
          sub_fmt = pattern_.substr(static_cast<size_t>(brace_begin),
                                    n - static_cast<size_t>(brace_begin));
          brace_state = 0;
          i = n;
          ++n;
          break;
        }
      }
      ++n;

      // 扫描到串尾：处理未闭合花括号或末尾占位符
      if (n == pattern_.size()) {
        if (brace_state == 0) {
          spec = pattern_.substr(n - 1, 1);
          i = n - 1;
          sub_fmt.clear();
          break;
        }

        // 花括号未闭合：标记格式错误
        for (char ch : literal) {
          tokens.push_back(std::make_tuple(std::string(1, ch), "", 0));
        }
        literal.clear();
        tokens.push_back(
            std::make_tuple(spec, "<format error>", 1));
        has_error_ = true;
        return;
      }
    }

    // 将累积的字面量拆为单字符 token
    if (!literal.empty()) {
      for (char ch : literal) {
        tokens.push_back(std::make_tuple(std::string(1, ch), "", 0));
      }
      literal.clear();
    }
    // 记录特殊占位符 token
    tokens.push_back(std::make_tuple(spec, sub_fmt, 1));
    spec.clear();
  }

  // 占位符字符 -> FormatItem 工厂表
  static const std::map<std::string,
                        std::function<FormatItem::ptr(const std::string&)>>
      kFactories = {
#define XX(ch, cls) \
  {#ch, [](const std::string& fmt) { return FormatItem::ptr(new cls(fmt)); }}
          XX(m, MessageFormatItem),
          XX(p, LevelFormatItem),
          XX(r, ElapseFormatItem),
          XX(c, NameFormatItem),
          XX(t, ThreadIdFormatItem),
          XX(n, NewLineFormatItem),
          XX(d, DateTimeFormatItem),
          XX(f, FileNameFormatItem),
          XX(l, LineFormatItem),
          XX(T, TabFormatItem),
          XX(F, FiberIdFormatItem),
          XX(N, ThreadNameFormatItem),
#undef XX
      };

  // 根据 token 列表实例化 FormatItem
  for (const auto& tok : tokens) {
    if (std::get<2>(tok) == 0) {
      // 普通字面量
      items_.push_back(
          FormatItem::ptr(new StringFormatItem(std::get<0>(tok))));
      continue;
    }

    // 特殊占位符：查工厂表，未知占位符标记错误
    const auto it = kFactories.find(std::get<0>(tok));
    if (it == kFactories.end()) {
      items_.push_back(FormatItem::ptr(
          new StringFormatItem("<format error%" + std::get<0>(tok) + ">")));
      has_error_ = true;
    } else {
      items_.push_back(it->second(std::get<1>(tok)));
    }
  }
}

}  // namespace net
