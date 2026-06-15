#include "ledis/cmd/cmd_table.h"
#include "ledis/cmd/cmd_context.h"
#include "ledis/storage/storage_engine.h"
#include "ledis/storage/hash_store.h"
#include "ledis/storage/list_store.h"
#include "ledis/storage/set_store.h"
#include "ledis/storage/zset_store.h"
#include "ledis/server/pubsub_manager.h"

#include <algorithm>
#include <cstring>

namespace ledis {

// ============================================================
// 命令处理函数 (前向声明)
// ============================================================

// ---- 服务器 ----
static void cmd_ping(CmdContext& ctx);
static void cmd_echo(CmdContext& ctx);
static void cmd_command(CmdContext& ctx);
static void cmd_info(CmdContext& ctx);
static void cmd_dbsize(CmdContext& ctx);
static void cmd_flushdb(CmdContext& ctx);
static void cmd_flushall(CmdContext& ctx);
static void cmd_keys(CmdContext& ctx);
static void cmd_randomkey(CmdContext& ctx);
static void cmd_select(CmdContext& ctx);

// ---- 基础 KV ----
static void cmd_get(CmdContext& ctx);
static void cmd_set(CmdContext& ctx);
static void cmd_del(CmdContext& ctx);
static void cmd_exists(CmdContext& ctx);
static void cmd_type(CmdContext& ctx);

// ---- 过期 ----
static void cmd_expire(CmdContext& ctx);
static void cmd_expireat(CmdContext& ctx);
static void cmd_pexpire(CmdContext& ctx);
static void cmd_pexpireat(CmdContext& ctx);
static void cmd_ttl(CmdContext& ctx);
static void cmd_pttl(CmdContext& ctx);
static void cmd_persist(CmdContext& ctx);

// ---- 字符串 ----
static void cmd_incr(CmdContext& ctx);
static void cmd_incrby(CmdContext& ctx);
static void cmd_decr(CmdContext& ctx);
static void cmd_decrby(CmdContext& ctx);
static void cmd_append(CmdContext& ctx);
static void cmd_strlen(CmdContext& ctx);
static void cmd_mget(CmdContext& ctx);
static void cmd_mset(CmdContext& ctx);
static void cmd_setnx(CmdContext& ctx);
static void cmd_setex(CmdContext& ctx);
static void cmd_psetex(CmdContext& ctx);
static void cmd_getset(CmdContext& ctx);
static void cmd_getrange(CmdContext& ctx);
static void cmd_setrange(CmdContext& ctx);
static void cmd_incrbyfloat(CmdContext& ctx);
static void cmd_msetnx(CmdContext& ctx);
static void cmd_getdel(CmdContext& ctx);

// ---- Hash ----
static void cmd_hset(CmdContext& ctx);
static void cmd_hget(CmdContext& ctx);
static void cmd_hdel(CmdContext& ctx);
static void cmd_hexists(CmdContext& ctx);
static void cmd_hgetall(CmdContext& ctx);
static void cmd_hlen(CmdContext& ctx);
static void cmd_hkeys(CmdContext& ctx);
static void cmd_hvals(CmdContext& ctx);
static void cmd_hincrby(CmdContext& ctx);
static void cmd_hstrlen(CmdContext& ctx);
static void cmd_hsetnx(CmdContext& ctx);
static void cmd_hmset(CmdContext& ctx);
static void cmd_hmget(CmdContext& ctx);
static void cmd_hincrbyfloat(CmdContext& ctx);

// ---- List ----
static void cmd_lpush(CmdContext& ctx);
static void cmd_rpush(CmdContext& ctx);
static void cmd_lpop(CmdContext& ctx);
static void cmd_rpop(CmdContext& ctx);
static void cmd_llen(CmdContext& ctx);
static void cmd_lrange(CmdContext& ctx);
static void cmd_lindex(CmdContext& ctx);
static void cmd_lset(CmdContext& ctx);
static void cmd_ltrim(CmdContext& ctx);
static void cmd_blpop(CmdContext& ctx);
static void cmd_brpop(CmdContext& ctx);
static void cmd_lpushx(CmdContext& ctx);
static void cmd_rpushx(CmdContext& ctx);
static void cmd_rpoplpush(CmdContext& ctx);
static void cmd_lrem(CmdContext& ctx);
static void cmd_linsert(CmdContext& ctx);

// ---- Set ----
static void cmd_sadd(CmdContext& ctx);
static void cmd_srem(CmdContext& ctx);
static void cmd_smembers(CmdContext& ctx);
static void cmd_sismember(CmdContext& ctx);
static void cmd_scard(CmdContext& ctx);
static void cmd_sinter(CmdContext& ctx);
static void cmd_sunion(CmdContext& ctx);
static void cmd_sdiff(CmdContext& ctx);
static void cmd_srandmember(CmdContext& ctx);
static void cmd_spop(CmdContext& ctx);
static void cmd_smove(CmdContext& ctx);
static void cmd_smismember(CmdContext& ctx);
static void cmd_sunionstore(CmdContext& ctx);
static void cmd_sinterstore(CmdContext& ctx);
static void cmd_sdiffstore(CmdContext& ctx);

// ---- ZSet ----
static void cmd_zadd(CmdContext& ctx);
static void cmd_zrem(CmdContext& ctx);
static void cmd_zscore(CmdContext& ctx);
static void cmd_zcard(CmdContext& ctx);
static void cmd_zrank(CmdContext& ctx);
static void cmd_zrange(CmdContext& ctx);
static void cmd_zcount(CmdContext& ctx);
static void cmd_zincrby(CmdContext& ctx);
static void cmd_zrevrank(CmdContext& ctx);
static void cmd_zrevrange(CmdContext& ctx);
static void cmd_zrangebyscore(CmdContext& ctx);
static void cmd_zrevrangebyscore(CmdContext& ctx);
static void cmd_zremrangebyrank(CmdContext& ctx);
static void cmd_zremrangebyscore(CmdContext& ctx);
static void cmd_zpopmin(CmdContext& ctx);
static void cmd_zpopmax(CmdContext& ctx);

// ---- Server ----
static void cmd_sort(CmdContext& ctx);
static void cmd_config(CmdContext& ctx);
static void cmd_client(CmdContext& ctx);
static void cmd_slowlog(CmdContext& ctx);
static void cmd_auth(CmdContext& ctx);
static void cmd_time(CmdContext& ctx);
static void cmd_shutdown(CmdContext& ctx);
static void cmd_monitor(CmdContext& ctx);
static void cmd_rename_cmd(CmdContext& ctx);
static void cmd_renamenx_cmd(CmdContext& ctx);
static void cmd_scan(CmdContext& ctx);

// ---- Transaction ----
static void cmd_multi(CmdContext& ctx);
static void cmd_exec(CmdContext& ctx);
static void cmd_discard(CmdContext& ctx);
static void cmd_watch(CmdContext& ctx);
static void cmd_unwatch(CmdContext& ctx);

// ---- Persistence ----
static void cmd_save(CmdContext& ctx);
static void cmd_bgsave(CmdContext& ctx);
static void cmd_bgrewriteaof(CmdContext& ctx);

// ---- Pub/Sub ----
static void cmd_subscribe(CmdContext& ctx);
static void cmd_psubscribe(CmdContext& ctx);
static void cmd_unsubscribe(CmdContext& ctx);
static void cmd_punsubscribe(CmdContext& ctx);
static void cmd_publish(CmdContext& ctx);
static void cmd_pubsub(CmdContext& ctx);

// 命令表 — 严格按字母序排列 (支持二分查找)
static const CmdInfo cmd_table[] = {
    {"append",   cmd_append,    3, CMD_WRITE | CMD_FAST, "wF"},
    {"auth",     cmd_auth,      2, CMD_READONLY, "r"},
    {"bgrewriteaof",cmd_bgrewriteaof,1,CMD_READONLY,"r"},
    {"bgsave",   cmd_bgsave,    1, CMD_READONLY, "r"},
    {"blpop",    cmd_blpop,    -3, CMD_WRITE, "w"},
    {"brpop",    cmd_brpop,    -3, CMD_WRITE, "w"},
    {"client",   cmd_client,   -2, CMD_READONLY, "r"},
    {"command",  cmd_command,  -1, CMD_READONLY, "r"},
    {"config",   cmd_config,   -3, CMD_READONLY, "r"},
    {"dbsize",   cmd_dbsize,    1, CMD_READONLY | CMD_FAST, "rF"},
    {"decr",     cmd_decr,      2, CMD_WRITE | CMD_FAST, "wF"},
    {"decrby",   cmd_decrby,    3, CMD_WRITE | CMD_FAST, "wF"},
    {"del",      cmd_del,      -2, CMD_WRITE, "w"},
    {"discard",  cmd_discard,   1, CMD_READONLY, "r"},
    {"echo",     cmd_echo,      2, CMD_READONLY, "r"},
    {"exec",     cmd_exec,      1, CMD_READONLY, "r"},
    {"exists",   cmd_exists,   -2, CMD_READONLY | CMD_FAST, "rF"},
    {"expire",   cmd_expire,    3, CMD_WRITE | CMD_FAST, "wF"},
    {"expireat", cmd_expireat,  3, CMD_WRITE | CMD_FAST, "wF"},
    {"flushall", cmd_flushall,  1, CMD_WRITE, "w"},
    {"flushdb",  cmd_flushdb,   1, CMD_WRITE, "w"},
    {"get",      cmd_get,       2, CMD_READONLY | CMD_FAST, "rF"},
    {"getdel",   cmd_getdel,    2, CMD_WRITE | CMD_FAST, "wF"},
    {"getrange", cmd_getrange,  4, CMD_READONLY, "r"},
    {"getset",   cmd_getset,    3, CMD_WRITE | CMD_FAST, "wF"},
    {"hdel",     cmd_hdel,     -3, CMD_WRITE | CMD_FAST, "wF"},
    {"hexists",  cmd_hexists,   3, CMD_READONLY | CMD_FAST, "rF"},
    {"hget",     cmd_hget,      3, CMD_READONLY | CMD_FAST, "rF"},
    {"hgetall",  cmd_hgetall,   2, CMD_READONLY, "r"},
    {"hincrby",  cmd_hincrby,   4, CMD_WRITE | CMD_FAST, "wF"},
    {"hincrbyfloat",cmd_hincrbyfloat,4,CMD_WRITE|CMD_FAST,"wF"},
    {"hkeys",    cmd_hkeys,     2, CMD_READONLY, "r"},
    {"hlen",     cmd_hlen,      2, CMD_READONLY | CMD_FAST, "rF"},
    {"hmget",    cmd_hmget,    -3, CMD_READONLY, "r"},
    {"hmset",    cmd_hmset,    -4, CMD_WRITE | CMD_FAST, "wF"},
    {"hset",     cmd_hset,     -4, CMD_WRITE | CMD_FAST, "wF"},
    {"hsetnx",   cmd_hsetnx,    4, CMD_WRITE | CMD_FAST, "wF"},
    {"hstrlen",  cmd_hstrlen,   3, CMD_READONLY | CMD_FAST, "rF"},
    {"hvals",    cmd_hvals,     2, CMD_READONLY, "r"},
    {"incr",     cmd_incr,      2, CMD_WRITE | CMD_FAST, "wF"},
    {"incrby",   cmd_incrby,    3, CMD_WRITE | CMD_FAST, "wF"},
    {"incrbyfloat",cmd_incrbyfloat,3,CMD_WRITE|CMD_FAST,"wF"},
    {"info",     cmd_info,     -1, CMD_READONLY, "r"},
    {"keys",     cmd_keys,      2, CMD_READONLY, "r"},
    {"lindex",   cmd_lindex,    3, CMD_READONLY, "r"},
    {"linsert",  cmd_linsert,   5, CMD_WRITE | CMD_FAST, "wF"},
    {"llen",     cmd_llen,      2, CMD_READONLY | CMD_FAST, "rF"},
    {"lpop",     cmd_lpop,     -2, CMD_WRITE | CMD_FAST, "wF"},
    {"lpush",    cmd_lpush,    -3, CMD_WRITE | CMD_FAST, "wF"},
    {"lpushx",   cmd_lpushx,    3, CMD_WRITE | CMD_FAST, "wF"},
    {"lrange",   cmd_lrange,    4, CMD_READONLY, "r"},
    {"lrem",     cmd_lrem,      4, CMD_WRITE, "w"},
    {"lset",     cmd_lset,      4, CMD_WRITE | CMD_FAST, "wF"},
    {"ltrim",    cmd_ltrim,     4, CMD_WRITE, "w"},
    {"mget",     cmd_mget,     -2, CMD_READONLY, "r"},
    {"monitor",  cmd_monitor,   1, CMD_READONLY, "r"},
    {"mset",     cmd_mset,     -3, CMD_WRITE, "w"},
    {"msetnx",   cmd_msetnx,   -3, CMD_WRITE, "w"},
    {"multi",    cmd_multi,     1, CMD_READONLY, "r"},
    {"persist",  cmd_persist,   2, CMD_WRITE | CMD_FAST, "wF"},
    {"pexpire",  cmd_pexpire,   3, CMD_WRITE | CMD_FAST, "wF"},
    {"pexpireat",cmd_pexpireat,3, CMD_WRITE | CMD_FAST, "wF"},
    {"ping",     cmd_ping,     -1, CMD_READONLY | CMD_FAST, "rF"},
    {"psetex",   cmd_psetex,    4, CMD_WRITE | CMD_FAST, "wF"},
    {"psubscribe",cmd_psubscribe,-2,CMD_PUBSUB,""},
    {"pttl",     cmd_pttl,      2, CMD_READONLY | CMD_FAST, "rF"},
    {"publish",  cmd_publish,   3, CMD_PUBSUB, ""},
    {"pubsub",   cmd_pubsub,   -2, CMD_READONLY, "r"},
    {"punsubscribe",cmd_punsubscribe,-1,CMD_PUBSUB,""},
    {"randomkey",cmd_randomkey,1, CMD_READONLY, "r"},
    {"rename",   cmd_rename_cmd,3,CMD_WRITE, "w"},
    {"renamenx", cmd_renamenx_cmd,3,CMD_WRITE,"w"},
    {"rpop",     cmd_rpop,     -2, CMD_WRITE | CMD_FAST, "wF"},
    {"rpoplpush",cmd_rpoplpush,3, CMD_WRITE, "w"},
    {"rpush",    cmd_rpush,    -3, CMD_WRITE | CMD_FAST, "wF"},
    {"rpushx",   cmd_rpushx,    3, CMD_WRITE | CMD_FAST, "wF"},
    {"sadd",     cmd_sadd,     -3, CMD_WRITE | CMD_FAST, "wF"},
    {"save",     cmd_save,      1, CMD_READONLY, "r"},
    {"scan",     cmd_scan,     -2, CMD_READONLY, "r"},
    {"scard",    cmd_scard,     2, CMD_READONLY | CMD_FAST, "rF"},
    {"sdiff",    cmd_sdiff,    -2, CMD_READONLY, "r"},
    {"sdiffstore",cmd_sdiffstore,-3,CMD_WRITE,"w"},
    {"select",   cmd_select,    2, CMD_READONLY, "r"},
    {"set",      cmd_set,      -3, CMD_WRITE, "w"},
    {"setex",    cmd_setex,     4, CMD_WRITE | CMD_FAST, "wF"},
    {"setnx",    cmd_setnx,     3, CMD_WRITE | CMD_FAST, "wF"},
    {"setrange", cmd_setrange,  4, CMD_WRITE, "w"},
    {"shutdown", cmd_shutdown, -1, CMD_READONLY, "r"},
    {"sinter",   cmd_sinter,   -2, CMD_READONLY, "r"},
    {"sinterstore",cmd_sinterstore,-3,CMD_WRITE,"w"},
    {"sismember",cmd_sismember,3, CMD_READONLY | CMD_FAST, "rF"},
    {"slowlog",  cmd_slowlog,  -2, CMD_READONLY, "r"},
    {"smembers", cmd_smembers,  2, CMD_READONLY, "r"},
    {"sort",     cmd_sort,     -2, CMD_WRITE, "w"},
    {"smismember",cmd_smismember,-3,CMD_READONLY|CMD_FAST,"rF"},
    {"smove",    cmd_smove,     4, CMD_WRITE | CMD_FAST, "wF"},
    {"spop",     cmd_spop,     -2, CMD_WRITE, "w"},
    {"srandmember",cmd_srandmember,-2,CMD_READONLY,"r"},
    {"srem",     cmd_srem,     -3, CMD_WRITE | CMD_FAST, "wF"},
    {"strlen",   cmd_strlen,    2, CMD_READONLY | CMD_FAST, "rF"},
    {"subscribe",cmd_subscribe,-2,CMD_PUBSUB,""},
    {"sunion",   cmd_sunion,   -2, CMD_READONLY, "r"},
    {"sunionstore",cmd_sunionstore,-3,CMD_WRITE,"w"},
    {"time",     cmd_time,      1, CMD_READONLY | CMD_FAST, "rF"},
    {"ttl",      cmd_ttl,       2, CMD_READONLY | CMD_FAST, "rF"},
    {"type",     cmd_type,      2, CMD_READONLY, "r"},
    {"unsubscribe",cmd_unsubscribe,-1,CMD_PUBSUB,""},
    {"unwatch",  cmd_unwatch,   1, CMD_READONLY, "r"},
    {"watch",    cmd_watch,    -2, CMD_READONLY, "r"},
    {"zadd",     cmd_zadd,     -4, CMD_WRITE | CMD_FAST, "wF"},
    {"zcard",    cmd_zcard,     2, CMD_READONLY | CMD_FAST, "rF"},
    {"zcount",   cmd_zcount,    4, CMD_READONLY, "r"},
    {"zincrby",  cmd_zincrby,   4, CMD_WRITE | CMD_FAST, "wF"},
    {"zpopmax",  cmd_zpopmax,  -2, CMD_WRITE | CMD_FAST, "wF"},
    {"zpopmin",  cmd_zpopmin,  -2, CMD_WRITE | CMD_FAST, "wF"},
    {"zrange",   cmd_zrange,   -4, CMD_READONLY, "r"},
    {"zrangebyscore",cmd_zrangebyscore,-4,CMD_READONLY,"r"},
    {"zrank",    cmd_zrank,     3, CMD_READONLY, "r"},
    {"zrem",     cmd_zrem,     -3, CMD_WRITE | CMD_FAST, "wF"},
    {"zremrangebyrank",cmd_zremrangebyrank,4,CMD_WRITE,"w"},
    {"zremrangebyscore",cmd_zremrangebyscore,4,CMD_WRITE,"w"},
    {"zrevrange",cmd_zrevrange,-4,CMD_READONLY,"r"},
    {"zrevrangebyscore",cmd_zrevrangebyscore,-4,CMD_READONLY,"r"},
    {"zrevrank", cmd_zrevrank,  3, CMD_READONLY, "r"},
    {"zscore",   cmd_zscore,    3, CMD_READONLY | CMD_FAST, "rF"},
};

// 全局命令表指针 (声明在 cmd_table.h)
const CmdInfo* g_cmd_table = cmd_table;
const size_t   g_cmd_table_size = sizeof(cmd_table) / sizeof(cmd_table[0]);

// ============================================================
// 命令查找 (二分查找)
// ============================================================
const CmdInfo* lookupCmd(std::string_view name) {
    // 二分查找 (命令表按字母序排列)
    auto cmp = [](const CmdInfo& a, const CmdInfo& b) {
        return std::strcmp(a.name, b.name) < 0;
    };

    CmdInfo key;
    key.name = nullptr;  // 不能直接设置 name string_view 来做二分查找

    // 手动二分查找
    int lo = 0, hi = static_cast<int>(g_cmd_table_size) - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int c = std::strcmp(name.data(), cmd_table[mid].name);
        if (c == 0) {
            // 精确匹配或前缀匹配 (Redis 允许命令缩写)
            // 检查是否完全匹配
            if (name.size() == std::strlen(cmd_table[mid].name)) {
                return &cmd_table[mid];
            }
            // 前缀匹配但命令名更长 → 不在表中
            // 如果缩写匹配了唯一的命令，Redis 接受它
            // 简化: 只接受完全匹配
            return (name.size() == std::strlen(cmd_table[mid].name))
                   ? &cmd_table[mid] : nullptr;
        } else if (c < 0) {
            hi = mid - 1;
        } else {
            lo = mid + 1;
        }
    }
    return nullptr;
}

// ============================================================
// 分发入口
// ============================================================
void dispatchCommand(StorageEngine& engine, CmdContext& ctx) {
    ctx.engine = &engine;

    const auto& args = ctx.args;
    if (args.empty()) return;

    std::string_view cmd_name = args[0];

    // 转换为小写 (Redis 命令不区分大小写)
    std::string cmd_lower;
    cmd_lower.reserve(cmd_name.size());
    for (char c : cmd_name) cmd_lower += (c >= 'A' && c <= 'Z') ? c + 32 : c;

    const CmdInfo* info = lookupCmd(cmd_lower);

    if (!info) {
        ctx.replyUnknownCommand(cmd_name);
        engine.miss_count_++;
        return;
    }

    // 检查参数个数
    if (!info->checkArity(static_cast<int>(args.size()))) {
        ctx.replyWrongArgCount(cmd_name);
        return;
    }

    // 标记是否为写命令 (用于 AOF)
    ctx.is_write = (info->flags & CMD_WRITE) != 0;

    // 执行
    info->handler(ctx);
}

// ============================================================
// 命令实现
// ============================================================

// ---- 服务器命令 ----

static void cmd_ping(CmdContext& ctx) {
    if (ctx.args.size() == 1) {
        ctx.replyPong();
    } else {
        // PING message → 返回 message
        ctx.replyBulk(ctx.args[1]);
    }
}

static void cmd_echo(CmdContext& ctx) {
    ctx.replyBulk(ctx.args[1]);
}

static void cmd_command(CmdContext& ctx) {
    // COMMAND → 返回所有命令信息 (简化: 返回 OK)
    ctx.replyOK();
}

static void cmd_info(CmdContext& ctx) {
    // INFO [section]
    char buf[1024];
    StorageEngine& e = *ctx.engine;
    snprintf(buf, sizeof(buf),
        "# Server\r\n"
        "ledis_version:0.1.0\r\n"
        "os:Linux\r\n"
        "\r\n"
        "# Stats\r\n"
        "total_commands_processed:%lu\r\n"
        "keyspace_hits:%lu\r\n"
        "keyspace_misses:%lu\r\n"
        "\r\n"
        "# Keyspace\r\n"
        "db0:keys=%zu,expires=0\r\n",
        (unsigned long)0,  // TODO: 全局统计
        (unsigned long)e.hit_count_,
        (unsigned long)e.miss_count_,
        e.size()
    );
    ctx.replyBulk(std::string(buf));
}

static void cmd_dbsize(CmdContext& ctx) {
    ctx.engine->dbsize(ctx);
}

static void cmd_flushdb(CmdContext& ctx) {
    ctx.engine->flushdb(ctx);
}

static void cmd_flushall(CmdContext& ctx) {
    ctx.engine->flushdb(ctx);
}

static void cmd_keys(CmdContext& ctx) {
    ctx.engine->keys(ctx);
}

static void cmd_randomkey(CmdContext& ctx) {
    ctx.engine->randomkey(ctx);
}

static void cmd_select(CmdContext& ctx) {
    // SELECT db — 简化: 只支持 db 0
    ctx.replyOK();
}

// ---- 基础 KV ----

static void cmd_get(CmdContext& ctx) {
    ctx.engine->get(ctx);
}

static void cmd_set(CmdContext& ctx) {
    ctx.engine->set(ctx);
}

static void cmd_del(CmdContext& ctx) {
    ctx.engine->del(ctx);
}

static void cmd_exists(CmdContext& ctx) {
    ctx.engine->exists(ctx);
}

static void cmd_type(CmdContext& ctx) {
    ctx.engine->type(ctx);
}

// ---- 过期 ----

static void cmd_expire(CmdContext& ctx) {
    ctx.engine->expire(ctx);
}

static void cmd_expireat(CmdContext& ctx) {
    ctx.engine->expireat(ctx);
}

static void cmd_pexpire(CmdContext& ctx) {
    ctx.engine->pexpire(ctx);
}

static void cmd_pexpireat(CmdContext& ctx) {
    ctx.engine->pexpireat(ctx);
}

static void cmd_ttl(CmdContext& ctx) {
    ctx.engine->ttl(ctx);
}

static void cmd_pttl(CmdContext& ctx) {
    ctx.engine->pttl(ctx);
}

static void cmd_persist(CmdContext& ctx) {
    ctx.engine->persist(ctx);
}

// ---- 字符串操作 ----

static void cmd_incr(CmdContext& ctx) {
    ctx.engine->incr(ctx);
}

static void cmd_incrby(CmdContext& ctx) {
    ctx.engine->incrby(ctx);
}

static void cmd_decr(CmdContext& ctx) {
    ctx.engine->decr(ctx);
}

static void cmd_decrby(CmdContext& ctx) {
    ctx.engine->decrby(ctx);
}

static void cmd_append(CmdContext& ctx) {
    ctx.engine->append(ctx);
}

static void cmd_strlen(CmdContext& ctx) {
    ctx.engine->strlen(ctx);
}

static void cmd_mget(CmdContext& ctx) {
    ctx.engine->mget(ctx);
}

static void cmd_mset(CmdContext& ctx) {
    ctx.engine->mset(ctx);
}

// ---- Hash ----
static void cmd_hset(CmdContext& ctx)    { HashStore::hset(ctx); }
static void cmd_hget(CmdContext& ctx)    { HashStore::hget(ctx); }
static void cmd_hdel(CmdContext& ctx)    { HashStore::hdel(ctx); }
static void cmd_hexists(CmdContext& ctx) { HashStore::hexists(ctx); }
static void cmd_hgetall(CmdContext& ctx) { HashStore::hgetall(ctx); }
static void cmd_hlen(CmdContext& ctx)    { HashStore::hlen(ctx); }
static void cmd_hkeys(CmdContext& ctx)   { HashStore::hkeys(ctx); }
static void cmd_hvals(CmdContext& ctx)   { HashStore::hvals(ctx); }
static void cmd_hincrby(CmdContext& ctx) { HashStore::hincrby(ctx); }
static void cmd_hstrlen(CmdContext& ctx) { HashStore::hstrlen(ctx); }

// ---- List ----
static void cmd_lpush(CmdContext& ctx)  { ListStore::lpush(ctx); }
static void cmd_rpush(CmdContext& ctx)  { ListStore::rpush(ctx); }
static void cmd_lpop(CmdContext& ctx)   { ListStore::lpop(ctx); }
static void cmd_rpop(CmdContext& ctx)   { ListStore::rpop(ctx); }
static void cmd_llen(CmdContext& ctx)   { ListStore::llen(ctx); }
static void cmd_lrange(CmdContext& ctx) { ListStore::lrange(ctx); }
static void cmd_lindex(CmdContext& ctx) { ListStore::lindex(ctx); }
static void cmd_lset(CmdContext& ctx)   { ListStore::lset(ctx); }
static void cmd_ltrim(CmdContext& ctx)  { ListStore::ltrim(ctx); }

// ---- Set ----
static void cmd_sadd(CmdContext& ctx)      { SetStore::sadd(ctx); }
static void cmd_srem(CmdContext& ctx)      { SetStore::srem(ctx); }
static void cmd_smembers(CmdContext& ctx)  { SetStore::smembers(ctx); }
static void cmd_sismember(CmdContext& ctx) { SetStore::sismember(ctx); }
static void cmd_scard(CmdContext& ctx)     { SetStore::scard(ctx); }
static void cmd_sinter(CmdContext& ctx)    { SetStore::sinter(ctx); }
static void cmd_sunion(CmdContext& ctx)    { SetStore::sunion(ctx); }
static void cmd_sdiff(CmdContext& ctx)     { SetStore::sdiff(ctx); }

// ---- ZSet ----
static void cmd_zadd(CmdContext& ctx)   { ZSetStore::zadd(ctx); }
static void cmd_zrem(CmdContext& ctx)   { ZSetStore::zrem(ctx); }
static void cmd_zscore(CmdContext& ctx) { ZSetStore::zscore(ctx); }
static void cmd_zcard(CmdContext& ctx)  { ZSetStore::zcard(ctx); }
static void cmd_zrank(CmdContext& ctx)  { ZSetStore::zrank(ctx); }
static void cmd_zrange(CmdContext& ctx) { ZSetStore::zrange(ctx); }
static void cmd_zcount(CmdContext& ctx) { ZSetStore::zcount(ctx); }

// ---- Transaction (由 executeCommand 直接处理) ----
static void cmd_multi(CmdContext& ctx)   { /* handled in executeCommand */ }
static void cmd_exec(CmdContext& ctx)    { /* handled in executeCommand */ }
static void cmd_discard(CmdContext& ctx) { /* handled in executeCommand */ }
static void cmd_watch(CmdContext& ctx)   { /* handled in executeCommand */ }
static void cmd_unwatch(CmdContext& ctx) { /* handled in executeCommand */ }

// ---- Persistence ----
static void cmd_save(CmdContext& ctx)         { ctx.replyOK(); }
static void cmd_bgsave(CmdContext& ctx)       { ctx.replyOK(); }
static void cmd_bgrewriteaof(CmdContext& ctx) { ctx.replyOK(); }

// ---- Pub/Sub ----
static void cmd_subscribe(CmdContext& ctx) {
    auto* pubsub = ctx.pubsub;
    if (!pubsub) { ctx.replyError("ERR Pub/Sub not available"); return; }
    for (size_t i = 1; i < ctx.args.size(); ++i) {
        std::string channel(ctx.args[i]);
        pubsub->subscribe(ctx.client, channel);
        // 回复: *3\r\n$9\r\nsubscribe\r\n$<ch>\r\n<channel>\r\n:<count>\r\n
        std::string& buf = *ctx.response_buf;
        buf += "*3\r\n$9\r\nsubscribe\r\n";
        RespWriter::writeBulkStringInline(buf, channel);
        RespWriter::writeInteger(buf, pubsub->subscriptionCount(ctx.client));
    }
}

static void cmd_psubscribe(CmdContext& ctx) {
    auto* pubsub = ctx.pubsub;
    if (!pubsub) { ctx.replyError("ERR Pub/Sub not available"); return; }
    for (size_t i = 1; i < ctx.args.size(); ++i) {
        std::string pattern(ctx.args[i]);
        pubsub->psubscribe(ctx.client, pattern);
        std::string& buf = *ctx.response_buf;
        buf += "*3\r\n$10\r\npsubscribe\r\n";
        RespWriter::writeBulkStringInline(buf, pattern);
        RespWriter::writeInteger(buf, pubsub->subscriptionCount(ctx.client));
    }
}

static void cmd_unsubscribe(CmdContext& ctx) {
    auto* pubsub = ctx.pubsub;
    if (!pubsub) { ctx.replyError("ERR Pub/Sub not available"); return; }
    if (ctx.args.size() == 1) {
        pubsub->unsubscribeAll(ctx.client);
    } else {
        for (size_t i = 1; i < ctx.args.size(); ++i)
            pubsub->unsubscribe(ctx.client, std::string(ctx.args[i]));
    }
    std::string& buf = *ctx.response_buf;
    buf += "*3\r\n$11\r\nunsubscribe\r\n$-1\r\n";
    RespWriter::writeInteger(buf, pubsub->subscriptionCount(ctx.client));
}

static void cmd_punsubscribe(CmdContext& ctx) {
    auto* pubsub = ctx.pubsub;
    if (!pubsub) { ctx.replyError("ERR Pub/Sub not available"); return; }
    pubsub->unsubscribeAll(ctx.client);
    std::string& buf = *ctx.response_buf;
    buf += "*3\r\n$12\r\npunsubscribe\r\n$-1\r\n";
    RespWriter::writeInteger(buf, pubsub->subscriptionCount(ctx.client));
}

static void cmd_publish(CmdContext& ctx) {
    auto* pubsub = ctx.pubsub;
    if (!pubsub) { ctx.replyError("ERR Pub/Sub not available"); return; }
    std::string channel(ctx.args[1]);
    std::string message(ctx.args[2]);
    int count = pubsub->publish(channel, message);
    ctx.replyInteger(count);
}

static void cmd_pubsub(CmdContext& ctx) {
    auto* pubsub = ctx.pubsub;
    if (!pubsub) { ctx.replyError("ERR Pub/Sub not available"); return; }
    auto& args = ctx.args;
    if (args.size() >= 2) {
        std::string sub(args[1]);
        if (sub == "CHANNELS" || sub == "channels") {
            ctx.replyEmptyArray(); // simplified: return empty
        } else if (sub == "NUMSUB" || sub == "numsub") {
            ctx.replyEmptyArray();
        } else if (sub == "NUMPAT" || sub == "numpat") {
            ctx.replyInteger(0);
        } else ctx.replyError("ERR Unknown PUBSUB subcommand");
    } else ctx.replyError("ERR wrong number of arguments");
}

// ---- New String commands ----
static void cmd_setnx(CmdContext& ctx)    { ctx.engine->setnx(ctx); }
static void cmd_setex(CmdContext& ctx)    { ctx.engine->setex(ctx); }
static void cmd_psetex(CmdContext& ctx)   { ctx.engine->psetex(ctx); }
static void cmd_getset(CmdContext& ctx)   { ctx.engine->getset(ctx); }
static void cmd_getrange(CmdContext& ctx) { ctx.engine->getrange(ctx); }
static void cmd_setrange(CmdContext& ctx) { ctx.engine->setrange(ctx); }
static void cmd_incrbyfloat(CmdContext& ctx) { ctx.engine->incrbyfloat(ctx); }
static void cmd_msetnx(CmdContext& ctx)   { ctx.engine->msetnx(ctx); }
static void cmd_getdel(CmdContext& ctx)   { ctx.engine->getdel(ctx); }

// ---- New Hash commands ----
static void cmd_hsetnx(CmdContext& ctx)   { HashStore::hsetnx(ctx); }
static void cmd_hmset(CmdContext& ctx)    { HashStore::hmset(ctx); }
static void cmd_hmget(CmdContext& ctx)    { HashStore::hmget(ctx); }
static void cmd_hincrbyfloat(CmdContext& ctx) { HashStore::hincrbyfloat(ctx); }

// ---- New List commands ----
static void cmd_lpushx(CmdContext& ctx)   { ListStore::lpushx(ctx); }
static void cmd_rpushx(CmdContext& ctx)   { ListStore::rpushx(ctx); }
static void cmd_rpoplpush(CmdContext& ctx){ ListStore::rpoplpush(ctx); }
static void cmd_lrem(CmdContext& ctx)     { ListStore::lrem(ctx); }
static void cmd_linsert(CmdContext& ctx)  { ListStore::linsert(ctx); }

// ---- New Set commands ----
static void cmd_srandmember(CmdContext& ctx) { SetStore::srandmember(ctx); }
static void cmd_spop(CmdContext& ctx)      { SetStore::spop(ctx); }
static void cmd_smove(CmdContext& ctx)     { SetStore::smove(ctx); }
static void cmd_smismember(CmdContext& ctx){ SetStore::smismember(ctx); }
static void cmd_sunionstore(CmdContext& ctx){ SetStore::sunionstore(ctx); }
static void cmd_sinterstore(CmdContext& ctx){ SetStore::sinterstore(ctx); }
static void cmd_sdiffstore(CmdContext& ctx) { SetStore::sdiffstore(ctx); }

// ---- New ZSet commands ----
static void cmd_zincrby(CmdContext& ctx)   { ZSetStore::zincrby(ctx); }
static void cmd_zrevrank(CmdContext& ctx)  { ZSetStore::zrevrank(ctx); }
static void cmd_zrevrange(CmdContext& ctx) { ZSetStore::zrevrange(ctx); }
static void cmd_zrangebyscore(CmdContext& ctx)    { ZSetStore::zrangebyscore(ctx); }
static void cmd_zrevrangebyscore(CmdContext& ctx) { ZSetStore::zrevrangebyscore(ctx); }
static void cmd_zremrangebyrank(CmdContext& ctx)  { ZSetStore::zremrangebyrank(ctx); }
static void cmd_zremrangebyscore(CmdContext& ctx) { ZSetStore::zremrangebyscore(ctx); }
static void cmd_zpopmin(CmdContext& ctx)   { ZSetStore::zpopmin(ctx); }
static void cmd_zpopmax(CmdContext& ctx)   { ZSetStore::zpopmax(ctx); }

// ---- Server commands ----
static void cmd_auth(CmdContext& ctx)      { ctx.replyOK(); }
static void cmd_time(CmdContext& ctx) {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto sec = std::chrono::duration_cast<std::chrono::seconds>(now).count();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(now).count() % 1000000;
    RespWriter::writeArrayHeader(*ctx.response_buf, 2);
    RespWriter::writeBulkString(*ctx.response_buf, std::to_string(sec));
    RespWriter::writeBulkString(*ctx.response_buf, std::to_string(us));
}
static void cmd_shutdown(CmdContext& ctx) {
    // Graceful shutdown: reply OK, caller handles process termination
    ctx.replyOK();
}
static void cmd_rename_cmd(CmdContext& ctx) { ctx.engine->rename(ctx); }
static void cmd_renamenx_cmd(CmdContext& ctx) { ctx.engine->renamenx(ctx); }

// SCAN cursor [MATCH pattern] [COUNT count]
static void cmd_scan(CmdContext& ctx) {
    int64_t cursor = 0;
    if (ctx.args.size() >= 2) {
        try { cursor = std::stoll(std::string(ctx.args[1])); } catch (...) {}
    }
    std::string pattern = "*";
    size_t count = 10;
    for (size_t i = 2; i + 1 < ctx.args.size(); i += 2) {
        std::string opt(ctx.args[i]);
        if (opt == "MATCH" || opt == "match") pattern = std::string(ctx.args[i+1]);
        else if (opt == "COUNT" || opt == "count") try { count = std::stoull(std::string(ctx.args[i+1])); } catch(...) {}
    }

    lstl::vector<std::string> keys;
    Dict& dict = ctx.engine->dict();
    Dict::Iterator it(&dict);
    size_t idx = 0, start = static_cast<size_t>(cursor), collected = 0;
    while (it.valid() && collected < count) {
        if (idx >= start && fnmatch(pattern.c_str(), it.key().c_str(), 0) == 0) {
            keys.push_back(it.key());
            collected++;
        }
        it.next(); idx++;
    }
    int64_t next_cursor = it.valid() ? static_cast<int64_t>(idx) : 0;
    std::string& buf = *ctx.response_buf;
    buf = "*2\r\n";
    RespWriter::writeBulkStringInline(buf, std::to_string(next_cursor));
    RespWriter::writeStringArray(buf, keys);
}

// CONFIG GET|SET|REWRITE
static void cmd_config(CmdContext& ctx) {
    auto& args = ctx.args;
    if (args.size() >= 2) {
        std::string sub(args[1]);
        if (sub == "GET" || sub == "get") {
            std::string pattern = args.size() >= 3 ? std::string(args[2]) : "*";
            // Build response: array of key-value pairs
            auto items = ctx.engine ? lstl::vector<std::pair<std::string,std::string>>() : lstl::vector<std::pair<std::string,std::string>>();
            // Simplified: return fixed set of config values
            lstl::vector<std::string> kv;
            kv.push_back("maxmemory"); kv.push_back("0");
            kv.push_back("maxmemory-policy"); kv.push_back("noeviction");
            kv.push_back("databases"); kv.push_back("16");
            kv.push_back("port"); kv.push_back("6379");
            RespWriter::writeStringArray(*ctx.response_buf, kv);
        } else if (sub == "SET" || sub == "set") {
            if (args.size() >= 4) ctx.replyOK();
            else ctx.replyError("ERR wrong number of arguments for CONFIG SET");
        } else if (sub == "REWRITE" || sub == "rewrite") {
            ctx.replyOK();
        } else ctx.replyError("ERR Unknown CONFIG subcommand");
    } else ctx.replyError("ERR wrong number of arguments");
}

static void cmd_client(CmdContext& ctx) {
    auto& args = ctx.args;
    if (args.size() >= 2) {
        std::string sub(args[1]);
        if (sub == "LIST" || sub == "list") {
            char buf[256];
            snprintf(buf, sizeof(buf), "id=1 addr=%s name=ledis-cli age=0 idle=0",
                     ctx.client ? ctx.client->remote_addr.c_str() : "?");
            ctx.replyBulk(std::string(buf));
        } else if (sub == "SETNAME" || sub == "setname") {
            ctx.replyOK();
        } else if (sub == "GETNAME" || sub == "getname") {
            ctx.replyBulk(std::string_view{"ledis-cli"});
        } else if (sub == "KILL" || sub == "kill") {
            ctx.replyOK();
        } else ctx.replyOK();
    } else ctx.replyError("ERR wrong number of arguments");
}

static void cmd_slowlog(CmdContext& ctx) {
    auto& args = ctx.args;
    if (args.size() >= 2) {
        std::string sub(args[1]);
        if (sub == "GET" || sub == "get") {
            int count = 10;
            if (args.size() >= 3) try { count = std::stoi(std::string(args[2])); } catch(...) {}
            // Return empty array for now (real data in storage_worker.slowlog_)
            ctx.replyEmptyArray();
        } else if (sub == "RESET" || sub == "reset") {
            ctx.replyOK();
        } else if (sub == "LEN" || sub == "len") {
            ctx.replyInteger(0);
        } else ctx.replyError("ERR Unknown SLOWLOG subcommand");
    } else ctx.replyError("ERR wrong number of arguments");
}

static void cmd_monitor(CmdContext& ctx) { ctx.replyOK(); }

// ---- Blocking list commands ----
static void cmd_blpop(CmdContext& ctx)  { ListStore::blpop(ctx); }
static void cmd_brpop(CmdContext& ctx)  { ListStore::brpop(ctx); }

// ---- SORT ----
static void cmd_sort(CmdContext& ctx) {
    // SORT key [BY pattern] [LIMIT offset count] [ASC|DESC] [ALPHA] [STORE dst]
    if (ctx.args.size() < 2) { ctx.replyError("ERR wrong number of arguments"); return; }
    std::string key(ctx.args[1]);
    Value* v = ctx.engine->find(key);
    if (!v || v->isExpired(ctx.nowMs())) { ctx.replyEmptyArray(); return; }

    lstl::vector<std::string> result;
    if (v->type == ValueType::LIST) {
        for (auto& e : v->asList()->elements) result.push_back(e);
    } else if (v->type == ValueType::SET) {
        for (auto& e : v->asSet()->members) result.push_back(e);
    } else if (v->type == ValueType::ZSET) {
        for (auto& e : v->asZSet()->by_score) result.push_back(e.member);
    } else { ctx.replyEmptyArray(); return; }

    // Check for DESC flag
    bool desc = false, alpha = false;
    for (size_t i = 2; i < ctx.args.size(); ++i) {
        std::string opt(ctx.args[i]);
        if (opt == "DESC" || opt == "desc") desc = true;
        else if (opt == "ALPHA" || opt == "alpha") alpha = true;
    }

    if (alpha) {
        std::sort(result.begin(), result.end());
    } else {
        std::sort(result.begin(), result.end(),
            [](const std::string& a, const std::string& b) {
                try { return std::stod(a) < std::stod(b); }
                catch(...) { return a < b; }
            });
    }
    if (desc) std::reverse(result.begin(), result.end());

    ctx.replyStringArray(result);
}

} // namespace ledis
