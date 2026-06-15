// Package mux 实现单连接多流复用 (Connection Multiplexing)。
//
// 设计动机:
//
//	gRPC 依赖 HTTP/2 的流多路复用能力，使得多个 RPC 调用可以共享一条 TCP 连接。
//	每个 HTTP/2 stream 独立收发数据，互不阻塞。
//
//	lrpc mux 层直接基于自定义协议帧实现类似能力:
//	  - 一个 MuxConn 管理一个 transport.Conn
//	  - 在同一个 TCP 连接上创建多个 Stream
//	  - 每个 Stream 有独立的 StreamID (帧头中的 4 字节)
//	  - MuxConn 的后台读 goroutine 按 StreamID 分发帧到对应的 Stream
//
// 使用示例:
//
//	// 服务端
//	mc := mux.NewMuxConn(tconn, true)   // true = server side
//	for {
//	    stream, _ := mc.Accept()         // 接受客户端发起的 stream
//	    go handleStream(stream)
//	}
//
//	// 客户端
//	mc := mux.NewMuxConn(tconn, false)  // false = client side
//	stream, _ := mc.Open()              // 发起新 stream
//	stream.Write(data)
//	resp, _ := stream.Read()
//	stream.Close()
package mux

import (
	"fmt"
	"io"
	"log"
	"sync"
	"sync/atomic"

	"lrpc/proto"
	"lrpc/transport"
)

// streamState 流状态
type streamState int32

const (
	stateOpen            streamState = iota // 双向开放
	stateHalfClosedLocal                    // 本地已发送 StreamEnd
	stateHalfClosedRemote                   // 远端已发送 StreamEnd
	stateClosed                             // 双向关闭
)

// Stream 代表一个多路复用流
//
// 设计参考 HTTP/2 stream:
//   - 全双工: 读写独立，不阻塞对方
//   - Half-close: 一端可发送 StreamEnd 标记自己不再写
//   - 有序交付: 同一 stream 内数据有序
//   - 独立流控: 每 stream 有独立接收缓冲 (后续版本加入流量控制)
type Stream struct {
	id    uint32
	mux   *MuxConn
	state streamState

	// recvCh 接收缓冲区 — 存放从网络读到的数据帧 (Payload 部分)
	recvCh chan []byte

	// closeNotify 通知远端已关闭
	closeNotify chan struct{}

	// recvErr 接收错误 (远端发送了 ErrorResponse 或 StreamEnd)
	recvErr   error
	recvErrMu sync.Mutex
}

// ID 返回流 ID
func (s *Stream) ID() uint32 { return s.id }

// Read 从流中读取数据 (阻塞直到有数据或流关闭)
func (s *Stream) Read() ([]byte, error) {
	select {
	case data, ok := <-s.recvCh:
		if !ok {
			// channel 已关闭，返回接收错误
			s.recvErrMu.Lock()
			err := s.recvErr
			s.recvErrMu.Unlock()
			if err != nil {
				return nil, err
			}
			return nil, io.EOF
		}
		return data, nil
	case <-s.closeNotify:
		return nil, io.EOF
	}
}

// Write 向流中写入数据
func (s *Stream) Write(data []byte) error {
	if atomic.LoadInt32((*int32)(&s.state)) >= int32(stateHalfClosedLocal) {
		return fmt.Errorf("mux: stream %d is closed for writing", s.id)
	}

	frame := proto.NewFrame(proto.MsgStreamData, s.id, data)
	return s.mux.tconn.SendFrame(frame)
}

// Close 关闭流的写端 (发送 StreamEnd)
// 读端仍然可以接收数据，直到远端也关闭
func (s *Stream) Close() error {
	for {
		old := streamState(atomic.LoadInt32((*int32)(&s.state)))
		var newState streamState
		switch old {
		case stateOpen:
			newState = stateHalfClosedLocal
		case stateHalfClosedRemote:
			newState = stateClosed
		default:
			return nil // 已经关闭
		}
		if atomic.CompareAndSwapInt32((*int32)(&s.state), int32(old), int32(newState)) {
			break
		}
	}

	// 发送 StreamEnd 帧
	err := s.mux.tconn.SendFrame(proto.NewFrame(proto.MsgStreamEnd, s.id, nil))
	if err != nil {
		log.Printf("[mux] stream %d send StreamEnd error: %v", s.id, err)
	}

	// 如果双向都关闭了，从 mux 中移除
	if atomic.LoadInt32((*int32)(&s.state)) == int32(stateClosed) {
		s.mux.removeStream(s.id)
	}

	return err
}

// handleFrame 处理到达此 stream 的帧 (由 MuxConn 的 readLoop 调用)
func (s *Stream) handleFrame(frame *proto.Frame) {
	switch frame.Header.MsgType {
	case proto.MsgStreamData:
		select {
		case s.recvCh <- frame.Payload:
		default:
			log.Printf("[mux] stream %d recv buffer full, dropping frame", s.id)
		}
	case proto.MsgStreamEnd:
		s.handleRemoteClose(nil)
	case proto.MsgErrorResponse:
		s.handleRemoteClose(fmt.Errorf("stream error: %s", string(frame.Payload)))
	}
}

// handleRemoteClose 处理远端关闭
func (s *Stream) handleRemoteClose(err error) {
	for {
		old := streamState(atomic.LoadInt32((*int32)(&s.state)))
		var newState streamState
		switch old {
		case stateOpen:
			newState = stateHalfClosedRemote
		case stateHalfClosedLocal:
			newState = stateClosed
		default:
			return // 已经关闭
		}
		if atomic.CompareAndSwapInt32((*int32)(&s.state), int32(old), int32(newState)) {
			break
		}
	}

	s.recvErrMu.Lock()
	s.recvErr = err
	s.recvErrMu.Unlock()

	// 通知 Read() 调用者流已关闭
	close(s.recvCh)

	if atomic.LoadInt32((*int32)(&s.state)) == int32(stateClosed) {
		s.mux.removeStream(s.id)
	}
}

// MuxConn 多路复用连接
//
//	    ┌──────────────────────────────┐
//	    │         MuxConn              │
//	    │  ┌────────┐  ┌────────┐      │
//	    │  │Stream 1│  │Stream 3│ ...  │
//	    │  └────────┘  └────────┘      │
//	    │  ┌──────────────────────┐    │
//	    │  │   transport.Conn     │    │  ← 一条 TCP 连接
//	    │  └──────────────────────┘    │
//	    └──────────────────────────────┘
type MuxConn struct {
	tconn   *transport.Conn
	isServer bool // true: 服务端 (生成偶数 StreamID), false: 客户端 (生成奇数 StreamID)

	mu      sync.RWMutex
	streams map[uint32]*Stream

	// acceptCh 服务端接受新 stream 的 channel
	acceptCh chan *Stream

	// nextStreamID 下一个可用的 stream ID 计数器
	nextStreamID uint32

	closed bool
}

// NewMuxConn 创建多路复用连接
// isServer: true 表示服务端 (Accept 客户端发起的流), false 表示客户端 (Open 新流)
func NewMuxConn(tconn *transport.Conn, isServer bool) *MuxConn {
	mc := &MuxConn{
		tconn:    tconn,
		isServer: isServer,
		streams:  make(map[uint32]*Stream),
		acceptCh: make(chan *Stream, 16),
	}

	if isServer {
		// 服务端的第一个流出 ID 从 2 开始 (偶数)
		mc.nextStreamID = 2
	} else {
		// 客户端的第一个流出 ID 从 1 开始 (奇数)
		mc.nextStreamID = 1
	}

	// 启动后台读循环
	go mc.readLoop()

	return mc
}

// Accept 接受一个新的来自客户端的流 (仅服务端使用)
// 阻塞直到有新流到达
func (mc *MuxConn) Accept() (*Stream, error) {
	stream, ok := <-mc.acceptCh
	if !ok {
		return nil, fmt.Errorf("mux: connection closed")
	}
	return stream, nil
}

// Open 创建一个新的流向服务端 (仅客户端使用)
func (mc *MuxConn) Open() (*Stream, error) {
	mc.mu.Lock()
	defer mc.mu.Unlock()

	if mc.closed {
		return nil, fmt.Errorf("mux: connection closed")
	}

	id := mc.allocateStreamID()
	stream := mc.newStream(id)
	mc.streams[id] = stream

	log.Printf("[mux] opened stream %d (client side)", id)
	return stream, nil
}

// allocateStreamID 分配下一个可用的 stream ID
func (mc *MuxConn) allocateStreamID() uint32 {
	id := mc.nextStreamID
	mc.nextStreamID += 2 // 同方向跳过 2 (保留奇偶性)
	return id
}

// newStream 创建新的 stream 实例
func (mc *MuxConn) newStream(id uint32) *Stream {
	return &Stream{
		id:          id,
		mux:         mc,
		state:       stateOpen,
		recvCh:      make(chan []byte, 64), // 缓冲 64 帧
		closeNotify: make(chan struct{}),
	}
}

// removeStream 从 mux 中移除 stream
func (mc *MuxConn) removeStream(id uint32) {
	mc.mu.Lock()
	defer mc.mu.Unlock()
	delete(mc.streams, id)
	log.Printf("[mux] stream %d fully closed and removed", id)
}

// readLoop 后台读取帧并按 StreamID 分发
// 这是 MuxConn 的核心: 单 goroutine 读 → 多 stream 分发
func (mc *MuxConn) readLoop() {
	for {
		frame, err := mc.tconn.ReceiveFrame()
		if err != nil {
			log.Printf("[mux] read error: %v", err)
			mc.closeAll()
			return
		}

		streamID := frame.Header.StreamID
		if streamID == 0 {
			// StreamID 0 是控制帧 (如心跳)
			mc.handleControlFrame(frame)
			continue
		}

		mc.mu.RLock()
		stream := mc.streams[streamID]
		mc.mu.RUnlock()

		if stream == nil {
			// 新 stream — 只有服务端接受客户端发起的流
			if mc.isServer {
				stream = mc.acceptNewStream(streamID)
				if stream == nil {
					continue // acceptNewStream handles logging
				}
			} else {
				log.Printf("[mux] unexpected stream %d (client side only opens streams)", streamID)
				continue
			}
		}

		stream.handleFrame(frame)
	}
}

// acceptNewStream 服务端接受新的客户端发起的流
func (mc *MuxConn) acceptNewStream(streamID uint32) *Stream {
	mc.mu.Lock()
	defer mc.mu.Unlock()

	if mc.closed {
		return nil
	}

	stream := mc.newStream(streamID)
	mc.streams[streamID] = stream

	log.Printf("[mux] accepted stream %d (server side)", streamID)

	// 非阻塞地发送到 acceptCh
	select {
	case mc.acceptCh <- stream:
	default:
		log.Printf("[mux] accept buffer full for stream %d", streamID)
	}

	return stream
}

// handleControlFrame 处理控制帧 (StreamID=0)
func (mc *MuxConn) handleControlFrame(frame *proto.Frame) {
	switch frame.Header.MsgType {
	case proto.MsgHeartbeat:
		mc.tconn.SendHeartbeatAck()
	case proto.MsgHeartbeatAck:
		// 收到心跳回复
	default:
		log.Printf("[mux] unexpected control frame: %s", frame.Header.MsgType)
	}
}

// closeAll 关闭所有 stream (连接断开时)
func (mc *MuxConn) closeAll() {
	mc.mu.Lock()
	defer mc.mu.Unlock()

	mc.closed = true
	close(mc.acceptCh)

	for _, stream := range mc.streams {
		close(stream.recvCh)
		close(stream.closeNotify)
	}
	mc.streams = make(map[uint32]*Stream)
}

// Close 关闭 MuxConn 和底层连接
func (mc *MuxConn) Close() error {
	mc.closeAll()
	return mc.tconn.Close()
}

// RemoteAddr 返回远端地址
func (mc *MuxConn) RemoteAddr() string {
	return mc.tconn.RemoteAddr().String()
}
