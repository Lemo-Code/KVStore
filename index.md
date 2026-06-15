# KVStore Project

> 高性能 Linux C++ 基础设施项目群

## 项目组成

| 子项目 | 状态 | 说明 |
|--------|------|------|
| **lstl** | ✅ 完成 | 轻量 STL 替代 — 内存池 + 10 种容器 |
| **Zero 网络库** | 📝 设计 | 协程 + epoll + M:N 调度 |
| **lrpc** | 📝 设计 | RPC 框架 |
| **ledis** | 原有 | 缓存服务器 |

## 文档入口

📋 **[docs/INDEX.md](docs/INDEX.md)** — 全局 K-V 文档索引

- 按受众导航（面试官 / 开发者 / 审查者）
- 按模块导航（架构 / 基础库 / RPC / 网络 / 缓存）
- 按状态导航（已完成 / 设计中 / 规划中）

## 快速链接

| 文档 | 说明 |
|------|------|
| [ARCHITECTURE.md](ARCHITECTURE.md) | Zero 网络库架构设计 |
| [newFolder/lstl/docs/API_REFERENCE.md](newFolder/lstl/docs/API_REFERENCE.md) | lstl API 参考手册 |
| [newFolder/lstl/docs/PROJECT_RETROSPECTIVE.md](newFolder/lstl/docs/PROJECT_RETROSPECTIVE.md) | lstl 项目复盘 (秋招) |

## 构建

```bash
cd newFolder/lstl/build
cmake .. -DLSTL_BUILD_TESTS=ON -DLSTL_BUILD_BENCH=ON
make -j$(nproc)
ctest --output-on-failure          # 15/15 PASS
```
