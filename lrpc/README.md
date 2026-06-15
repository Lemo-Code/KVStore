# lrpc — 从零构建的 Go RPC 框架

> 通过拆解 gRPC 内部机制，从零实现一个分布式 RPC 框架，理解其核心设计。

## 学习路线

| 阶段 | 模块 | gRPC 对应 | 核心知识点 |
|------|------|-----------|------------|
| 1 | Wire Protocol | HTTP/2 framing | 二进制协议帧、字节序、流控 |
| 2 | Codec | gRPC codec 接口 | 序列化抽象、Protobuf/JSON 可插拔 |
| 3 | Transport | HTTP/2 transport | 连接管理、长连接复用、心跳 |
| 4 | Server | gRPC Server | 反射分发、服务注册、并发模型 |
| 5 | Client | gRPC Client | 连接池、超时传播、backoff 重试 |
| 6 | Interceptor | gRPC Interceptor | 责任链模式、鉴权/日志/metric |
| 7 | Multiplex | HTTP/2 streams | 单连接多 stream 并发、流控 |
| 8 | Balancer | gRPC LB | 负载均衡、服务发现、一致性哈希 |
| 9 | Advanced | gRPC 高级特性 | 熔断、限流、deadline propagation |

## 目录结构

```
lrpc/
├── proto/          # 协议帧定义 & 读写
├── codec/          # 序列化抽象层
├── transport/      # 传输层 (客户端/服务端)
├── interceptor/    # 拦截器链
├── balancer/       # 负载均衡 & 服务发现
├── examples/       # 示例代码
└── docs/           # 详细设计文档
```
