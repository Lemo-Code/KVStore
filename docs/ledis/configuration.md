# Ledis 配置说明

> **配置入口**：`Application` 统一加载 → `LedisSettings`  
> **优先级**：CLI / positional **>** `--config` / `-c` YAML **>** 默认可执行文件旁 `conf/ledis.yaml` **>** 结构体默认值  
> **运行时**：`CONFIG GET` / `CONFIG SET`（Redis 兼容子集，见 §1.3）

完整 YAML 示例：`ledis/conf/ledis.yaml`（构建后位于 `bin/Ledis/conf/ledis.yaml`）。

---

## 1. 快速参考

### 1.1 启动模式（短选项）

由 `ledis/app/env.h` + `Application` 解析：

| 选项 | 说明 |
|------|------|
| `-s` | 显式前台运行（与不传 `-d` 等价） |
| `-d` | 守护进程：`daemon(3)` + 子进程 worker；异常退出 5 秒后自动重启 |
| `-c PATH` | 配置目录（加载 `PATH/ledis.yaml`）或 YAML 文件路径 |
| `-p` / `-h` | 打印帮助并退出（exit 0） |
| `--pidfile PATH` | pid 文件路径；默认 `<dir>/ledis.pid` |

**启动流程**

```
main → Application::init
         ├─ Env 解析短选项
         ├─ 加载 YAML（--config > -c > conf/ledis.yaml）
         ├─ ApplyLedisCliOverrides（CLI 覆盖）
         ├─ mkdir(dir)、pidfile 防重复启动
         └─ Application::run → startDaemon → Application::main
              ├─ chdir(dir)、写 pidfile
              ├─ LedisServer::start
              └─ SIGINT/SIGTERM 优雅退出并删 pidfile
```

### 1.2 命令行长选项一览

```text
ledis-server [host] [port] [选项...]
```

| 选项 | 对应字段 | 默认值 |
|------|----------|--------|
| `[host]` | `host` | `127.0.0.1` |
| `[port]` | `port` | `6379` |
| `--config PATH` | 加载 YAML | — |
| `--async` | `single_thread_mode=false` | 单线程 |
| `--io-threads N` | `io_threads` | `1` |
| `--maxclients N` | `maxclients` | `10000` |
| `--maxmemory N` | `maxmemory` | `0` |
| `--maxmemory-policy P` | `maxmemory_policy` | `allkeys-lru` |
| `--requirepass PASS` | `requirepass` | 空 |
| `--dir PATH` | `dir` | `.` |
| `--dbfilename NAME` | `dbfilename` | `dump.ledis` |
| `--appendonly` | `appendonly=true` | `false` |
| `--appendfilename NAME` | `appendfilename` | `appendonly.aof` |
| `--appendfsync POLICY` | `appendfsync` | `everysec` |
| `--pidfile PATH` | pid 文件 | `<dir>/ledis.pid` |

**示例**

```bash
# 开发：默认 conf + 16379
./bin/Ledis/ledis-server 127.0.0.1 16379

# 生产：YAML + 异步 + 持久化 + 密码
./bin/Ledis/ledis-server --config /etc/ledis/ledis.yaml \
  --requirepass secret --appendonly

# 守护进程
./bin/Ledis/ledis-server -d --dir /var/lib/ledis \
  --pidfile /var/lib/ledis/ledis.pid
```

### 1.3 CONFIG GET / SET 一览

```bash
redis-cli CONFIG GET maxmemory
redis-cli CONFIG SET appendfsync everysec
redis-cli CONFIG SET maxmemory-policy volatile-lfu
```

| 参数 | GET | SET | 说明 |
|------|-----|-----|------|
| `port` | ✓ | ✗ | 只读（启动时绑定） |
| `maxclients` | ✓ | ✓ | 最大连接数 |
| `maxmemory` | ✓ | ✓ | 字节；`0` 不限制 |
| `maxmemory-policy` | ✓ | ✓ | `allkeys-lru` / `volatile-lru` / `allkeys-lfu` / `volatile-lfu` |
| `databases` / `db_count` | ✓ | ✗ | 固定 `16` |
| `dir` | ✓ | ✓ | RDB/AOF 目录 |
| `dbfilename` | ✓ | ✓ | RDB 文件名 |
| `appendonly` | ✓ | ✓ | `yes` / `no` |
| `appendfilename` | ✓ | ✓ | AOF 文件名 |
| `appendfsync` | ✓ | ✓ | `always` / `everysec` / `no` |
| `requirepass` | ✓ | ✓ | 空字符串取消密码 |

参数名大小写不敏感。`CONFIG SET` 成功返回 `OK`；引擎通过回调即时应用 `appendonly` / `appendfsync` / `dir` 等。

**不在 CONFIG 中的项**（仅启动时生效）：`host`、`single_thread_mode`、`io_threads`、`max_pending_commands`、`query_buffer_limit`、`active_expire_*`。

---

## 2. 配置项详解（LedisSettings）

源码：`ledis/include/ledis/config/ledis_settings.h`  
CLI：`ledis/src/config/ledis_settings.cc`  
YAML：`ledis/src/config/ledis_yaml_config.cc`

### 2.1 网络 · 监听

#### `host`

| 属性 | 值 |
|------|-----|
| 类型 | `string` |
| 默认 | `127.0.0.1` |
| CLI | 第一个 positional |
| YAML | `server.host` |
| CONFIG | — |

#### `port`

| 属性 | 值 |
|------|-----|
| 类型 | `uint16_t` |
| 默认 | `6379` |
| CLI | host 后 positional |
| YAML | `server.port` |
| CONFIG GET | `port` |

`port=0` 时由 OS 分配临时端口，`LedisServer::boundPort()` 返回实际值。

---

### 2.2 并发 · 线程模型

#### `single_thread_mode`

| 属性 | 值 |
|------|-----|
| 默认 | `true` |
| CLI | 默认；`--async` → `false` |
| YAML | `ledis.single_thread_mode` |

| 值 | 行为 |
|----|------|
| `true` | IO 与命令同协程同步执行 |
| `false` | IO 协程 + DB 线程 + MPSC/SPSC 队列 |

#### `io_threads`

| 属性 | 值 |
|------|-----|
| 默认 | `1` |
| CLI | `--io-threads N` |
| YAML | `ledis.io_threads` 或 `io.threads` |

#### `maxclients`

| 属性 | 值 |
|------|-----|
| 默认 | `10000` |
| CLI | `--maxclients N` |
| YAML | `ledis.maxclients` |
| CONFIG | GET/SET |

`0` 表示不限制。

#### `max_pending_commands`

| 属性 | 值 |
|------|-----|
| 默认 | `65536` |
| YAML | `ledis.max_pending_commands` |

异步模式 InboundQueue 容量；满时 `ERR command queue full`。

#### `query_buffer_limit`

| 属性 | 值 |
|------|-----|
| 默认 | `1048576`（1 MiB） |
| YAML | `ledis.query_buffer_limit` |

单连接读缓冲上限；`0` 不限制。

---

### 2.3 内存与淘汰

#### `maxmemory`

| 属性 | 值 |
|------|-----|
| 默认 | `0` |
| CLI | `--maxmemory N` |
| YAML | `ledis.maxmemory` |
| CONFIG | GET/SET |

超出时按 `maxmemory_policy` 淘汰；估算基于 `used_memory`。

#### `maxmemory_policy`

| 属性 | 值 |
|------|-----|
| 默认 | `allkeys-lru` |
| CLI | `--maxmemory-policy` |
| YAML | `ledis.maxmemory_policy` |
| CONFIG | GET/SET |

| 策略 | 说明 |
|------|------|
| `allkeys-lru` | 所有 key，LRU |
| `volatile-lru` | 仅带 TTL 的 key，LRU |
| `allkeys-lfu` | 所有 key，LFU |
| `volatile-lfu` | 仅带 TTL 的 key，LFU |

---

### 2.4 安全

#### `requirepass`

| 属性 | 值 |
|------|-----|
| 默认 | 空 |
| CLI | `--requirepass PASS` |
| YAML | `ledis.requirepass` |
| CONFIG | GET/SET |

非空时除 `AUTH` / `PING` / `ECHO` 外需先认证。

---

### 2.5 过期

#### `active_expire_enabled` / `active_expire_cycle_keys`

| 字段 | 默认 | YAML |
|------|------|------|
| `active_expire_enabled` | `true` | `ledis.active_expire_enabled` |
| `active_expire_cycle_keys` | `20` | `ledis.active_expire_cycle_keys` |

DB 线程周期性主动过期；单线程模式主要依赖惰性删除。

---

### 2.6 持久化 · RDB-lite / AOF-lite

| 字段 | 默认 | CLI | YAML | CONFIG |
|------|------|-----|------|--------|
| `dir` | `.` | `--dir` | `ledis.dir` | GET/SET |
| `dbfilename` | `dump.ledis` | `--dbfilename` | `ledis.dbfilename` | GET/SET |
| `appendonly` | `false` | `--appendonly` | `ledis.appendonly` | GET/SET |
| `appendfilename` | `appendonly.aof` | `--appendfilename` | `ledis.appendfilename` | GET/SET |
| `appendfsync` | `everysec` | `--appendfsync` | `ledis.appendfsync` | GET/SET |

启动时 `Application::main` 会 `chdir(dir)`。  
RDB 魔数 `LEDIS003`；AOF 魔数 `LEDIS-AOF1`；支持 `SAVE` / `BGSAVE` / `BGREWRITEAOF`。

---

### 2.7 只读常量

| 项 | CONFIG GET | 说明 |
|----|------------|------|
| `databases` | `16` | `DBManager::kDbCount` |
| `port` | 实际绑定端口 | 启动后更新 |

---

## 3. YAML 配置

### 3.1 加载顺序

1. `--config PATH`
2. `-c PATH`（目录 → `PATH/ledis.yaml`；或以 `.yaml`/`.yml` 结尾的文件）
3. `<exe_dir>/conf/ledis.yaml`（存在则加载）
4. 以上均无则使用 `LedisSettings` 默认值
5. **`ApplyLedisCliOverrides`** 覆盖 YAML

### 3.2 字段映射

| YAML 键 | LedisSettings 字段 |
|---------|-------------------|
| `server.host` | `host` |
| `server.port` | `port` |
| `io.threads` | `io_threads` |
| `ledis.single_thread_mode` | `single_thread_mode` |
| `ledis.io_threads` | `io_threads` |
| `ledis.maxclients` | `maxclients` |
| `ledis.max_pending_commands` | `max_pending_commands` |
| `ledis.query_buffer_limit` | `query_buffer_limit` |
| `ledis.maxmemory` | `maxmemory` |
| `ledis.maxmemory_policy` | `maxmemory_policy` |
| `ledis.requirepass` | `requirepass` |
| `ledis.active_expire_enabled` | `active_expire_enabled` |
| `ledis.active_expire_cycle_keys` | `active_expire_cycle_keys` |
| `ledis.dir` | `dir` |
| `ledis.dbfilename` | `dbfilename` |
| `ledis.appendonly` | `appendonly` |
| `ledis.appendfilename` | `appendfilename` |
| `ledis.appendfsync` | `appendfsync` |

布尔值支持：`true/false`、`yes/no`、`on/off`、`1/0`。

### 3.3 完整示例

见 `ledis/conf/ledis.yaml`。测试夹具：`tests/ledis/fixtures/ledis_mvp.yaml`。

```bash
./bin/Ledis/ledis-server --config tests/ledis/fixtures/ledis_mvp.yaml
./bin/Ledis/ledis-server -c conf 127.0.0.1 17000   # CLI 覆盖 host/port
```

---

## 4. 守护进程与 pidfile

| 机制 | 说明 |
|------|------|
| `-d` | 双 fork 守护；worker 崩溃后 5 秒重启；正常 exit 0 不重启 |
| pidfile | 默认 `<dir>/ledis.pid`；启动前检测存活进程；退出时删除 |
| `--pidfile` | 自定义 pid 路径 |
| 信号 | `SIGINT` / `SIGTERM` → 优雅 `stop()` |

实现：`ledis/app/daemon.cc`、`ledis/app/fs_util.cc`、`ledis/app/application.cc`。

---

## 5. 配置与模块映射

```
LedisSettings
    │
    ├─► Application       dir, chdir, pidfile, 信号, YAML/CLI
    ├─► LedisServer       host, port, single_thread_mode, io_threads,
    │                     maxclients, query_buffer_limit
    ├─► LedisEngine       dir, dbfilename, append*, requirepass,
    │                     active_expire_*, max_pending_commands, maxmemory
    └─► CommandRegistry   CONFIG GET/SET, AUTH, 淘汰策略名
```

---

## 6. 推荐配置组合

### 开发（默认 conf）

```bash
./bin/Ledis/ledis-server
```

### 本地持久化

```bash
./bin/Ledis/ledis-server --dir ./data --appendonly --dbfilename dump.ledis
```

### 多连接 / Pipeline

```bash
./bin/Ledis/ledis-server --async --io-threads 4 --maxclients 10000
```

### 生产守护

```bash
./bin/Ledis/ledis-server -d -c /etc/ledis \
  --dir /var/lib/ledis --appendonly --requirepass "$PASS"
```

---

## 7. 环境变量

当前 **不支持** 环境变量覆盖；使用 CLI、YAML 或 `CONFIG SET`。

---

## 8. 变更记录

| 版本 | 说明 |
|------|------|
| v0.7 | Application/Env/Daemon 启动框架；YAML 自动加载；pidfile |
| v0.6 | CONFIG appendfsync；maxmemory 淘汰 |
| v0.5 | AOF-lite、BGREWRITEAOF |
| v0.4 | RDB-lite、dir/dbfilename |
| v0.3 | requirepass、过期配置 |
| v0.2 | `--async`、`io_threads`、`maxclients` |

---

## 9. 参考

- [features.md](./features.md) — 命令与行为
- [architecture.md](./architecture.md) — 架构分层
- `ledis/include/ledis/app/application.h` — 应用入口
