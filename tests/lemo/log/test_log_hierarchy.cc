#include "test_common.h"

#include "lemo/log/logger_repository.h"

int main() {
  lemo::log::Logger::ptr root = lemo::log::LoggerRepository::Instance().GetRoot();
  lemo::log::Logger::ptr com =
      lemo::log::LoggerRepository::Instance().GetLogger("com");
  lemo::log::Logger::ptr svc =
      lemo::log::LoggerRepository::Instance().GetLogger("com.example");

  LEMO_CHECK(com->GetParent() == root);
  LEMO_CHECK(svc->GetParent() == com);
  com->SetLevel(lemo::log::LogLevel::WARN);
  LEMO_CHECK(svc->GetEffectiveLevel() == lemo::log::LogLevel::WARN);
  svc->SetLevel(lemo::log::LogLevel::DEBUG);
  LEMO_CHECK(svc->GetEffectiveLevel() == lemo::log::LogLevel::DEBUG);
  std::printf("PASS test_log_hierarchy\n");
  return 0;
}
