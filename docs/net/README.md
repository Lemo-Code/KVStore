# 协程网络库架构文档

本目录描述 KVStore **协程网络库（`net::`）** 的目标架构，用于替代当前 `module/net` 与 `sylar/` 参考实现中的设计问题。

| 文档 | 内容 |
|------|------|
| [architecture.md](./architecture.md) | 总体分层、线程模型、数据流、设计原则 |
| [layer_contracts.md](./layer_contracts.md) | 各层公开接口契约（头文件级） |
| [roadmap.md](./roadmap.md) | 分阶段实施路线与验收标准 |

接口骨架（仅契约，不参与编译）位于：

```
module/net/design/
```

旧实现保留在 `module/net/{fiber,io,socket,...}`，待新架构逐层落地后迁移替换。
