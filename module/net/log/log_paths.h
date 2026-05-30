#ifndef NET_LOG_LOG_PATHS_H
#define NET_LOG_LOG_PATHS_H

#include <string>

namespace net {

/** 日志文件根目录（默认 log/，可用环境变量 NET_LOG_DIR 覆盖）。 */
std::string GetLogDir();

/** 确保日志目录存在（不存在则创建）。 */
bool EnsureLogDir();

/**
 * 将配置或代码中的路径解析为可写路径：
 *   - 绝对路径原样返回
 *   - 相对路径 -> GetLogDir() / name
 */
std::string ResolveLogPath(const std::string& path);

}  // namespace net

#endif  // NET_LOG_LOG_PATHS_H
