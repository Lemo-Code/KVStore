#ifndef NET_TEST_COMMON_H
#define NET_TEST_COMMON_H

#include "log/async_sink.h"
#include "log/log_paths.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

/** 断言失败时打印位置并退出 */
#define NET_CHECK(cond)                                                        \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)

namespace net_test {

/** 测试/压测日志写入 bin/net/log/（ctest 通过 NET_LOG_DIR 指向该目录）。 */
inline std::string LogPath(const std::string& filename) {
  net::EnsureLogDir();
  return net::ResolveLogPath(filename);
}

inline std::string ReadFile(const std::string& path) {
  std::ifstream ifs(path.c_str());
  return std::string((std::istreambuf_iterator<char>(ifs)),
                     std::istreambuf_iterator<char>());
}

/** 排空异步队列并刷写 ByteBuffer（替代 sleep 等待） */
inline void FlushAsyncLogs() {
  net::AsyncLogMgr::GetInstance()->flush();
}

}  // namespace net_test

#endif  // NET_TEST_COMMON_H
