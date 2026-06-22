# KVStore

基于 **lstl** 轻量 STL 容器库 + **zero** 高性能网络库实现的完整项目集合。

## 项目结构

| 目录 | 说明 |
|------|------|
| `ledis/` | **161 命令仿 Redis** (30 万 QPS, Pipeline 225 万) |
| `zero/` | 高性能 M:N 协程网络库 (echo 74 万 QPS) |
| `lstl/` | 轻量 STL 容器库 (内存池, 开放寻址, 无锁队列) |
| `redisLearningPlat/` | Redis 学习平台 (React + FastAPI + AI) |

## ledis — 核心产品

- **161 命令**: String/Hash/List/Set/ZSet/Stream + Bitmap/HLL/Geo
- **事务**: MULTI/EXEC/WATCH 乐观锁
- **Lua 脚本**: EVAL/EVALSHA (LuaJIT)
- **Pub/Sub**: Channel/Pattern 订阅
- **Stream Consumer Group**: XREADGROUP/XACK
- **持久化**: AOF + RDB (SAVE/BGSAVE)
- **淘汰策略**: maxmemory + LRU/LFU/TTL/Random
- **服务器管理**: CONFIG/INFO/CLIENT/MONITOR/SLOWLOG
- **性能**: 单线程 30 万 QPS (ARM64), Pipeline 225 万
- **迭代记录**: 见 `ledis/doc/ITERATIONS.md`

## 构建

```bash
cd ledis/build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./ledis-server --port 6379
```
