// Package lrpc 提供 RPC Server 实现。
//
// Server 负责:
//   - 服务注册: 将用户定义的服务对象注册到方法表
//   - 连接管理: Accept TCP 连接, 每个连接启动独立读 goroutine
//   - 请求分发: 解析 Envelope → 反射调用 → 编码响应
//   - 并发处理: 每个请求启动新 goroutine (类似 gRPC 的 per-request goroutine)
//
// 使用示例:
//
//	s := lrpc.NewServer()
//	s.Register(&Arith{})
//	s.Serve(":8080")
package lrpc

import (
	"fmt"
	"io"
	"log"
	"net"
	"reflect"
	"strings"
	"sync"

	"lrpc/codec"
	"lrpc/interceptor"
	"lrpc/proto"
	"lrpc/transport"
)

// Server RPC 服务端
type Server struct {
	// codecName 编解码器名称
	codecName string

	// services 已注册的服务, key 为服务名
	services map[string]*service
	mu       sync.RWMutex

	// interceptors 服务端拦截器链
	interceptors []interceptor.UnaryServerInterceptor
}

// service 一个已注册的服务
type service struct {
	name    string                 // 服务名 (结构体类型名)
	rcvr    reflect.Value          // 接收者实例
	typ     reflect.Type           // 接收者类型
	methods map[string]*methodType // 方法表: 方法名 → 方法信息
}

// methodType RPC 方法的信息
type methodType struct {
	method    reflect.Method // 反射方法
	argsType  reflect.Type   // 第一个参数的类型 (请求参数)
	replyType reflect.Type   // 第二个参数的类型 (响应参数)
}

// NewServer 创建 Server 实例
func NewServer() *Server {
	return &Server{
		codecName: codec.DefaultCodec,
		services:  make(map[string]*service),
	}
}

// SetCodec 设置编解码器
func (s *Server) SetCodec(name string) {
	s.codecName = name
}

// Use 注册服务端拦截器
// 拦截器按注册顺序执行 (先注册的先包裹外层)
//
//	s.Use(interceptor.RecoveryInterceptor())   // 最外层: panic 恢复
//	s.Use(interceptor.LoggingInterceptor())    // 中间层: 日志
//	s.Use(authInterceptor)                     // 最内层: 鉴权
func (s *Server) Use(i interceptor.UnaryServerInterceptor) {
	s.interceptors = append(s.interceptors, i)
}

// Register 注册一个服务对象
// rcvr 必须是指针, 其所有导出方法如果符合签名约定则注册为 RPC 方法。
//
// 方法签名约定 (参考 Go net/rpc):
//   - 方法必须是导出的 (首字母大写)
//   - 方法有 3 个参数: (receiver, args *T, reply *U)
//   - 返回类型 error
//   - 方法签名: func (t *T) MethodName(args *ArgType, reply *ReplyType) error
func (s *Server) Register(rcvr any) error {
	s.mu.Lock()
	defer s.mu.Unlock()

	svc := newService(rcvr)
	if _, exists := s.services[svc.name]; exists {
		return fmt.Errorf("lrpc: service %q already registered", svc.name)
	}
	s.services[svc.name] = svc
	log.Printf("[lrpc] registered service: %s (%d methods)", svc.name, len(svc.methods))
	for name := range svc.methods {
		log.Printf("[lrpc]   - %s.%s", svc.name, name)
	}
	return nil
}

// newService 通过反射提取服务对象的方法表
func newService(rcvr any) *service {
	typ := reflect.TypeOf(rcvr)
	val := reflect.ValueOf(rcvr)

	s := &service{
		name:    reflect.Indirect(val).Type().Name(),
		rcvr:    val,
		typ:     typ,
		methods: make(map[string]*methodType),
	}

	for i := 0; i < typ.NumMethod(); i++ {
		method := typ.Method(i)
		mtype, err := parseMethod(method)
		if err != nil {
			// 跳过不符合签名的方法
			continue
		}
		s.methods[method.Name] = mtype
	}

	return s
}

// parseMethod 检查方法签名是否符合 RPC 约定
//
// 约定签名: func (t *T) MethodName(args *ArgType, reply *ReplyType) error
// 即: 3 个输入 (receiver, args, reply), 1 个输出 (error)
func parseMethod(method reflect.Method) (*methodType, error) {
	mtyp := method.Type

	// 需要 3 个参数: receiver + args + reply
	if mtyp.NumIn() != 3 {
		return nil, fmt.Errorf("lrpc: method %q has %d input params, need 3 (receiver, args, reply)", method.Name, mtyp.NumIn())
	}
	// 1 个返回值: error
	if mtyp.NumOut() != 1 {
		return nil, fmt.Errorf("lrpc: method %q has %d output params, need 1 (error)", method.Name, mtyp.NumOut())
	}

	// 第一参数 (args) 必须是指针
	argsType := mtyp.In(1)
	if argsType.Kind() != reflect.Ptr {
		return nil, fmt.Errorf("lrpc: method %q args type must be a pointer", method.Name)
	}

	// 第二参数 (reply) 必须是指针
	replyType := mtyp.In(2)
	if replyType.Kind() != reflect.Ptr {
		return nil, fmt.Errorf("lrpc: method %q reply type must be a pointer", method.Name)
	}

	// 返回值必须是 error
	if returnType := mtyp.Out(0); returnType != reflect.TypeOf((*error)(nil)).Elem() {
		return nil, fmt.Errorf("lrpc: method %q must return error", method.Name)
	}

	return &methodType{
		method:    method,
		argsType:  argsType,
		replyType: replyType,
	}, nil
}

// Serve 在给定地址启动服务
// 阻塞直到 Stop 被调用
func (s *Server) Serve(addr string) error {
	listener, err := net.Listen("tcp", addr)
	if err != nil {
		return fmt.Errorf("lrpc: listen %s: %w", addr, err)
	}
	defer listener.Close()

	log.Printf("[lrpc] server listening on %s", addr)
	return s.acceptLoop(listener)
}

// ServeListener 在已有的 listener 上启动服务
func (s *Server) ServeListener(lis net.Listener) error {
	log.Printf("[lrpc] server listening on %s", lis.Addr())
	return s.acceptLoop(lis)
}

// acceptLoop 接受连接的主循环
func (s *Server) acceptLoop(lis net.Listener) error {
	for {
		conn, err := lis.Accept()
		if err != nil {
			// 如果是临时错误则继续
			if ne, ok := err.(net.Error); ok && ne.Temporary() {
				log.Printf("[lrpc] temporary accept error: %v", err)
				continue
			}
			return fmt.Errorf("lrpc: accept: %w", err)
		}
		log.Printf("[lrpc] accepted connection from %s", conn.RemoteAddr())
		go s.handleConn(transport.NewConn(conn))
	}
}

// handleConn 处理单个连接的所有帧
func (s *Server) handleConn(tconn *transport.Conn) {
	defer tconn.Close()

	codecInst := codec.Get(s.codecName)
	if codecInst == nil {
		log.Printf("[lrpc] codec %q not found, closing connection", s.codecName)
		return
	}

	for {
		frame, err := tconn.ReceiveFrame()
		if err != nil {
			if err != io.EOF {
				log.Printf("[lrpc] read frame error from %s: %v", tconn.RemoteAddr(), err)
			}
			return
		}

		// 分发处理 (每个请求启动新 goroutine，服务端可以并发处理)
		switch frame.Header.MsgType {
		case proto.MsgUnaryRequest:
			go s.handleRequest(tconn, frame, codecInst)
		case proto.MsgHeartbeat:
			go func() {
				if err := tconn.SendHeartbeatAck(); err != nil {
					log.Printf("[lrpc] send heartbeat ack error: %v", err)
				}
			}()
		default:
			log.Printf("[lrpc] unexpected message type from %s: %s", tconn.RemoteAddr(), frame.Header.MsgType)
		}
	}
}

// handleRequest 处理一元 RPC 请求
func (s *Server) handleRequest(tconn *transport.Conn, frame *proto.Frame, c codec.Codec) {
	// 1. 解码信封
	var env codec.Envelope
	if err := c.Unmarshal(frame.Payload, &env); err != nil {
		s.sendError(tconn, 0, fmt.Errorf("decode envelope: %w", err), c)
		return
	}

	// 2. 解析服务名和方法名
	dot := strings.LastIndex(env.Method, ".")
	if dot < 0 {
		s.sendError(tconn, env.ID, fmt.Errorf("invalid method: %q (expected Service.Method)", env.Method), c)
		return
	}
	serviceName := env.Method[:dot]
	methodName := env.Method[dot+1:]

	// 3. 查找服务和对应方法
	s.mu.RLock()
	svc := s.services[serviceName]
	s.mu.RUnlock()

	if svc == nil {
		s.sendError(tconn, env.ID, fmt.Errorf("service %q not found", serviceName), c)
		return
	}

	mtype := svc.methods[methodName]
	if mtype == nil {
		s.sendError(tconn, env.ID, fmt.Errorf("method %q not found in service %q", methodName, serviceName), c)
		return
	}

	// 4. 构建参数和返回值实例
	args := reflect.New(mtype.argsType.Elem()).Interface()
	reply := reflect.New(mtype.replyType.Elem()).Interface()

	// 5. 解码请求参数
	if env.Payload != nil && len(env.Payload) > 0 {
		if err := c.Unmarshal(env.Payload, args); err != nil {
			s.sendError(tconn, env.ID, fmt.Errorf("decode args: %w", err), c)
			return
		}
	}

	// 6. 构建真正的业务 handler
	businessHandler := func(req any) (any, error) {
		returns := mtype.method.Func.Call([]reflect.Value{
			svc.rcvr,
			reflect.ValueOf(req),
			reflect.ValueOf(reply),
		})
		errInter := returns[0].Interface()
		if errInter != nil {
			return nil, errInter.(error)
		}
		return reply, nil
	}

	// 7. 构建拦截器链 (洋葱模型: 先注册的在外层)
	info := &interceptor.UnaryServerInfo{
		Service:    serviceName,
		Method:     methodName,
		FullMethod: env.Method,
	}
	handler := interceptor.ChainUnaryServer(s.interceptors, info, businessHandler)

	// 8. 通过拦截器链调用
	resp, err := handler(args)

	// 9. 处理错误
	if err != nil {
		s.sendError(tconn, env.ID, err, c)
		return
	}

	// 10. 序列化返回值并发送
	replyBytes, err := c.Marshal(resp)
	if err != nil {
		s.sendError(tconn, env.ID, fmt.Errorf("encode reply: %w", err), c)
		return
	}

	respEnv := codec.NewResponse(env.ID, replyBytes)
	respEnvBytes, err := c.Marshal(respEnv)
	if err != nil {
		s.sendError(tconn, env.ID, fmt.Errorf("encode envelope: %w", err), c)
		return
	}

	respFrame := proto.NewFrame(proto.MsgUnaryResponse, 0, respEnvBytes)
	if err := tconn.SendFrame(respFrame); err != nil {
		log.Printf("[lrpc] send response error: %v", err)
	}
}

// sendError 发送错误响应
func (s *Server) sendError(tconn *transport.Conn, id uint64, err error, c codec.Codec) {
	log.Printf("[lrpc] error for request %d: %v", id, err)

	env := codec.NewErrorResponse(id, err)
	envBytes, marshalErr := c.Marshal(env)
	if marshalErr != nil {
		log.Printf("[lrpc] marshal error response: %v", marshalErr)
		return
	}

	frame := proto.NewFrame(proto.MsgErrorResponse, 0, envBytes)
	if sendErr := tconn.SendFrame(frame); sendErr != nil {
		log.Printf("[lrpc] send error response: %v", sendErr)
	}
}
