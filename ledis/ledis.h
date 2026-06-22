#pragma once

// Ledis — High-Performance Redis-Compatible KV Store
//
// 基于 lstl 容器库 + zero 网络库构建。
// 使用: #include "ledis/ledis.h" 包含所有公共头文件。
//
// 架构分层:
//   Protocol    → RESP 协议解析/序列化
//   Core        → 数据结构 (Value, Dict) + 存储引擎 + 命令系统
//   Server      → 会话管理 + 服务器主类
//   Replication → AOF 持久化

// Protocol
#include "ledis/protocol/resp_types.h"
#include "ledis/protocol/resp_parser.h"
#include "ledis/protocol/resp_writer.h"

// Core
#include "ledis/core/value.h"
#include "ledis/core/dict.h"
#include "ledis/core/storage_engine.h"
#include "ledis/core/command.h"

// Server
#include "ledis/server/session.h"
#include "ledis/server/server.h"       // v2: 单线程 fiber
#include "ledis/server/server_v5.h"    // v5: 多核 sharding + epoll
#include "ledis/server/uring_server.h" // vu: io_uring 批量 I/O

// Replication
#include "ledis/replication/aof_writer.h"
