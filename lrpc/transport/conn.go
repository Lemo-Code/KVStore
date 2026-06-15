// Package transport 管理 TCP 连接和帧的读写。
//
// 设计原则:
//   - 写操作加锁 (多个 goroutine 可能同时写同一连接)
//   - 读操作不加锁 (每个连接只有一个读 goroutine)
//   - 支持读写超时
//   - 内建心跳检测
package transport

import (
	"crypto/rand"
	"encoding/binary"
	"fmt"
	"io"
	"net"
	"sync"
	"time"

	"lrpc/proto"
)

// Conn 封装一个 TCP 连接，提供帧级别的读写。
// 安全用于并发写 (内部加锁), 不支持并发读。
type Conn struct {
	conn   net.Conn
	mu     sync.Mutex // 保护写操作
	closed bool
}

// NewConn 包装一个已有的 net.Conn
func NewConn(conn net.Conn) *Conn {
	return &Conn{conn: conn}
}

// Dial 连接到远程地址，返回包装后的 Conn
func Dial(address string, timeout time.Duration) (*Conn, error) {
	conn, err := net.DialTimeout("tcp", address, timeout)
	if err != nil {
		return nil, fmt.Errorf("transport: dial %s: %w", address, err)
	}
	return NewConn(conn), nil
}

// LocalAddr 返回本地地址
func (c *Conn) LocalAddr() net.Addr {
	return c.conn.LocalAddr()
}

// RemoteAddr 返回远端地址
func (c *Conn) RemoteAddr() net.Addr {
	return c.conn.RemoteAddr()
}

// Close 关闭连接
func (c *Conn) Close() error {
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.closed {
		return nil
	}
	c.closed = true
	return c.conn.Close()
}

// IsClosed 检查连接是否已关闭
func (c *Conn) IsClosed() bool {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.closed
}

// SetDeadline 设置读写截止时间
func (c *Conn) SetDeadline(t time.Time) error {
	return c.conn.SetDeadline(t)
}

// SetReadDeadline 设置读截止时间
func (c *Conn) SetReadDeadline(t time.Time) error {
	return c.conn.SetReadDeadline(t)
}

// SetWriteDeadline 设置写截止时间
func (c *Conn) SetWriteDeadline(t time.Time) error {
	return c.conn.SetWriteDeadline(t)
}

// SendFrame 发送一个帧 (线程安全)
func (c *Conn) SendFrame(f *proto.Frame) error {
	c.mu.Lock()
	defer c.mu.Unlock()

	if c.closed {
		return fmt.Errorf("transport: connection closed")
	}

	_, err := f.WriteTo(c.conn)
	return err
}

// ReceiveFrame 读取一个帧 (非线程安全, 仅从读 goroutine 调用)
func (c *Conn) ReceiveFrame() (*proto.Frame, error) {
	frame, err := proto.ReadFrame(c.conn)
	if err != nil {
		if err == io.EOF {
			return nil, err
		}
		return nil, fmt.Errorf("transport: receive frame: %w", err)
	}
	return frame, nil
}

// SendHeartbeat 发送心跳帧
func (c *Conn) SendHeartbeat() error {
	return c.SendFrame(proto.NewFrame(proto.MsgHeartbeat, 0, nil))
}

// SendHeartbeatAck 发送心跳响应
func (c *Conn) SendHeartbeatAck() error {
	return c.SendFrame(proto.NewFrame(proto.MsgHeartbeatAck, 0, nil))
}

// 创建新的 Stream ID
// 客户端生成奇数, 服务端生成偶数
// 参考 HTTP/2 stream ID 分配规则

// NewClientStreamID 生成客户端发起的 stream ID (奇数)
func NewClientStreamID() uint32 {
	var buf [4]byte
	rand.Read(buf[:])
	id := binary.BigEndian.Uint32(buf[:])
	if id%2 == 0 {
		id++ // 确保是奇数
	}
	if id == 0 {
		id = 1
	}
	return id
}

// NewServerStreamID 生成服务端发起的 stream ID (偶数)
func NewServerStreamID() uint32 {
	var buf [4]byte
	rand.Read(buf[:])
	id := binary.BigEndian.Uint32(buf[:])
	if id%2 == 1 {
		id++ // 确保是偶数
	}
	if id == 0 {
		id = 2
	}
	return id
}
