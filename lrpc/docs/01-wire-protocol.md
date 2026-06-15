# 01 — Wire Protocol (帧协议层)

## 设计动机

gRPC 使用 HTTP/2 作为传输层，HTTP/2 的帧格式 (9 bytes header) 已经提供了:
- 流多路复用 (Stream ID)
- 流控 (WINDOW_UPDATE)
- HPACK 头部压缩

gRPC 在 HTTP/2 DATA 帧内部再包一层薄帧:

```
[1 byte compressed-flag][4 bytes message-length][protobuf payload]
```

**lrpc 的目标**: 直接从 TCP 构建, 不依赖 HTTP/2 库, 所以需要自建帧协议。

## 设计决策

| 决策点 | 选择 | 理由 |
|--------|------|------|
| 魔数 | 2 字节 `0x4C52` ('LR') | 快速识别 lrpc 协议，区分非 lrpc 连接 |
| 版本号 | 1 字节 | 未来协议升级时做兼容 |
| 消息类型 | 1 字节, 7 种类型 | 覆盖一元 RPC + 三种流式 + 心跳 |
| StreamID | 4 字节, 客户端生成奇数/服务端生成偶数 | 借鉴 HTTP/2 stream ID 分配规则 |
| 负载长度 | 4 字节, 最大 16 MiB | 防止内存攻击 |
| 字节序 | Big-Endian | 网络字节序标准 |

## 帧结构

```
┌──────────────────────────────────────────────────────┐
│ offset  size  field                                   │
├──────────────────────────────────────────────────────┤
│ 0       2     Magic       = 0x4C52 ('L','R')        │
│ 2       1     Version     = 0x01                     │
│ 3       1     MsgType     见下表                     │
│ 4       4     StreamID    uint32 BE                   │
│ 8       4     PayloadLen  uint32 BE                   │
├──────────────────────────────────────────────────────┤
│ 12      N     Payload     业务数据                    │
└──────────────────────────────────────────────────────┘
Total header: 12 bytes
```

## 消息类型详解

| 值 | 类型 | 方向 | 用途 |
|----|------|------|------|
| 0x01 | UnaryRequest | C→S | 一元请求，Payload = 序列化的 request body |
| 0x02 | UnaryResponse | S→C | 一元响应，Payload = 序列化的 response body |
| 0x03 | ErrorResponse | S→C | 错误响应，Payload = JSON 编码的错误信息 |
| 0x04 | StreamData | 双向 | 流式数据帧，客户端/服务端/双向流都使用 |
| 0x05 | StreamEnd | 双向 | 发送方标记流结束 (half-close) |
| 0x06 | Heartbeat | 双向 | 心跳检测 |
| 0x07 | HeartbeatAck | 双向 | 心跳回复 |

## StreamID 分配规则

借鉴 HTTP/2 设计:
- **客户端发起的流**: StreamID 为奇数 (1, 3, 5, ...)
- **服务端发起的流**: StreamID 为偶数 (2, 4, 6, ...)
- **一元 RPC**: StreamID = 0 (特殊值，不参与流管理)

## 典型交互时序

### 一元 RPC (Unary)
```
Client                                  Server
  │                                       │
  │── UnaryRequest(StreamID=0, payload)──→│
  │                                       │ 处理请求
  │←─ UnaryResponse(StreamID=0, payload)─│
  │                                       │
```

### 服务端流式 (Server Streaming)
```
Client                                  Server
  │                                       │
  │── UnaryRequest(StreamID=1, payload)──→│
  │                                       │ 开始流式返回
  │←───── StreamData(StreamID=1, d1)─────│
  │←───── StreamData(StreamID=1, d2)─────│
  │←───── StreamData(StreamID=1, d3)─────│
  │←───── StreamEnd(StreamID=1)──────────│
  │                                       │
```

### 心跳
```
Client                                  Server
  │                                       │
  │──────── Heartbeat(StreamID=0)────────→│
  │                                       │
  │←─────── HeartbeatAck(StreamID=0)──────│
  │                                       │
```

## 与 gRPC 帧协议的对比

| 特性 | gRPC (HTTP/2) | lrpc |
|------|---------------|------|
| 传输层 | HTTP/2 | 原始 TCP |
| 帧头大小 | 5 bytes | 12 bytes |
| 多路复用 | HTTP/2 Stream | StreamID 字段 |
| 头部压缩支持 | HPACK | 预留 Version 字段 |
| 流控 | HTTP/2 WINDOW_UPDATE | 后续版本 |
| 魔数/协议识别 | 无 (ALPN "h2") | 2 bytes 魔数 |
