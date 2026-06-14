#include "test_common.h"

#include "lemo/log/appender.h"
#include "lemo/log/event.h"
#include "lemo/log/logger_repository.h"
#include "lemo/log/pattern_layout.h"

#include <vector>

namespace {

class CollectAppender : public lemo::log::Appender {
 public:
  void Append(const lemo::log::LogRecord& record) override {
    records_.push_back(record.logger_name + ":" + record.message);
  }
  void Flush() override {}
  const char* Type() const override { return "collect"; }
  const std::vector<std::string>& Records() const { return records_; }
  void Clear() { records_.clear(); }

 private:
  std::vector<std::string> records_;
};

}  // namespace

int main() {
  lemo::log::Logger::ptr root = lemo::log::LoggerRepository::Instance().GetRoot();
  lemo::log::Logger::ptr child =
      lemo::log::LoggerRepository::Instance().GetLogger("com.example");
  root->ClearAppenders();
  child->ClearAppenders();

  lemo::log::Layout::ptr layout(new lemo::log::PatternLayout("%m"));
  lemo::log::Appender::ptr root_sink(new CollectAppender());
  lemo::log::Appender::ptr child_sink(new CollectAppender());
  root_sink->SetLayout(layout);
  child_sink->SetLayout(layout);
  root->AddAppender(root_sink);
  child->AddAppender(child_sink);

  lemo::log::LogEvent::ptr event(new lemo::log::LogEvent(
      child, lemo::log::LogLevel::DEBUG, __FILE__, __LINE__, 0, 1, 0, time(0),
      "t"));
  event->GetSS() << "msg";
  child->Log(lemo::log::LogLevel::DEBUG, event);

  CollectAppender* root_col = static_cast<CollectAppender*>(root_sink.get());
  CollectAppender* child_col = static_cast<CollectAppender*>(child_sink.get());
  LEMO_CHECK(root_col->Records().size() == 1);
  LEMO_CHECK(child_col->Records().size() == 1);

  root_col->Clear();
  child_col->Clear();
  child->SetAdditive(false);
  child->Log(lemo::log::LogLevel::DEBUG, event);
  LEMO_CHECK(root_col->Records().empty());
  LEMO_CHECK(child_col->Records().size() == 1);
  std::printf("PASS test_log_additivity\n");
  return 0;
}
