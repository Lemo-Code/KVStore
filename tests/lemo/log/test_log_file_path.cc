#include "test_common.h"

#include "lemo/log/file_appender.h"
#include "lemo/log/log.h"
#include "lemo/log/log_paths.h"
#include "lemo/log/logger_repository.h"
#include "lemo/log/pattern_layout.h"

#include <fstream>
#include <string>

namespace {

std::string ReadAll(const std::string& path) {
  std::ifstream in(path.c_str());
  LEMO_CHECK(in.good());
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
}

}  // namespace

int main() {
  const std::string base = "/tmp/lemo_shared_base.log";
  const std::string path_a =
      lemo::log::ResolveLoggerFilePath("svc.a", base);
  const std::string path_b =
      lemo::log::ResolveLoggerFilePath("svc.b", base);
  LEMO_CHECK(path_a == "/tmp/svc.a_lemo_shared_base.log");
  LEMO_CHECK(path_b == "/tmp/svc.b_lemo_shared_base.log");
  LEMO_CHECK(path_a != path_b);

  std::remove(path_a.c_str());
  std::remove(path_b.c_str());

  lemo::log::LoggerRepository& repo = lemo::log::LoggerRepository::Instance();
  lemo::log::Logger::ptr logger_a = repo.GetLogger("svc.a");
  lemo::log::Logger::ptr logger_b = repo.GetLogger("svc.b");
  logger_a->ClearAppenders();
  logger_b->ClearAppenders();
  logger_a->SetAdditive(false);
  logger_b->SetAdditive(false);
  logger_a->SetLayout(lemo::log::Layout::ptr(new lemo::log::PatternLayout("%m")));
  logger_b->SetLayout(lemo::log::Layout::ptr(new lemo::log::PatternLayout("%m")));

  logger_a->AddAppender(lemo::log::FileAppender::ForLogger("svc.a", base));
  logger_b->AddAppender(lemo::log::FileAppender::ForLogger("svc.b", base));

  LEMO_LOG_INFO(logger_a) << "only_a";
  LEMO_LOG_INFO(logger_b) << "only_b";
  logger_a->Flush();
  logger_b->Flush();

  const std::string content_a = ReadAll(path_a);
  const std::string content_b = ReadAll(path_b);
  LEMO_CHECK(content_a.find("only_a") != std::string::npos);
  LEMO_CHECK(content_a.find("only_b") == std::string::npos);
  LEMO_CHECK(content_b.find("only_b") != std::string::npos);
  LEMO_CHECK(content_b.find("only_a") == std::string::npos);

  std::remove(path_a.c_str());
  std::remove(path_b.c_str());
  std::printf("PASS test_log_file_path\n");
  return 0;
}
