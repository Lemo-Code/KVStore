# 03 — Transport 传输层

## 设计动机

gRPC 的传输层基于 HTTP/2，提供:
- 长连接复用
- 流多路复用 (多个 stream 共享一个 TCP 连接)
- 流控 (WINDOW_UPDATE)
- HPACK 头部压缩

lrpc 的传输层直接在 TCP 上工作，提供帧级别的读写封装。

## 设计

### Conn 封装

```go
type Conn struct {
    conn   net.Conn
    mu     sync.Mutex  // 保护写操作 (多 goroutine 可并发写)
    closed bool
}
```

设计原则:
- **写加锁**: 多个 goroutine 可同时写(如多个请求并发发送响应)
- **读不加锁**: 每个连接只有一个读 goroutine
- **帧为最小单位**: 不拆帧，不粘包

### 读写流程

```
Server 端:
  Accept() → net.Conn
  → transport.NewConn(conn)
  → ReceiveFrame() loop (单 goroutine)
  → 每帧启动新 goroutine 处理 (并发)
  → SendFrame() 发送响应 (多 goroutine 安全写)

Client 端:
  Dial() → net.Conn
  → transport.NewConn(conn)
  → readLoop() (单 goroutine)
  → Go()/Call() 发送请求 → pending 等待
  → readLoop 收到响应 → 匹配 pending call → 唤醒调用者
```

### 心跳机制

```
发送方                       接收方
  │                            │
  │──── Heartbeat ────────────→│
  │                            │ 收到心跳
  │←──── HeartbeatAck ────────│ 回复心跳确认
  │                            │

客户端或服务端都可以发起心跳。
心跳帧的 StreamID = 0, Payload 为空。
```

## 性能设计

| 决策 | 选择 | 理由 |
|------|------|------|
| TCP NoDelay | 默认 | RPC 请求不应被 Nagle 算法延迟 |
| 读写缓冲 | 依赖 OS | 后续可加用户态缓冲 |
| 连接池 | 单连接 | Phase 1, 后续支持连接池 |

## 与 gRPC 对比

| 特性 | gRPC | lrpc |
|------|------|------|
| 传输协议 | HTTP/2 over TCP | 自定义帧 over TCP |
| 连接复用 | HTTP/2 stream | 单连接, 后续版本支持 multiplex |
| 流控 | HTTP/2 WINDOW_UPDATE | 无 (后续版本) |
| 心跳 | HTTP/2 PING | 自定义 Heartbeat/HeartbeatAck 帧 |
| 连接建立 | TCP + TLS ALPN | TCP |
