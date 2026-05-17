/**
 * @file sink.cc
 * @brief 输出地方案工厂。
 */
#include "log/sink.h"

namespace net {

SinkSet SinkSet::FixedFile(const std::string& path) {
  SinkSet set;
  SinkSpec spec;
  spec.kind = SinkKind::FixedFile;
  spec.path = path;
  set.specs.push_back(spec);
  return set;
}

SinkSet SinkSet::MultiFile(const std::vector<std::string>& paths) {
  SinkSet set;
  for (const auto& p : paths) {
    SinkSpec spec;
    spec.kind = SinkKind::FixedFile;
    spec.path = p;
    set.specs.push_back(spec);
  }
  return set;
}

SinkSet SinkSet::TimeRotate(const std::string& base_path,
                            file_sink::RollInterval interval) {
  SinkSet set;
  SinkSpec spec;
  spec.kind = SinkKind::TimeRotateFile;
  spec.path = base_path;
  spec.roll_interval = interval;
  set.specs.push_back(spec);
  return set;
}

SinkSet SinkSet::SizeChain(const std::string& path, uint64_t max_bytes,
                           uint32_t max_files) {
  SinkSet set;
  SinkSpec spec;
  spec.kind = SinkKind::SizeChainFile;
  spec.path = path;
  spec.max_bytes = max_bytes;
  spec.max_files = max_files;
  set.specs.push_back(spec);
  return set;
}

SinkSet SinkSet::CircularRing(const std::string& base_path,
                              uint32_t slot_count,
                              uint64_t max_bytes_per_slot,
                              const std::vector<std::string>& paths) {
  SinkSet set;
  SinkSpec spec;
  spec.kind = SinkKind::CircularFiles;
  spec.path = base_path;
  spec.slot_count = slot_count;
  spec.max_bytes_per_slot = max_bytes_per_slot;
  spec.paths = paths;
  set.specs.push_back(spec);
  return set;
}

SinkSet SinkSet::ConsoleAndFile(const std::string& path) {
  SinkSet set;
  SinkSpec out;
  out.kind = SinkKind::Stdout;
  set.specs.push_back(out);
  SinkSpec file;
  file.kind = SinkKind::FixedFile;
  file.path = path;
  set.specs.push_back(file);
  return set;
}

LogAppender::ptr MakeAppender(const SinkSpec& spec) {
  LogAppender::ptr app;
  switch (spec.kind) {
    case SinkKind::Stdout:
      app.reset(new StdoutLogAppender());
      break;
    case SinkKind::FixedFile:  // File 为同名枚举别名
      app.reset(new FileLogAppender(spec.path));
      break;
    case SinkKind::TimeRotateFile:
      app.reset(new TimeRotateFileLogAppender(spec.path, spec.roll_interval));
      break;
    case SinkKind::SizeChainFile:  // RollingFile 为同名枚举别名
      app.reset(new RollingFileLogAppender(spec.path, spec.max_bytes,
                                           spec.max_files, spec.roll_interval));
      break;
    case SinkKind::CircularFiles:
      app.reset(new CircularFileLogAppender(spec.path, spec.slot_count,
                                            spec.max_bytes_per_slot,
                                            spec.paths));
      break;
    default:
      return nullptr;
  }
  if (app && spec.level != LogLevel::UNKNOWN) {
    app->setLevel(spec.level);
  }
  return app;
}

void ApplySinkSet(const Logger::ptr& logger, const SinkSet& sinks) {
  if (!logger) {
    return;
  }
  logger->clearAppenders();
  for (const auto& spec : sinks.specs) {
    LogAppender::ptr app = MakeAppender(spec);
    if (app) {
      logger->addAppender(app);
    }
  }
}

}  // namespace net
