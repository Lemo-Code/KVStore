# Ledis 已实现功能说明

> **状态**：Phase 0–7 + AOF/淘汰/阻塞列表/YAML/应用框架  
> **测试**：`run_tests_ledis` 共 **35** 项 CTest 全绿  
> **协议**：RESP2（见 [protocol.md](./protocol.md)）

---

## 1. 概述

Ledis 是 KVStore 仓库内的 **Redis 兼容内存 KV 服务**，目标是对齐 Redis 常用子集，供 `redis-cli` / 应用客户端通过 TCP 访问。

| 能力 | 状态 |
|------|------|
| 网络 | LemoNettyCore 协程 TCP、长连接、Pipeline |
| 并发 | 单线程模式（默认）/ IO·DB 分离（`--async`） |
| 存储 | 16 逻辑 DB、String/Hash/List/Set/Zset |
| 过期 | 惰性删除 + 主动过期周期 |
| 持久化 | RDB-lite + AOF-lite（自定义格式，非 Redis 原生 RDB/AOF 文件） |
| 安全 | `requirepass` + AUTH |
| 发布订阅 | SUBSCRIBE / PSUBSCRIBE / PUBLISH / PUBSUB |
| 运维 | INFO、CONFIG GET/SET、Application 守护进程、pidfile、SIGINT/SIGTERM 优雅退出 |
| 内存 | maxmemory + LRU/LFU 淘汰（allkeys/volatile） |

**非目标**（当前未实现）：集群、主从、Sentinel、Lua、Module、RESP3、标准 Redis RDB 二进制格式。

---

## 2. 构建与运行

### 2.1 构建

```bash
cmake -B build-nettycore -DLEDIS_BUILD=ON -DLEMONETTYCORE_BUILD=ON
cmake --build build-nettycore --target ledis-server run_tests_ledis -j$(nproc)
```

产物：

| 路径 | 说明 |
|------|------|
| `bin/Ledis/ledis-server` | 服务端可执行文件 |
| `bin/Ledis/<module>/<test>` | 单元/集成测试二进制 |

### 2.2 启动（Application 框架）

`main` 仅委托 `ledis::Application`（`ledis/app/`）：

| 阶段 | 行为 |
|------|------|
| `init` | `Env` 解析 `-s/-d/-c/-p`；YAML 加载；CLI 覆盖；创建 `dir`；pidfile 防重复 |
| `run` | 前台或 `startDaemon`（`-d`：崩溃 5 秒后重启） |
| `main` | `chdir(dir)`、写 pidfile、`LedisServer::start`、等待信号、`stop`、删 pidfile |

```bash
# 帮助
./bin/Ledis/ledis-server -p

# 默认（自动加载 bin/Ledis/conf/ledis.yaml）
./bin/Ledis/ledis-server

# YAML + 覆盖
./bin/Ledis/ledis-server -c conf 0.0.0.0 6379 --async --io-threads 4

# 持久化 + 密码
./bin/Ledis/ledis-server 127.0.0.1 16379 \
  --requirepass secret --dir /var/lib/ledis --appendonly

# 守护进程
./bin/Ledis/ledis-server -d --dir /data/ledis --pidfile /data/ledis/ledis.pid
```

配置优先级与全部字段见 [configuration.md](./configuration.md)。

### 2.3 测试

```bash
cmake --build build-nettycore --target run_tests_ledis
# 或
ctest -R '^Ledis\.' --test-dir build-nettycore/ledis --output-on-failure
```

### 2.4 客户端

```bash
redis-cli -h 127.0.0.1 -p 6379 PING
```

**注意**：`SELECT` 切换 DB 的状态绑定在**同一 TCP 连接**上；每条独立 `redis-cli` 命令是新连接，默认 DB 0。交互式 `redis-cli` 或 Pipeline 才能正确验证多 DB。

---

## 3. 架构分层

```
ledis-app        Env / Application / Daemon / FsUtil（启动、配置、pidfile）
ledis-server     TcpServer + WorkerGroup + Session
ledis-session    连接、Pipeline、IO/DB 队列、PubSub、阻塞 List
ledis-command    命令注册表、CONFIG、AUTH、淘汰
ledis-protocol   RESP2 解析/编码
ledis-store      DB、Keyspace、对象、过期、RDB/AOF
LemoNettyCore    TcpServer、Runtime、Fiber
lstl             Hash/List/Set/Zset 底层容器
```

详见 [architecture.md](./architecture.md)、[layer_contracts.md](./layer_contracts.md)。

---

## 4. 数据模型

### 4.1 逻辑 DB

- 固定 **16** 个 DB，编号 `0`–`15`（与 Redis 默认一致）
- `SELECT index` 切换当前连接上下文；非法 index 返回 `ERR DB index is out of range`

### 4.2 对象类型

| Redis 类型 | LedisObject | 编码 |
|------------|-------------|------|
| String | `kString` | `kRaw` |
| Hash | `kHash` | `kHashMap`（`SdsDict<Sds>`） |
| List | `kList` | `kListDeque`（`Deque<Sds>`） |
| Set | `kSet` | `kSetHash`（`SdsSet`） |
| Zset | `kZset` | `kZsetSkipList`（`ZsetDict`，member→score；范围查询排序后按 rank） |

类型不匹配时统一返回 `-WRONGTYPE Operation against a key holding the wrong kind of value`。

### 4.3 Key 与过期

- Key 为二进制安全 `Sds`（可含 `\0`）
- 过期时间存于 Keyspace 条目，单位毫秒（内部）
- `TTL` 返回秒；无 key 为 `-2`，无过期为 `-1`
- 读取时惰性删除过期 key；DB 线程周期性主动过期（可配置）

---

## 5. 已实现命令

### 5.1 连接与通用

| 命令 | 说明 |
|------|------|
| `PING` | 返回 `PONG` |
| `ECHO message` | 回显 message |
| `AUTH password` | 设置 `requirepass` 后必需；未认证时除 `AUTH`/`PING`/`ECHO` 外返回 `-NOAUTH` |
| `SELECT index` | 切换 DB |
| `INFO [section]` | `section` 可为 `server`、`memory`；省略则输出全部已支持段 |
| `CONFIG GET param` | 只读查询，见 [configuration.md](./configuration.md) |
| `CONFIG SET param value` | 运行时修改部分配置项，见 [configuration.md](./configuration.md) |

### 5.2 Pub/Sub

| 命令 | 说明 |
|------|------|
| `SUBSCRIBE channel [channel ...]` | 订阅频道；返回 subscribe 确认（多 channel 多帧） |
| `UNSUBSCRIBE [channel ...]` | 取消订阅；无参数则全部取消 |
| `PSUBSCRIBE pattern [pattern ...]` | 模式订阅（glob：`*` `?` `[abc]`）；推送 `pmessage` |
| `PUNSUBSCRIBE [pattern ...]` | 取消模式订阅 |
| `PUBLISH channel message` | 向订阅者推送 message / pmessage，返回送达连接数 |
| `PUBSUB CHANNELS [pattern]` | 列出活跃频道（pattern 为 glob，默认 `*`） |
| `PUBSUB NUMSUB channel [channel ...]` | 各频道订阅连接数 |
| `PUBSUB NUMPAT` | 有订阅者的模式数量 |

订阅模式下仅允许 `(P)SUBSCRIBE` / `(P)UNSUBSCRIBE` / `PING`（及规划中的 `QUIT`）。

### 5.3 事务

| 命令 | 说明 |
|------|------|
| `MULTI` | 进入事务；后续命令入队并返回 `QUEUED` |
| `EXEC` | 顺序执行队列内命令，返回结果数组；被 WATCH 的 key 已变更则返回 null |
| `DISCARD` | 放弃事务并清空队列 |
| `WATCH key [key ...]` | 乐观锁监视；`MULTI` 内不可用 |
| `UNWATCH` | 取消所有监视 |

事务状态绑定在**同一连接**的 `SessionContext`；`EXEC` 时逐条执行（无 WATCH 回滚）。开启 AOF 时，`EXEC` 成功后按队列顺序追加各写命令。

### 5.4 String

| 命令 | 说明 |
|------|------|
| `GET key` | 不存在返回 null bulk |
| `SET key value [NX\|XX] [EX seconds\|PX milliseconds]` | 支持 NX/XX/EX/PX；条件不满足返回 null bulk |
| `GETSET key value` | 返回旧值，不存在返回 null；保留原 TTL |
| `DEL key [key ...]` | 返回删除个数 |
| `EXISTS key [key ...]` | 返回存在的 key 数量 |
| `MGET key [key ...]` | 顺序返回，缺失为 null |
| `MSET key value [key value ...]` | 批量设置 |
| `INCR` / `DECR` / `INCRBY` / `DECRBY` | 字符串整数运算；key 不存在视为 0 |
| `APPEND key value` | 追加字符串；不存在则创建；保留 TTL |
| `STRLEN key` | 不存在返回 0 |
| `SETEX key seconds value` | 带 TTL 的 SET（秒） |
| `PSETEX key milliseconds value` | 带 TTL 的 SET（毫秒） |

### 5.5 Key 空间

| 命令 | 说明 |
|------|------|
| `TYPE key` | `none` / `string` / `hash` / `list` / `set` / `zset` |
| `EXISTS key [key ...]` | 返回存在的 key 数量 |
| `RENAME key newkey` | newkey 已存在则覆盖 |
| `RENAMENX key newkey` | 仅当 newkey 不存在时重命名；返回 0/1 |
| `MOVE key db` | 将 key 移到指定 DB；返回 0/1 |
| `TOUCH key [key ...]` | 刷新 LRU 访问时间；返回成功 touch 的数量 |
| `UNLINK key [key ...]` | 同 DEL，删除 key |
| `KEYS pattern` | glob：`*` 任意、`?` 单字符（生产环境慎用，会阻塞遍历） |
| `RANDOMKEY` | 随机返回一个 key；DB 为空返回 null |
| `SCAN cursor [MATCH pattern] [COUNT count]` | 游标迭代；`cursor=0` 开始，返回 `[next_cursor, [keys...]]` |
| `DBSIZE` | 当前 DB 未过期 key 数量 |
| `FLUSHDB` | 清空当前 DB |
| `FLUSHALL` | 清空全部 16 个 DB |

### 5.6 过期

| 命令 | 说明 |
|------|------|
| `EXPIRE key seconds` | 设置 TTL（秒） |
| `PEXPIRE key milliseconds` | 设置 TTL（毫秒） |
| `EXPIREAT key timestamp` | 绝对 Unix 时间戳（秒）；已过期则立即删除 |
| `PEXPIREAT key milliseconds-timestamp` | 绝对 Unix 时间戳（毫秒） |
| `TTL key` | 查询剩余 TTL（秒） |
| `PTTL key` | 查询剩余 TTL（毫秒） |
| `PERSIST key` | 移除过期时间 |

### 5.7 Hash

| 命令 | 说明 |
|------|------|
| `HSET key field value [field value ...]` | 返回新增 field 数量 |
| `HGET key field` | |
| `HDEL key field [field ...]` | |
| `HGETALL key` | flat array `[f1,v1,f2,v2,...]` |
| `HLEN key` | |
| `HEXISTS key field` | 返回 0/1 |
| `HMGET key field [field ...]` | 顺序返回，缺失为 null |
| `HKEYS key` | 返回 field 数组 |
| `HVALS key` | 返回 value 数组 |
| `HINCRBY key field increment` | field 整数自增；不存在视为 0 |

### 5.8 List

| 命令 | 说明 |
|------|------|
| `LPUSH` / `RPUSH key element [element ...]` | 返回列表长度 |
| `LPOP` / `RPOP key` | 空列表返回 null |
| `LLEN key` | 不存在视为 0 |
| `LRANGE key start stop` | 支持负索引 |
| `LINDEX key index` | 按索引取元素；越界返回 null |
| `LTRIM key start stop` | 保留区间内元素，其余删除 |
| `BLPOP key [key ...] timeout` | 阻塞式左弹出；timeout=0 表示一直阻塞 |
| `BRPOP key [key ...] timeout` | 阻塞式右弹出 |
| `RPOPLPUSH source destination` | 原子：从 source 尾部弹出并 push 到 destination 头部 |
| `BRPOPLPUSH source destination timeout` | 阻塞版 RPOPLPUSH |

### 5.9 Set

| 命令 | 说明 |
|------|------|
| `SADD key member [member ...]` | 返回新增 member 数 |
| `SREM key member [member ...]` | |
| `SMEMBERS key` | |
| `SCARD key` | |
| `SISMEMBER key member` | 返回 0/1 |
| `SINTER key [key ...]` | 交集；不存在 key 视为空集 |
| `SUNION key [key ...]` | 并集 |
| `SDIFF key [key ...]` | 差集（第一个 key 减去其余） |
| `SINTERSTORE dest key [key ...]` | 交集写入 dest，返回结果集大小 |
| `SUNIONSTORE dest key [key ...]` | 并集写入 dest |
| `SDIFFSTORE dest key [key ...]` | 差集写入 dest |

### 5.10 Zset

| 命令 | 说明 |
|------|------|
| `ZADD key score member [score member ...]` | 返回新增 member 数 |
| `ZSCORE key member` | 不存在返回 null |
| `ZCARD key` | |
| `ZRANGE key start stop [WITHSCORES]` | 按 score 升序，支持负索引 |
| `ZREVRANGE key start stop [WITHSCORES]` | 按 score 降序 |
| `ZRANGEBYSCORE key min max [WITHSCORES] [LIMIT offset count]` | 支持 `-inf`/`+inf` 与 `(score` 开区间 |
| `ZREM key member [member ...]` | |
| `ZINCRBY key increment member` | 增减 score，返回新 score |
| `ZCOUNT key min max` | score 区间内 member 数量；支持 `-inf`/`+inf` 与开区间 |
| `ZRANK key member` | 0 基 rank，不存在返回 null |
| `ZREVRANK key member` | 降序 rank |
| `ZREVRANGEBYSCORE key max min [WITHSCORES] [LIMIT offset count]` | 按 score 降序；min/max 顺序与 Redis 一致 |

### 5.11 持久化

| 命令 | 说明 |
|------|------|
| `SAVE` | 同步写 RDB-lite 快照 |
| `BGSAVE` | 后台线程写快照；进行中再次调用返回错误 |
| `BGREWRITEAOF` | 后台重写 AOF（需 `appendonly=yes`）；重写期间增量命令缓冲后合并到新文件 |

持久化由 `LedisEngine` 注册，不在 `registerDefaultCommands()` 内。

---

## 6. 持久化

### 6.1 启动加载顺序

1. 若存在 RDB-lite 文件（`dir` + `dbfilename`）→ `loadSnapshot`
2. 若 `appendonly=yes` → 重放 AOF 文件（`appendfilename`）
3. 开启 AOF 时打开 AOF 文件追加

### 6.2 RDB-lite

| 项 | 值 |
|----|-----|
| 魔数 | `LEDIS003\r\n` |
| 内容 | RESP 编码的 16 个 DB 数组 |
| 条目 | `[key, expire_at_ms, type_payload]` |
| 写盘 | 先写 `*.tmp` 再 `rename` 原子替换 |
| 过期 | 保存时跳过已过期 key |

### 6.3 AOF-lite

| 项 | 值 |
|----|-----|
| 魔数 | `LEDIS-AOF1\r\n` |
| 内容 | 逐条 RESP 命令帧 |
| 记录范围 | 写命令（SET/DEL/HSET/…/FLUSHALL 等），不含 GET/PING/SAVE |
| 刷盘 | 每条写命令 `fflush`；`fsync` 由 `appendfsync` 控制（`always` / `everysec` / `no`） |
| 重写 | `BGREWRITEAOF` 从内存快照生成紧凑 AOF，并合并重写期间的增量命令 |

### 6.4 与 Redis 差异

- 文件格式为 Ledis 自定义，**不能**用 `redis-check-rdb` / `redis-check-aof`
- 无自动 `save` 策略（需手动 SAVE/BGSAVE 或依赖 AOF）

---

## 7. 网络与并发

### 7.1 单线程模式（默认）

- `single_thread_mode: true`
- IO 读包 → 同线程 `dispatchSync` → 写回
- 适合开发调试、单连接功能验证

### 7.2 IO/DB 分离（`--async`）

- IO 协程：`Session` 解析命令 → `InboundQueue`（MPSC）
- DB 线程：消费队列 → `dispatchSync` → `ReplyRouter` → IO 写回
- `SELECT` / `AUTH` 等会话状态通过 `ReplyEnvelope.ctx` 回写连接

### 7.3 其他

| 能力 | 说明 |
|------|------|
| Pipeline | 单连接多命令顺序响应 |
| `maxclients` | 超限拒绝新连接 |
| `query_buffer_limit` | 读缓冲超限断开并返回错误 |
| `max_pending_commands` | 异步模式下 IO 入队上限 |

---

## 8. INFO 段

### server

```
# Server
redis_version:7.0-ledis
ledis_mode:standalone
```

### memory

```
# Memory
used_memory:<bytes>       # 估算内存占用
maxmemory:<bytes>         # 0 表示不限制
maxmemory_policy:allkeys-lru   # 或 volatile-lru / allkeys-lfu / volatile-lfu
db_keys:<N>               # 16 个 DB 未过期 key 总数
```

---

## 9. 错误语义

| 错误 | 典型场景 |
|------|----------|
| `ERR unknown command` | 未注册命令 |
| `ERR wrong number of arguments` | 参数个数不对 |
| `ERR syntax error` | SET/SCAN/CONFIG 等语法错误 |
| `-WRONGTYPE` | 类型不匹配 |
| `-NOAUTH Authentication required` | 未 AUTH |
| `ERR invalid password` | AUTH 密码错误 |
| `ERR query buffer limit` | 超过 `query_buffer_limit` |
| `ERR command queue full` | 异步入队满 |

---

## 10. 测试覆盖

| 模块 | 测试 |
|------|------|
| app | `test_env` |
| protocol | `test_resp_reader`, `test_resp_writer` |
| store | `test_keyspace`, `test_hash/list/set/zset_object`, `test_snapshot`, `test_aof` |
| command | `test_ping`, `test_ledis_yaml_config`, `test_env`, `test_phase3`, ... |
| session | `test_command_queue`, `test_io_db`, `test_query_buffer`, `test_session` |
| stream | `test_ledis_stream` |
| integration | `test_mvp`, `test_pipeline`, `test_async_tcp`, `test_maxclients`, `test_rdb_restart`, `test_aof_restart` |

---

## 11. 已知限制

1. **Async Pipeline + SELECT**：同一批 pipeline 内多条命令共享入队时的 `db_index` 快照，批量 SELECT 场景可能不符合预期
2. **KEYS** 全表扫描，大数据量会阻塞 DB 线程
3. **PubSub** 跨连接推送依赖 async 模式 + ReplyRouter
4. **BLPOP/BRPOP/BRPOPLPUSH** 单连接 Pipeline 中阻塞命令会暂停后续解析直至唤醒

---

## 12. 参考

- [configuration.md](./configuration.md) — 全部配置项
- [protocol.md](./protocol.md) — RESP 约定
- [implementation_order.md](./implementation_order.md) — 分阶段路线
- [architecture.md](./architecture.md) — 总体架构
