# KVStore Project

> 高性能 Linux C++ 基础设施项目群 | C++14/17 | ARM64/x86_64

## 项目组成

| 子项目 | 状态 | 说明 | 性能 |
|--------|------|------|------|
| **lstl** | ✅ | 轻量 STL — 内存池 + 10 种容器 | pool 比 malloc 快 40-71% |
| **Zero 网络库** | ✅ | 协程 + M:N调度 + epoll + Hook | Echo 35万 QPS / P50=67μs |
| **lrpc** | 📝 | RPC 框架 | — |

## 文档入口

📋 **[docs/INDEX.md](docs/INDEX.md)** — 全局 K-V 文档索引 (30+ 条目)

## 快速链接

| 文档 | 说明 |
|------|------|
| [ARCHITECTURE.md](ARCHITECTURE.md) | Zero 网络库架构设计 |
| [zero/docs/API.md](zero/docs/API.md) | Zero 网络库 API 参考 |
| [lstl/docs/API_REFERENCE.md](newFolder/lstl/docs/API_REFERENCE.md) | lstl API 参考 |
| [lstl/docs/PROJECT_RETROSPECTIVE.md](newFolder/lstl/docs/PROJECT_RETROSPECTIVE.md) | lstl 项目复盘 |
| [lstl/docs/COMPARE.md](newFolder/lstl/docs/COMPARE.md) | lstl vs STL 对比 |
| [lstl/docs/BUG_AUDIT.md](newFolder/lstl/docs/BUG_AUDIT.md) | lstl Bug 审计报告 |
