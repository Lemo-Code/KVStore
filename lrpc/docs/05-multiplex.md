# 05 — Multiplex 多路复用

## 设计动机

gRPC 的核心性能优势来自 HTTP/2 的多路复用:
- 多个 RPC 调用共享一条 TCP 连接
- 避免每个请求建立/拆除连接的开销
- 减少端口消耗和服务端连接数
- Header 行首阻塞的解决 (HTTP/2 vs HTTP/1.1)

lrpc 在自定义帧协议上直接实现多路复用，核心是 StreamID。

## 架构

```
                    ┌──────────────────────────────┐
                    │         MuxConn              │
                    │                              │
  goroutine-1 ─────→│ Stream(id=1) ┐              │
  goroutine-2 ─────→│ Stream(id=3) │ 复用一条 TCP │
  goroutine-3 ─────→│ Stream(id=5) │              │
                    │      │                      │
                    │      └── transport.Conn ──────→ TCP socket
                    │             ▲                │
                    │    readLoop │ 单 goroutine   │
                    │    demux by StreamID         │
                    └──────────────────────────────┘
```

## Stream 生命周期

```
                    Client                  Server
                       │                      │
  Open() ────────────→ │                      │
                       │──── StreamData ─────→│ ─→ Accept()
                       │                      │
                       │←─── StreamData ──────│
                       │←─── StreamData ──────│
                       │                      │
  Close() ───────────→ │──── StreamEnd ──────→│ state → HalfClosedRemote
  state → HalfClosedLocal                     │
                       │                      │
                       │←─── StreamEnd ──────│ Close()
  state → Closed       │                      │ state → Closed
                       │                      │
```

## StreamID 分配

借鉴 HTTP/2:
- 客户端发起的流: 奇数 ID (1, 3, 5, ...)
- 服务端发起的流: 偶数 ID (2, 4, 6, ...)
- StreamID 0 保留给控制帧 (心跳等)

## 关键设计点

### 1. 单读 goroutine 分发

```
readLoop() {
    for {
        frame := tconn.ReceiveFrame()
        stream := streams[frame.StreamID]
        stream.handleFrame(frame)
    }
}
```

### 2. Stream 状态机

```
          Open
         /    \
    LocalEnd  RemoteEnd
         \    /
         Closed
```

### 3. 缓冲区

每个 Stream 有独立的接收缓冲区 (buffered channel, 默认 64 帧):

```go
type Stream struct {
    recvCh chan []byte  // 缓冲 64 帧
}
```

### 4. 流量控制

当前版本: 无流控 (依赖 buffered channel 做背压)
后续版本: credit-based flow control (参考 HTTP/2 WINDOW_UPDATE)

## 与 gRPC 对比

| 特性 | gRPC (HTTP/2) | lrpc mux |
|------|---------------|----------|
| 多路复用 | HTTP/2 stream | StreamID + frame 分发 |
| Stream 创建 | HEADERS frame | client: Open(), server: Accept() |
| Half-close | END_STREAM flag | StreamEnd frame |
| 流量控制 | WINDOW_UPDATE frame | 后续版本 |
| 优先级 | PRIORITY frame | 未实现 |
| 连接级流控 | SETTINGS frame | 未实现 |
