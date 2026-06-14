#include "lemo/log/pattern_layout.h"
#include "lemo/log/level.h"

#include <cctype>
#include <ctime>
#include <functional>
#include <map>
#include <sstream>
#include <tuple>
#include <vector>

namespace lemo {
namespace log {
namespace detail {

struct FormatItem {
  virtual ~FormatItem() {}
  virtual void Format(std::ostream& os, const LogRecord& record) const = 0;
};

class MessageItem : public FormatItem {
 public:
  void Format(std::ostream& os, const LogRecord& record) const override {
    os << record.message;
  }
};

class LevelItem : public FormatItem {
 public:
  void Format(std::ostream& os, const LogRecord& record) const override {
    os << LogLevel::ToString(record.level);
  }
};

class LoggerItem : public FormatItem {
 public:
  void Format(std::ostream& os, const LogRecord& record) const override {
    os << record.logger_name;
  }
};

class ThreadItem : public FormatItem {
 public:
  void Format(std::ostream& os, const LogRecord& record) const override {
    os << record.thread_id;
  }
};

class FiberItem : public FormatItem {
 public:
  void Format(std::ostream& os, const LogRecord& record) const override {
    os << record.fiber_id;
  }
};

class ElapseItem : public FormatItem {
 public:
  void Format(std::ostream& os, const LogRecord& record) const override {
    os << record.elapse;
  }
};

class DateItem : public FormatItem {
 public:
  explicit DateItem(const std::string& fmt) : fmt_(fmt) {}
  void Format(std::ostream& os, const LogRecord& record) const override {
    struct tm t;
    const time_t ts = static_cast<time_t>(record.timestamp);
    localtime_r(&ts, &t);
    char buf[128];
    strftime(buf, sizeof(buf), fmt_.c_str(), &t);
    os << buf;
  }

 private:
  std::string fmt_;
};

class FileItem : public FormatItem {
 public:
  void Format(std::ostream& os, const LogRecord& record) const override {
    os << record.file;
  }
};

class LineItem : public FormatItem {
 public:
  void Format(std::ostream& os, const LogRecord& record) const override {
    os << record.line;
  }
};

class NewLineItem : public FormatItem {
 public:
  void Format(std::ostream& os, const LogRecord&) const override { os << '\n'; }
};

class TabItem : public FormatItem {
 public:
  void Format(std::ostream& os, const LogRecord&) const override { os << '\t'; }
};

class ThreadNameItem : public FormatItem {
 public:
  void Format(std::ostream& os, const LogRecord& record) const override {
    os << record.thread_name;
  }
};

class MdcItem : public FormatItem {
 public:
  explicit MdcItem(const std::string& key) : key_(key) {}
  void Format(std::ostream& os, const LogRecord& record) const override {
    std::map<std::string, std::string>::const_iterator it = record.mdc.find(key_);
    if (it != record.mdc.end()) os << it->second;
  }

 private:
  std::string key_;
};

class LiteralItem : public FormatItem {
 public:
  explicit LiteralItem(const std::string& s) : s_(s) {}
  void Format(std::ostream& os, const LogRecord&) const override { os << s_; }

 private:
  std::string s_;
};

typedef std::function<std::shared_ptr<FormatItem>(const std::string&)> FactoryFn;

const std::map<std::string, FactoryFn>& GetFactoryMap() {
  static std::map<std::string, FactoryFn> factory;
  if (!factory.empty()) return factory;
  factory["m"] = [](const std::string&) {
    return std::shared_ptr<FormatItem>(new MessageItem());
  };
  factory["p"] = [](const std::string&) {
    return std::shared_ptr<FormatItem>(new LevelItem());
  };
  factory["c"] = [](const std::string&) {
    return std::shared_ptr<FormatItem>(new LoggerItem());
  };
  factory["t"] = [](const std::string&) {
    return std::shared_ptr<FormatItem>(new ThreadItem());
  };
  factory["F"] = [](const std::string&) {
    return std::shared_ptr<FormatItem>(new FiberItem());
  };
  factory["r"] = [](const std::string&) {
    return std::shared_ptr<FormatItem>(new ElapseItem());
  };
  factory["f"] = [](const std::string&) {
    return std::shared_ptr<FormatItem>(new FileItem());
  };
  factory["l"] = [](const std::string&) {
    return std::shared_ptr<FormatItem>(new LineItem());
  };
  factory["n"] = [](const std::string&) {
    return std::shared_ptr<FormatItem>(new NewLineItem());
  };
  factory["T"] = [](const std::string&) {
    return std::shared_ptr<FormatItem>(new TabItem());
  };
  factory["N"] = [](const std::string&) {
    return std::shared_ptr<FormatItem>(new ThreadNameItem());
  };
  factory["d"] = [](const std::string& f) {
    return std::shared_ptr<FormatItem>(
        new DateItem(f.empty() ? "%Y-%m-%d %H:%M:%S" : f));
  };
  factory["X"] = [](const std::string& key) {
    return std::shared_ptr<FormatItem>(new MdcItem(key));
  };
  return factory;
}

}  // namespace detail

struct PatternLayout::Impl {
  std::string pattern;
  std::vector<std::shared_ptr<detail::FormatItem> > items;
  bool has_error;
};

PatternLayout::PatternLayout(const std::string& pattern)
    : impl_(new Impl()) {
  impl_->pattern = pattern;
  impl_->has_error = false;
  InitPattern();
}

PatternLayout::~PatternLayout() {}

std::string PatternLayout::Format(const LogRecord& record) const {
  std::ostringstream os;
  for (size_t i = 0; i < impl_->items.size(); ++i) {
    impl_->items[i]->Format(os, record);
  }
  return os.str();
}

const std::string& PatternLayout::GetPattern() const { return impl_->pattern; }

bool PatternLayout::IsError() const { return impl_->has_error; }

void PatternLayout::InitPattern() {
  std::vector<std::tuple<std::string, std::string, int> > tokens;
  std::string literal;
  std::string spec;
  std::string arg;
  const std::string& pattern = impl_->pattern;

  for (size_t i = 0; i < pattern.size(); ++i) {
    if (pattern[i] != '%') {
      literal.push_back(pattern[i]);
      continue;
    }
    if (i + 1 >= pattern.size()) {
      literal.push_back('%');
      continue;
    }
    if (!isalpha(pattern[i + 1])) {
      literal.push_back(pattern[i + 1]);
      ++i;
      continue;
    }

    size_t spec_pos = i + 1;
    spec = pattern.substr(spec_pos, 1);
    arg.clear();
    if (spec_pos + 1 < pattern.size() && pattern[spec_pos + 1] == '{') {
      const size_t end = pattern.find('}', spec_pos + 2);
      if (end == std::string::npos) {
        impl_->has_error = true;
        return;
      }
      arg = pattern.substr(spec_pos + 2, end - spec_pos - 2);
      i = end;
    } else {
      i = spec_pos;
    }

    if (!literal.empty()) {
      for (size_t k = 0; k < literal.size(); ++k) {
        tokens.push_back(std::make_tuple(std::string(1, literal[k]), "", 0));
      }
      literal.clear();
    }
    tokens.push_back(std::make_tuple(spec, arg, 1));
  }
  if (!literal.empty()) {
    tokens.push_back(std::make_tuple(literal, "", 0));
  }

  const std::map<std::string, detail::FactoryFn>& factory = detail::GetFactoryMap();

  for (size_t i = 0; i < tokens.size(); ++i) {
    if (std::get<2>(tokens[i]) == 0) {
      impl_->items.push_back(std::shared_ptr<detail::FormatItem>(
          new detail::LiteralItem(std::get<0>(tokens[i]))));
      continue;
    }
    std::map<std::string, detail::FactoryFn>::const_iterator it =
        factory.find(std::get<0>(tokens[i]));
    if (it == factory.end()) {
      impl_->items.push_back(std::shared_ptr<detail::FormatItem>(new detail::LiteralItem(
          "<err%" + std::get<0>(tokens[i]) + ">")));
      impl_->has_error = true;
    } else {
      impl_->items.push_back(it->second(std::get<1>(tokens[i])));
    }
  }
}

}  // namespace log
}  // namespace lemo
