package lrpc

import (
	"fmt"
	"io"
	"log"
	"sync"
	"sync/atomic"
	"time"

	"lrpc/codec"
	"lrpc/interceptor"
	"lrpc/proto"
	"lrpc/transport"
)

// Call 代表一个进行中的 RPC 调用。
// 客户端发起调用后，等待服务端响应时使用此结构。
type Call struct {
	// Seq 请求序列号
	Seq uint64

	// ServiceMethod 被调用的服务方法名
	ServiceMethod string

	// Reply 用于接收响应数据的对象指针
	Reply any

	// Error 调用错误 (如果发生)
	Error error

	// Done 完成信号通道 — 响应到达或超时时关闭
	Done chan *Call
}

// done 标记调用完成
func (call *Call) done() {
	select {
	case call.Done <- call:
	default:
	}
}

// Client RPC 客户端
//
// 设计:
//   - 单连接 + 后台读 goroutine 读取响应帧
//   - 每个 Call 分配递增的序列号 (Seq)
//   - pending map 存储未完成的调用, 响应到达时通过 Seq 匹配
//   - 支持并发调用 (多个 goroutine 同时调用 Client.Call)
//
// 使用示例:
//
//	client, _ := lrpc.Dial("localhost:8080")
//	var reply int
//	err := client.Call("Arith.Add", &Args{A:1, B:2}, &reply)
//	client.Close()
type Client struct {
	codecName string
	tconn     *transport.Conn

	seq    uint64            // 请求序列号生成器 (原子递增)
	mu     sync.Mutex        // 保护 pending map
	pending map[uint64]*Call // 未完成的调用

	closing bool   // 是否正在关闭
	shutdown bool  // 读循环是否已退出

	// interceptors 客户端拦截器链
	interceptors []interceptor.UnaryClientInterceptor
}

// Dial 连接 RPC 服务器
func Dial(address string) (*Client, error) {
	tconn, err := transport.Dial(address, 5*time.Second)
	if err != nil {
		return nil, fmt.Errorf("lrpc: dial: %w", err)
	}

	client := &Client{
		codecName: codec.DefaultCodec,
		tconn:     tconn,
		pending:   make(map[uint64]*Call),
	}

	go client.readLoop()
	log.Printf("[lrpc] client connected to %s", address)
	return client, nil
}

// SetCodec 设置编解码器
func (c *Client) SetCodec(name string) {
	c.codecName = name
}

// Use 注册客户端拦截器
// 拦截器按注册顺序执行
func (c *Client) Use(i interceptor.UnaryClientInterceptor) {
	c.interceptors = append(c.interceptors, i)
}

// Call 发起远程调用 (阻塞)
// serviceMethod: 服务和方法, 如 "Arith.Add"
// args: 请求参数 (必须和注册的 args 类型匹配)
// reply: 返回值接收对象 (指针)
func (c *Client) Call(serviceMethod string, args, reply any) error {
	call := <-c.Go(serviceMethod, args, reply, make(chan *Call, 1)).Done
	return call.Error
}

// Go 异步发起调用，返回 Call 对象
// 用户可以从 Call.Done channel 等待结果
//
// 拦截器链在此构建和调用:
//   1. 构造基础 invoker (send + wait 的完整逻辑)
//   2. 用拦截器链包裹 invoker
//   3. 在 goroutine 中执行链
func (c *Client) Go(serviceMethod string, args, reply any, done chan *Call) *Call {
	if done == nil {
		done = make(chan *Call, 1)
	}

	call := &Call{
		Seq:           atomic.AddUint64(&c.seq, 1),
		ServiceMethod: serviceMethod,
		Reply:         reply,
		Done:          done,
	}

	// 构建基础 invoker: 序列化 → 注册 → 发送 → 等待响应 → 反序列化
	baseInvoker := func(method string, req, resp any) error {
		codecInst := codec.Get(c.codecName)
		if codecInst == nil {
			return fmt.Errorf("lrpc: codec %q not found", c.codecName)
		}

		// 序列化参数
		argsBytes, err := codecInst.Marshal(req)
		if err != nil {
			return fmt.Errorf("lrpc: marshal args: %w", err)
		}

		// 构建请求信封
		env := codec.NewRequest(call.Seq, method, argsBytes)
		envBytes, err := codecInst.Marshal(env)
		if err != nil {
			return fmt.Errorf("lrpc: marshal envelope: %w", err)
		}

		// 注册到 pending map
		c.mu.Lock()
		if c.closing {
			c.mu.Unlock()
			return fmt.Errorf("lrpc: client is closing")
		}
		c.pending[call.Seq] = call
		c.mu.Unlock()

		// 发送请求帧
		frame := proto.NewFrame(proto.MsgUnaryRequest, 0, envBytes)
		if err := c.tconn.SendFrame(frame); err != nil {
			c.mu.Lock()
			delete(c.pending, call.Seq)
			c.mu.Unlock()
			return fmt.Errorf("lrpc: send request: %w", err)
		}

		// 等待响应 (阻塞直到 readLoop 收到响应)
		result := <-call.Done
		return result.Error
	}

	// 用拦截器链包裹基础 invoker
	invoker := interceptor.ChainUnaryClient(c.interceptors, baseInvoker)

	// 异步执行
	go func() {
		call.Error = invoker(serviceMethod, args, reply)
		// 如果拦截器链已完成 (可能是同步错误), 确保 done 被调用
		// 如果是正常路径, baseInvoker 里已经调用了 call.done()
		// 这里防止 baseInvoker 因错误提前返回而没调 done 的情况
		select {
		case <-call.Done:
		default:
			call.done()
		}
	}()

	return call
}

// readLoop 后台读取响应的 goroutine
// 读到的帧按 MsgType 分发:
//   - UnaryResponse: 提取 Envelope, 匹配 pending Call
//   - ErrorResponse: 提取错误, 匹配 pending Call
//   - Heartbeat: 回复 HeartbeatAck
func (c *Client) readLoop() {
	codecInst := codec.Get(c.codecName)
	if codecInst == nil {
		log.Printf("[lrpc] client readLoop: codec %q not found", c.codecName)
		return
	}

	for {
		frame, err := c.tconn.ReceiveFrame()
		if err != nil {
			if err == io.EOF {
				log.Printf("[lrpc] client: server closed connection")
			} else {
				log.Printf("[lrpc] client read error: %v", err)
			}
			c.terminatePending(fmt.Errorf("connection error: %w", err))
			return
		}

		switch frame.Header.MsgType {
		case proto.MsgUnaryResponse:
			c.handleResponse(frame, codecInst)
		case proto.MsgErrorResponse:
			c.handleError(frame, codecInst)
		case proto.MsgHeartbeat:
			// 回复心跳
			if err := c.tconn.SendHeartbeatAck(); err != nil {
				log.Printf("[lrpc] client send heartbeat ack error: %v", err)
			}
		default:
			log.Printf("[lrpc] client unexpected message: %s", frame.Header.MsgType)
		}
	}
}

// handleResponse 处理一元响应
func (c *Client) handleResponse(frame *proto.Frame, cd codec.Codec) {
	var env codec.Envelope
	if err := cd.Unmarshal(frame.Payload, &env); err != nil {
		log.Printf("[lrpc] client decode response envelope: %v", err)
		return
	}

	c.mu.Lock()
	call := c.pending[env.ID]
	delete(c.pending, env.ID)
	c.mu.Unlock()

	if call == nil {
		log.Printf("[lrpc] client: no pending call for seq=%d", env.ID)
		return
	}

	// 解码业务数据
	if call.Reply != nil && len(env.Payload) > 0 {
		if err := cd.Unmarshal(env.Payload, call.Reply); err != nil {
			call.Error = fmt.Errorf("lrpc: decode reply: %w", err)
		}
	}

	call.done()
}

// handleError 处理错误响应
func (c *Client) handleError(frame *proto.Frame, cd codec.Codec) {
	var env codec.Envelope
	if err := cd.Unmarshal(frame.Payload, &env); err != nil {
		log.Printf("[lrpc] client decode error envelope: %v", err)
		return
	}

	c.mu.Lock()
	call := c.pending[env.ID]
	delete(c.pending, env.ID)
	c.mu.Unlock()

	if call == nil {
		log.Printf("[lrpc] client: no pending call for error seq=%d", env.ID)
		return
	}

	call.Error = env.GetError()
	call.done()
}

// terminatePending 终止所有未完成的调用 (用于连接断开时)
func (c *Client) terminatePending(err error) {
	c.mu.Lock()
	defer c.mu.Unlock()

	for _, call := range c.pending {
		call.Error = err
		call.done()
	}
	c.pending = make(map[uint64]*Call)
	c.shutdown = true
}

// Close 关闭客户端
func (c *Client) Close() error {
	c.mu.Lock()
	c.closing = true
	c.mu.Unlock()

	return c.tconn.Close()
}
