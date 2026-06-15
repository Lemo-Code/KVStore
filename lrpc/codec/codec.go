// Package codec 定义序列化抽象层。
//
// lrpc 的序列化分两层:
//
//  1. Envelope (信封): 承载 RPC 元信息 (方法名, 请求ID, 错误信息)
//     这一层由 lrpc 框架自己处理，对用户透明。
//
//  2. Payload (业务负载): 用户的方法参数和返回值
//     用户通过 Codec 接口序列化/反序列化。
//
// 数据流:
//
//	Client                          Server
//	  │                                │
//	  │  EncodeRequest(env)           │
//	  │  → Frame{Payload: jsonBytes}  │
//	  │                                │
//	  │───────────────────────────────→│
//	  │                                │  DecodeRequest(frame.Payload)
//	  │                                │  → Envelope{Method: "Arith.Add"}
//	  │                                │  → 查找服务方法
//	  │                                │  → codec.Unmarshal(args)
//	  │                                │  → handler(args) → reply
//	  │                                │  ← codec.Marshal(reply)
//	  │                                │  ← EncodeResponse(env)
//	  │                                │
//	  │←───────────────────────────────│
//	  │                                │
//	  │  DecodeResponse(frame.Payload) │
//	  │  → codec.Unmarshal(reply)     │
//	  │                                │
//
// 设计参考:
//   - gRPC: codec 只负责业务负载，方法名在 HTTP/2 :path header
//   - Go net/rpc: 用 encoding/gob 序列化整个 Request/Response 结构体
//   - lrpc: codec 同时序列化 Envelope + Payload，不依赖 HTTP/2
package codec

import "fmt"

// Codec 序列化编解码器接口
// 每种序列化方式 (JSON, Protobuf, MessagePack 等) 实现此接口
type Codec interface {
	// Name 返回编解码器名称, 如 "json", "protobuf"
	Name() string

	// Marshal 将对象序列化为字节切片
	Marshal(v any) ([]byte, error)

	// Unmarshal 将字节切片反序列化为对象
	Unmarshal(data []byte, v any) error
}

// Envelope 是 RPC 调用的外层信封
// 封装了服务寻址和请求追踪信息, 不包含业务数据本身
type Envelope struct {
	// ID 请求序列号, 用于客户端匹配请求和响应
	// 一元 RPC 中，每次调用分配一个递增的 ID
	ID uint64 `json:"id"`

	// Method 服务名和方法名, 格式: "ServiceName.MethodName"
	// 例如: "Arith.Add", "UserService.GetUser"
	Method string `json:"method"`

	// Error 错误信息, 仅响应帧填充
	// 空字符串表示成功
	Error string `json:"error,omitempty"`

	// Payload 序列化后的业务数据
	//   - 请求时: 序列化后的方法参数
	//   - 响应时: 序列化后的返回值
	Payload []byte `json:"payload,omitempty"`
}

// NewRequest 创建请求信封
func NewRequest(id uint64, method string, payload []byte) *Envelope {
	return &Envelope{
		ID:      id,
		Method:  method,
		Payload: payload,
	}
}

// NewResponse 创建成功响应信封
func NewResponse(id uint64, payload []byte) *Envelope {
	return &Envelope{
		ID:      id,
		Payload: payload,
	}
}

// NewErrorResponse 创建错误响应信封
func NewErrorResponse(id uint64, err error) *Envelope {
	return &Envelope{
		ID:    id,
		Error: err.Error(),
	}
}

// IsError 检查是否为错误响应
func (e *Envelope) IsError() bool {
	return e.Error != ""
}

// GetError 获取错误信息
func (e *Envelope) GetError() error {
	if e.Error == "" {
		return nil
	}
	return fmt.Errorf("%s", e.Error)
}
