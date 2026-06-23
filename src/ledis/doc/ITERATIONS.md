# Ledis 迭代开发记录

## 版本概览

| 版本 | 架构 | I/O | 线程 | QPS | 瓶颈 |
|------|------|-----|------|-----|------|
| v1 | 存储线程 + IO fibers | zero hook | 4IO+1存储 | 19万 | eventfd 竞争 |
| v2 | 单 fiber inline | zero hook | 1 | 30万 | syscall |
| v3 | v2 + 开放寻址 Dict | zero hook | 1 | 30万 | syscall |
| v4 | v3 + 非阻塞 IO + hash缓存 | MSG_DONTWAIT | 1 | 31万 | syscall |
| v5 | 多核 sharding + epoll/uring | 可配置 | N | ? | 待测 |

## v1: 存储线程模型 (已废弃)

**思路**: IO 线程负责网络，存储线程独占 Dict。通过 MPSC 队列 + eventfd 通信。

**问题**:
- eventfd 通知和 Reactor 注册有竞态条件
- GET 命令在 IO fiber 直接执行 → Dict 被 IO fiber 和存储线程同时访问 → 数据竞争
- pthread_mutex 阻塞整个线程 → 死锁

**教训**: 用户态 fiber 不能用阻塞锁。fiber yield 时持有锁 → 死锁。

## v2: 单线程 inline (当前生产版本)

**思路**: 学 Redis，所有命令在一个线程的 fiber 中直接执行。无锁、无队列、无上下文切换。

**为什么单线程**:
- zero::Mutex = pthread_mutex，会阻塞整个 OS 线程
- fiber 持有锁后 yield → 其他 fiber 无法获取锁 → 死锁
- 只有去掉锁（单线程）才安全

**文件**: server/server.h (当前)

## v3: 开放寻址 Dict

**思路**: 替换 v1 的链式哈希（指针跳转，cache miss），用开放寻址 + 线性探测。

**改进**:
- FNV-1a hash 替代 std::hash + finalizer
- 槽内缓存 hash 值，探测时先比 8 字节整数，命中才比字符串
- Power-of-2 容量 + 位掩码（免除法）
- 批量 resize（无渐进 rehash 的每操作开销）

**结论**: Dict 不是瓶颈。Pipeline 验证引擎能到 250 万。

**文件**: core/dict.h (当前), core/dict_v1.h (旧版备份)

## v4: 非阻塞 IO

**思路**: 跳过 zero hook，用 MSG_DONTWAIT + YieldToReady 轮询。

**为什么**: zero 的 hook 给每个 read/write 加 ~200ns 开销（fd 查找、状态检查）。

**改进**: 
- ::recv(fd, buf, len, MSG_DONTWAIT) 替代 stream.read()
- ::send(fd, buf, len, MSG_DONTWAIT | MSG_NOSIGNAL) 替代 stream.writeFixed()
- TCP_NODELAY 禁用 Nagle 算法

**结论**: 提升 10%，但仍受限于每个命令 2 次 syscall（~2μs）。

## v5: 多核 sharding + 可配置 I/O (开发中)

**思路**:
1. **多核 sharding**: N 个线程，每个线程独立 epoll + 独立 Dict 分片
2. **可配置 I/O 引擎**: epoll 或 io_uring
3. **SO_REUSEPORT**: 内核级连接分发，无需 accept 线程

**为什么能线性扩展**:
- 每个线程有自己的 Dict 分片 → 无锁
- 每个线程有自己的 epoll/uring → 无竞争
- 只共享 AOF 文件（需要 mutex 保护）

**预期**: 4 核 → 4×30万 = 120万 rps

### io_uring 原理
- 取代 recv/send 系统调用
- 通过 submission queue (SQ) 提交 I/O 请求
- 通过 completion queue (CQ) 获取结果
- 一次 io_uring_enter() 可提交/获取多个 I/O → O(1) 而非 O(n) syscall

### 键分片策略
```
shard_id = crc16(key) % num_shards
```
单节点拥有全部 16384 个 slot，多节点时按 slot 分配。

### 可配置选项
```
--io-threads 4       # worker 线程数（每个一个 Dict 分片）
--io-engine epoll     # epoll 或 io_uring
```
