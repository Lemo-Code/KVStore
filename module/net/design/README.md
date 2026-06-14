# net 接口骨架（design/）

本目录存放**目标架构的接口头文件**，用于冻结各层 API，**不参与 CMake 编译**。

实现阶段在对应正式目录落地代码：

| design/ 路径 | 正式实现路径 |
|--------------|--------------|
| `runtime/` | `module/net/runtime/` |
| `io/` | `module/net/io/` |
| `transport/` | `module/net/transport/` |
| `server/` | `module/net/server/` |

架构说明见 [docs/net/architecture.md](../../../docs/net/architecture.md)。
