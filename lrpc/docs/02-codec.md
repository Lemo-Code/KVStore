# 02 — Codec 序列化抽象层

## 设计动机

gRPC 的 Codec 接口非常简单:

```go
// gRPC codec 接口 (简化)
type Codec interface {
    Marshal(v any) ([]byte, error)
    Unmarshal(data []byte, v any) error
    Name() string
}
```

gRPC codec 只序列化 **业务负载** (protobuf message)。
方法名通过 HTTP/2 `:path` 伪头部传递，请求 ID 通过 HTTP/2 Stream ID 隐式关联。

**lrpc 的挑战**: 我们没有 HTTP/2 层，需要在帧负载中自行编码元信息。

## 两层序列化设计

```
Frame Payload = codec.Encode(Envelope{
    ID:      uint64    ← 请求/响应匹配
    Method:  string    ← 服务方法寻址
    Error:   string    ← 错误信息
    Payload: []byte    ← codec.Encode(业务数据)
})
```

### Envelope 结构

```go
type Envelope struct {
    ID      uint64 `json:"id"`              // 请求序列号
    Method  string `json:"method"`           // "Service.Method"
    Error   string `json:"error,omitempty"`  // 错误信息 (响应)
    Payload []byte `json:"payload,omitempty"` // 序列化的业务数据
}
```

### 调用流程

```
客户端调用 Arith.Add(1, 2):
  1. codec.Marshal(Args{A:1, B:2}) → argsBytes
  2. NewRequest(seqId=42, method="Arith.Add", payload=argsBytes)
  3. codec.Marshal(envelope) → framePayload
  4. Frame{Type: UnaryRequest, Payload: framePayload}
  5. 发送到服务端

服务端处理:
  1. 接收 Frame, 提取 Payload
  2. codec.Unmarshal(framePayload) → Envelope{ID:42, Method:"Arith.Add", Payload: argsBytes}
  3. 查找已注册的 Arith.Add 方法
  4. codec.Unmarshal(argsBytes) → Args{A:1, B:2}
  5. handler(args) → 3, nil
  6. codec.Marshal(3) → replyBytes
  7. NewResponse(id=42, payload=replyBytes)
  8. codec.Marshal(envelope) → framePayload
  9. Frame{Type: UnaryResponse, Payload: framePayload}
  10. 发送回客户端
```

## 已实现的编解码器

| 编解码器 | 名称 | 适用场景 |
|----------|------|----------|
| JsonCodec | "json" | 调试、跨语言、简单场景 |
| ProtoCodec | "proto" | 生产环境、高性能 |

## 注册机制

```go
var registry = make(map[string]Codec)

func Register(c Codec) {
    registry[c.Name()] = c
}

func Get(name string) Codec {
    return registry[name]
}
```

编解码器在 `init()` 中自注册，类似 `database/sql` 驱动的注册模式。

## 与 gRPC/Go-RPC 的对比

| 特性 | gRPC | Go net/rpc | lrpc |
|------|------|------------|------|
| 编解码器 | Codec 接口 | 默认 gob | Codec 接口 |
| 方法名位置 | HTTP/2 :path | Request 结构体 | Envelope.Method |
| 请求 ID | HTTP/2 Stream ID | Request.Seq | Envelope.ID |
| 可插拔序列化 | 是 (protobuf 默认) | 否 (gob 硬编码) | 是 |
| 错误传递 | Trailers status | Response.Error | Envelope.Error |
