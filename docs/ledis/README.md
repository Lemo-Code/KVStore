# Ledis 文档

Ledis 是 KVStore 仓库内的 **Redis 兼容内存 KV 服务**，网络层基于 **LemoNettyCore**（`liblemo_nettycore.so`），存储与协议层在 `ledis/` 模块内实现。

| 文档 | 内容 |
|------|------|
| **[features.md](./features.md)** | **已实现功能、命令列表、持久化、启动框架、测试** |
| **[configuration.md](./configuration.md)** | **全部配置项：CLI、YAML、CONFIG GET/SET、守护进程** |
| **[protocol.md](./protocol.md)** | RESP2 协议约定 |
| [architecture.md](./architecture.md) | 总体分层、并发模型、模块边界 |
| [layer_contracts.md](./layer_contracts.md) | 各子模块公开接口契约 |
| [implementation_order.md](./implementation_order.md) | 分阶段实现顺序与验收标准 |

## 快速开始

```bash
cmake -B build-nettycore -DLEDIS_BUILD=ON -DLEMONETTYCORE_BUILD=ON
cmake --build build-nettycore --target ledis-server run_tests_ledis -j$(nproc)

# 前台启动（自动加载 bin/Ledis/conf/ledis.yaml，若存在）
./bin/Ledis/ledis-server

# 指定地址端口
./bin/Ledis/ledis-server 127.0.0.1 6379

# 使用 YAML + 异步 IO/DB
./bin/Ledis/ledis-server -c conf --async --io-threads 4

redis-cli -p 6379 PING
```

帮助与守护进程：

```bash
./bin/Ledis/ledis-server -p          # 打印全部 CLI 选项
./bin/Ledis/ledis-server -d --dir /data/ledis   # 守护进程 + pidfile
```

配置详见 [configuration.md](./configuration.md)；命令与能力详见 [features.md](./features.md)。

## 相关文档

| 文档 | 说明 |
|------|------|
| [third/LemoNettyCore/README.md](../../third/LemoNettyCore/README.md) | 网络库构建与 API |
| [docs/lstl/memory_pool_design.md](../lstl/memory_pool_design.md) | kv_pool 与对象尺寸 |
| [docs/net/architecture.md](../net/architecture.md) | 协程网络栈分层参考 |

## 源码布局

```
ledis/
  include/ledis/app/     Env / Application / Daemon / FsUtil
  include/ledis/...      协议、存储、会话、命令
  conf/ledis.yaml        完整配置示例（构建时复制到 bin/Ledis/conf/）
tests/ledis/             单元/集成测试（35 项 CTest）
bin/Ledis/               ledis-server 与 conf/
docs/ledis/              本目录
```
