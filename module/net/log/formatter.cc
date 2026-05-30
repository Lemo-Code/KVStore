/**
 * @file formatter.cc
 * @brief LogFormatter 模式解析与各 FormatItem 实现。
 */
#include "log/formatter.h"
#include "log/format_util.h"
#include "log/logger.h"

#include <ctime>
#include <functional>
#include <iostream>
#include <map>
#include <tuple>

namespace net {

namespace {

class MessageFormatItem : public LogFormatter::FormatItem {
 public:
  explicit MessageFormatItem(const std::string& /*str*/ = "") {}
  void appendTo(std::string& out, std::shared_ptr<Logger> /*logger*/,
                LogLevel::Level /*level*/, LogEvent::ptr event) override {
    out += event->getMessage();
  }
};

class LevelFormatItem : public LogFormatter::FormatItem {
 public:
  explicit LevelFormatItem(const std::string& /*str*/ = "") {}
  void appendTo(std::string& out, std::shared_ptr<Logger> /*logger*/,
                LogLevel::Level level, LogEvent::ptr /*event*/) override {
    out += LogLevel::ToString(level);
  }
};

class ElapseFormatItem : public LogFormatter::FormatItem {
 public:
  explicit ElapseFormatItem(const std::string& /*str*/ = "") {}
  void appendTo(std::string& out, std::shared_ptr<Logger> /*logger*/,
                LogLevel::Level /*level*/, LogEvent::ptr event) override {
    format_util::AppendUInt(out, event->getElapse());
  }
};

class NameFormatItem : public LogFormatter::FormatItem {
 public:
  explicit NameFormatItem(const std::string& /*str*/ = "") {}
  void appendTo(std::string& out, std::shared_ptr<Logger> /*logger*/,
                LogLevel::Level /*level*/, LogEvent::ptr event) override {
    out += event->getLogger()->getName();
  }
};

class ThreadIdFormatItem : public LogFormatter::FormatItem {
 public:
  explicit ThreadIdFormatItem(const std::string& /*str*/ = "") {}
  void appendTo(std::string& out, std::shared_ptr<Logger> /*logger*/,
                LogLevel::Level /*level*/, LogEvent::ptr event) override {
    format_util::AppendUInt(out, event->getThreadId());
  }
};

class FiberIdFormatItem : public LogFormatter::FormatItem {
 public:
  explicit FiberIdFormatItem(const std::string& /*str*/ = "") {}
  void appendTo(std::string& out, std::shared_ptr<Logger> /*logger*/,
                LogLevel::Level /*level*/, LogEvent::ptr event) override {
    format_util::AppendUInt(out, event->getFiberId());
  }
};

class DateTimeFormatItem : public LogFormatter::FormatItem {
 public:
  explicit DateTimeFormatItem(const std::string& fmt = "%Y-%m-%d %H:%M:%S")
      : fmt_(fmt) {}
  void appendTo(std::string& out, std::shared_ptr<Logger> /*logger*/,
                LogLevel::Level /*level*/, LogEvent::ptr event) override {
    struct tm t;
    const time_t ts = static_cast<time_t>(event->getTime());
    localtime_r(&ts, &t);
    char buf[64];
    const size_t n = strftime(buf, sizeof(buf), fmt_.c_str(), &t);
    if (n > 0) {
      out.append(buf, n);
    }
  }

 private:
  std::string fmt_;
};

class FileNameFormatItem : public LogFormatter::FormatItem {
 public:
  explicit FileNameFormatItem(const std::string& /*str*/ = "") {}
  void appendTo(std::string& out, std::shared_ptr<Logger> /*logger*/,
                LogLevel::Level /*level*/, LogEvent::ptr event) override {
    const char* f = event->getFile();
    if (f) {
      out += f;
    }
  }
};

class LineFormatItem : public LogFormatter::FormatItem {
 public:
  explicit LineFormatItem(const std::string& /*str*/ = "") {}
  void appendTo(std::string& out, std::shared_ptr<Logger> /*logger*/,
                LogLevel::Level /*level*/, LogEvent::ptr event) override {
    format_util::AppendInt(out, event->getLine());
  }
};

class NewLineFormatItem : public LogFormatter::FormatItem {
 public:
  explicit NewLineFormatItem(const std::string& /*str*/ = "") {}
  void appendTo(std::string& out, std::shared_ptr<Logger> /*logger*/,
                LogLevel::Level /*level*/, LogEvent::ptr /*event*/) override {
    out.push_back('\n');
  }
};

class StringFormatItem : public LogFormatter::FormatItem {
 public:
  explicit StringFormatItem(const std::string& str)
      : LogFormatter::FormatItem(str), string_(str) {}
  void appendTo(std::string& out, std::shared_ptr<Logger> /*logger*/,
                LogLevel::Level /*level*/, LogEvent::ptr /*event*/) override {
    out += string_;
  }

 private:
  std::string string_;
};

class TabFormatItem : public LogFormatter::FormatItem {
 public:
  explicit TabFormatItem(const std::string& /*str*/ = "") {}
  void appendTo(std::string& out, std::shared_ptr<Logger> /*logger*/,
                LogLevel::Level /*level*/, LogEvent::ptr /*event*/) override {
    out.push_back('\t');
  }
};

class ThreadNameFormatItem : public LogFormatter::FormatItem {
 public:
  explicit ThreadNameFormatItem(const std::string& /*str*/ = "") {}
  void appendTo(std::string& out, std::shared_ptr<Logger> /*logger*/,
                LogLevel::Level /*level*/, LogEvent::ptr event) override {
    out += event->getThreadName();
  }
};

}  // namespace

LogFormatter::LogFormatter(const std::string& pattern)
    : pattern_(pattern), has_error_(false) {
  init();
}

void LogFormatter::setPattern(const std::string& pattern) {
  pattern_ = pattern;
  items_.clear();
  has_error_ = false;
  init();
}

std::string LogFormatter::format(std::shared_ptr<Logger> logger,
                                 LogLevel::Level level, LogEvent::ptr event) {
  std::string out;
  formatTo(out, logger, level, event);
  return out;
}

void LogFormatter::formatTo(std::string& out, std::shared_ptr<Logger> logger,
                            LogLevel::Level level, LogEvent::ptr event) {
  out.clear();
  out.reserve(128 + event->getMessage().size());
  for (const auto& item : items_) {
    item->appendTo(out, logger, level, event);
  }
}

void LogFormatter::init() {
  std::vector<std::tuple<std::string, std::string, int>> tokens;
  std::string literal;
  std::string spec;
  std::string sub_fmt;

  for (size_t i = 0; i < pattern_.size(); ++i) {
    if (pattern_[i] != '%') {
      literal.push_back(pattern_[i]);
      continue;
    }

    if (i + 1 >= pattern_.size()) {
      literal.push_back('%');
      break;
    }

    if (!isalpha(pattern_[i + 1])) {
      literal.push_back(pattern_[i + 1]);
      ++i;
      continue;
    }

    size_t n = i + 1;
    int brace_begin = 0;
    int brace_state = 0;

    while (n < pattern_.size()) {
      if ((pattern_[n] != '{') && (pattern_[n] != '}') && (brace_state == 0) &&
          isalpha(pattern_[n - 1])) {
        spec = pattern_.substr(i + 1, 1);
        i = i + 1;
        sub_fmt.clear();
        break;
      }

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

      if (n == pattern_.size()) {
        if (brace_state == 0) {
          spec = pattern_.substr(n - 1, 1);
          i = n - 1;
          sub_fmt.clear();
          break;
        }

        for (char ch : literal) {
          tokens.push_back(std::make_tuple(std::string(1, ch), "", 0));
        }
        literal.clear();
        tokens.push_back(std::make_tuple(spec, "<format error>", 1));
        has_error_ = true;
        return;
      }
    }

    if (!literal.empty()) {
      for (char ch : literal) {
        tokens.push_back(std::make_tuple(std::string(1, ch), "", 0));
      }
      literal.clear();
    }
    tokens.push_back(std::make_tuple(spec, sub_fmt, 1));
    spec.clear();
  }

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

  for (const auto& tok : tokens) {
    if (std::get<2>(tok) == 0) {
      items_.push_back(
          FormatItem::ptr(new StringFormatItem(std::get<0>(tok))));
      continue;
    }

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
