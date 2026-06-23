#pragma once
// ============================================================
// lrpc/lrpc.h — 轻量 RPC 框架入口
// ============================================================
//
// 使用:
//   #include "lrpc/lrpc.h"
//
// 分层:
//   protocol.h      — 二进制协议定义 (帧格式, 编解码)
//   rpc_connection.h — RPC 连接管理 (epoll I/O 线程, 请求/响应)
//

#include "lrpc/protocol.h"
#include "lrpc/rpc_connection.h"
