// Package proto 定义 lrpc 的二进制协议帧格式。
//
// 帧结构 (12 bytes 固定头 + 可变 payload):
//
//	┌──────────────────────────────────────────────────────┐
//	│  0-1:  Magic       (2B) = 0x4C52 ('LR' in ASCII)   │
//	│    2:  Version     (1B) = 0x01                     │
//	│    3:  MsgType     (1B)                            │
//	│  4-7:  StreamID    (4B) uint32 big-endian          │
//	│ 8-11:  PayloadLen  (4B) uint32 big-endian          │
//	├──────────────────────────────────────────────────────┤
//	│ 12+:   Payload     (PayloadLen bytes)              │
//	└──────────────────────────────────────────────────────┘
//
// 消息类型:
//   - UnaryRequest  (0x01): 一元 RPC 请求
//   - UnaryResponse (0x02): 一元 RPC 响应
//   - ErrorResponse (0x03): 错误响应
//   - StreamData    (0x04): 流式数据帧 (client/server/bidi 通用)
//   - StreamEnd     (0x05): 流结束标记
//   - Heartbeat     (0x06): 心跳请求
//   - HeartbeatAck  (0x07): 心跳响应
package proto

import (
	"encoding/binary"
	"fmt"
	"io"
)

// 帧协议常量
const (
	// MagicNumber 是 lrpc 协议的魔数, 用于校验和区分协议
	MagicNumber uint16 = 0x4C52

	// Version1 当前协议版本
	Version1 byte = 0x01

	// HeaderSize 固定帧头大小: 2+1+1+4+4 = 12 bytes
	HeaderSize = 12
)

// MsgType 消息类型枚举
type MsgType byte

const (
	MsgUnaryRequest  MsgType = 0x01 // 一元 RPC 请求
	MsgUnaryResponse MsgType = 0x02 // 一元 RPC 响应
	MsgErrorResponse MsgType = 0x03 // 错误响应 (带错误码和消息)
	MsgStreamData    MsgType = 0x04 // 流式数据帧
	MsgStreamEnd     MsgType = 0x05 // 流结束 (发送方完成)
	MsgHeartbeat     MsgType = 0x06 // 心跳 Ping
	MsgHeartbeatAck  MsgType = 0x07 // 心跳 Pong
)

// String 返回消息类型的可读名称
func (m MsgType) String() string {
	switch m {
	case MsgUnaryRequest:
		return "UnaryRequest"
	case MsgUnaryResponse:
		return "UnaryResponse"
	case MsgErrorResponse:
		return "ErrorResponse"
	case MsgStreamData:
		return "StreamData"
	case MsgStreamEnd:
		return "StreamEnd"
	case MsgHeartbeat:
		return "Heartbeat"
	case MsgHeartbeatAck:
		return "HeartbeatAck"
	default:
		return fmt.Sprintf("Unknown(0x%02X)", byte(m))
	}
}

// Frame 表示一个完整的协议帧
type Frame struct {
	Header FrameHeader
	Payload []byte // 原始负载数据
}

// FrameHeader 12 字节固定帧头
type FrameHeader struct {
	Magic      uint16  // 魔数 0x4C52
	Version    byte    // 协议版本
	MsgType    MsgType // 消息类型
	StreamID   uint32  // 流 ID，一元 RPC 默认为 0
	PayloadLen uint32  // 负载长度
}

// NewFrame 创建新的协议帧
// streamID: 流标识符 (一元 RPC 为 0)
// msgType: 消息类型
// payload: 负载数据
func NewFrame(msgType MsgType, streamID uint32, payload []byte) *Frame {
	return &Frame{
		Header: FrameHeader{
			Magic:      MagicNumber,
			Version:    Version1,
			MsgType:    msgType,
			StreamID:   streamID,
			PayloadLen: uint32(len(payload)),
		},
		Payload: payload,
	}
}

// Encode 将帧编码为字节切片, 用于发送
func (f *Frame) Encode() []byte {
	buf := make([]byte, HeaderSize+len(f.Payload))

	binary.BigEndian.PutUint16(buf[0:2], f.Header.Magic)
	buf[2] = f.Header.Version
	buf[3] = byte(f.Header.MsgType)
	binary.BigEndian.PutUint32(buf[4:8], f.Header.StreamID)
	binary.BigEndian.PutUint32(buf[8:12], f.Header.PayloadLen)

	copy(buf[12:], f.Payload)
	return buf
}

// WriteTo 将帧写入 io.Writer, 减少内存拷贝
func (f *Frame) WriteTo(w io.Writer) (int64, error) {
	// 先写固定头
	header := make([]byte, HeaderSize)
	binary.BigEndian.PutUint16(header[0:2], f.Header.Magic)
	header[2] = f.Header.Version
	header[3] = byte(f.Header.MsgType)
	binary.BigEndian.PutUint32(header[4:8], f.Header.StreamID)
	binary.BigEndian.PutUint32(header[8:12], f.Header.PayloadLen)

	n, err := w.Write(header)
	if err != nil {
		return int64(n), err
	}
	m, err := w.Write(f.Payload)
	return int64(n + m), err
}

// ValidateHeader 校验帧头合法性
func (h *FrameHeader) Validate() error {
	if h.Magic != MagicNumber {
		return fmt.Errorf("proto: invalid magic number 0x%04X, expected 0x%04X", h.Magic, MagicNumber)
	}
	if h.Version != Version1 {
		return fmt.Errorf("proto: unsupported version %d, expected %d", h.Version, Version1)
	}
	if h.MsgType < MsgUnaryRequest || h.MsgType > MsgHeartbeatAck {
		return fmt.Errorf("proto: unknown message type 0x%02X", byte(h.MsgType))
	}
	return nil
}

// 可选的负载最大长度 (防止内存耗尽)
const maxPayloadSize = 16 * 1024 * 1024 // 16 MiB

// ReadFrame 从 io.Reader 读取一个完整帧
func ReadFrame(r io.Reader) (*Frame, error) {
	header := make([]byte, HeaderSize)
	if _, err := io.ReadFull(r, header); err != nil {
		return nil, fmt.Errorf("proto: read header: %w", err)
	}

	fh := FrameHeader{
		Magic:      binary.BigEndian.Uint16(header[0:2]),
		Version:    header[2],
		MsgType:    MsgType(header[3]),
		StreamID:   binary.BigEndian.Uint32(header[4:8]),
		PayloadLen: binary.BigEndian.Uint32(header[8:12]),
	}

	if err := fh.Validate(); err != nil {
		return nil, err
	}

	if fh.PayloadLen > maxPayloadSize {
		return nil, fmt.Errorf("proto: payload too large: %d bytes (max %d)", fh.PayloadLen, maxPayloadSize)
	}

	payload := make([]byte, fh.PayloadLen)
	if fh.PayloadLen > 0 {
		if _, err := io.ReadFull(r, payload); err != nil {
			return nil, fmt.Errorf("proto: read payload: %w", err)
		}
	}

	return &Frame{Header: fh, Payload: payload}, nil
}
