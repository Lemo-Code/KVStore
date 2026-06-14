#pragma once

namespace ledis {

/** RESP 增量解析返回值（protocol.md §6.2）。 */
enum class ParseResult {
  kNeedMore,       ///< 缓冲区内无完整一帧，等待更多数据
  kOk,             ///< 成功解析一帧
  kProtocolError,  ///< 格式非法
};

}  // namespace ledis
