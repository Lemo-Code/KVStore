#pragma once

// Ledis — High-Performance Redis-Compatible KV Store
//
// 基于 lstl 容器库 + zero 网络库构建。
// 使用: #include "kv_ledis/ledis.h" 包含所有公共头文件。
//
// 架构分层:
//   Protocol    → RESP 协议解析/序列化
//   Core        → 数据结构 (Value, Dict) + 存储引擎 + 命令系统
//   Server      → 会话管理 + 服务器主类
//   Replication → AOF 持久化

// Protocol
#include "kv_ledis/protocol/resp_types.h"
#include "kv_ledis/protocol/resp_parser.h"
#include "kv_ledis/protocol/resp_writer.h"

// Core
#include "kv_ledis/core/value.h"
#include "kv_ledis/core/dict.h"
#include "kv_ledis/core/storage_engine.h"
#include "kv_ledis/core/command.h"

// Server
#include "kv_ledis/server/session.h"
#include "kv_ledis/server/server.h"       // v2: 单线程 fiber
#include "kv_ledis/server/server_v5.h"    // v5: 多核 sharding + epoll
#include "kv_ledis/server/uring_server.h" // vu: io_uring 批量 I/O

// Replication
#include "kv_ledis/replication/aof_writer.h"
