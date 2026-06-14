#pragma once

#include "ledis/types.h"

#include <map>
#include <utility>
#include <vector>

namespace ledis {

/** 解析短选项 (-s/-d/-c/-p) 与 exe/cwd，长选项 (--async 等) 留给 LedisSettings。 */
class Env {
 public:
  bool init(int argc, char** argv);

  void addHelp(const String& key, const String& desc);
  /** 启动手册 / --help 共用同一份内容。 */
  void printManual() const;
  void printHelp() const { printManual(); }

  bool has(const String& key) const;
  String get(const String& key, const String& default_value = "") const;

  String program() const { return program_; }
  String exe() const { return exe_; }
  String cwd() const { return cwd_; }

  String absolutePath(const String& path) const;
  String configPath() const;

 private:
  void add(const String& key, const String& val);

  std::map<String, String> args_;
  std::vector<std::pair<String, String>> helps_;
  String program_;
  String exe_;
  String cwd_;
};

}  // namespace ledis
