#include "kv_ledis/core/command.h"
#include "kv_ledis/core/storage_engine.h"

#include <cstring>
#include <algorithm>
#include <lstl/container/vector.h>

namespace ledis {

// ============================================================
// 命令注册表
// ============================================================

static lstl::vector<CmdInfo> g_commands;

void registerCommand(const char* name, CmdHandler handler,
                     int arity, uint32_t flags, const char* sflags) {
    g_commands.push_back({name, handler, arity, flags, sflags});
}

const CmdInfo* lookupCommand(std::string_view name) {
    auto it = std::lower_bound(g_commands.begin(), g_commands.end(), name,
        [](const CmdInfo& a, std::string_view b) {
            return std::strcmp(a.name, b.data()) < 0;
        });
    return (it != g_commands.end() && it->name == name) ? &(*it) : nullptr;
}

void dispatchCommand(CmdContext& ctx) {
    if (ctx.args.empty()) return;
    std::string_view cmd_name = ctx.args[0];

    // 命令名转小写 (Redis 协议大小写不敏感)
    std::string lower;
    lower.reserve(cmd_name.size());
    for (char c : cmd_name) lower += static_cast<char>(c | 0x20);

    const CmdInfo* info = lookupCommand(lower);
    if (!info) { ctx.replyUnknownCmd(cmd_name); return; }
    if (!info->checkArity(static_cast<int>(ctx.args.size()))) {
        ctx.replyWrongArgs(cmd_name); return;
    }
    info->handler(ctx);
}

// ============================================================
// 注册宏
// ============================================================

#define REG(method) [](CmdContext& ctx) { ctx.engine->method(ctx); }

#define SCMD(name, method, arity, flags, sflags) \
    registerCommand(name, REG(method), arity, flags, sflags)

// ============================================================
// 命令注册
// ============================================================

void initCommandTable() {
    // ---- String ----
    SCMD("append",      append,      3, CMD_WRITE, "wm");
    SCMD("decr",        decr,        2, CMD_WRITE | CMD_FAST, "wmF");
    SCMD("decrby",      decrby,      3, CMD_WRITE | CMD_FAST, "wmF");
    SCMD("del",         del,        -2, CMD_WRITE, "wm");
    SCMD("exists",      exists,     -2, CMD_READONLY | CMD_FAST, "rF");
    SCMD("expire",      expire,      3, CMD_WRITE | CMD_FAST, "wmF");
    SCMD("expireat",    expireat,    3, CMD_WRITE | CMD_FAST, "wmF");
    SCMD("get",         get,         2, CMD_READONLY | CMD_FAST, "rF");
    SCMD("getdel",      getdel,      2, CMD_WRITE | CMD_FAST, "wmF");
    SCMD("getrange",    getrange,    4, CMD_READONLY, "r");
    SCMD("getset",      getset,      3, CMD_WRITE | CMD_FAST, "wmF");
    SCMD("incr",        incr,        2, CMD_WRITE | CMD_FAST, "wmF");
    SCMD("incrby",      incrby,      3, CMD_WRITE | CMD_FAST, "wmF");
    SCMD("incrbyfloat", incrbyfloat, 3, CMD_WRITE, "wm");
    SCMD("mget",        mget,       -2, CMD_READONLY, "r");
    SCMD("mset",        mset,       -3, CMD_WRITE, "wm");
    SCMD("msetnx",      msetnx,     -3, CMD_WRITE, "wm");
    SCMD("persist",     persist,     2, CMD_WRITE | CMD_FAST, "wmF");
    SCMD("pexpire",     pexpire,     3, CMD_WRITE | CMD_FAST, "wmF");
    SCMD("pexpireat",   pexpireat,   3, CMD_WRITE | CMD_FAST, "wmF");
    SCMD("psetex",      psetex,      4, CMD_WRITE, "wm");
    SCMD("pttl",        pttl,        2, CMD_READONLY | CMD_FAST, "rF");
    SCMD("rename",      rename,      3, CMD_WRITE, "wm");
    SCMD("renamenx",    renamenx,    3, CMD_WRITE, "wm");
    SCMD("set",         set,        -3, CMD_WRITE, "wm");
    SCMD("setex",       setex,       4, CMD_WRITE, "wm");
    SCMD("setnx",       setnx,       3, CMD_WRITE, "wm");
    SCMD("setrange",    setrange,    4, CMD_WRITE, "wm");
    SCMD("strlen",      strlen,      2, CMD_READONLY | CMD_FAST, "rF");
    SCMD("ttl",         ttl,         2, CMD_READONLY | CMD_FAST, "rF");
    SCMD("type",        type,        2, CMD_READONLY | CMD_FAST, "rF");

    // ---- Hash ----
    SCMD("hdel",        hdel,       -3, CMD_WRITE | CMD_FAST, "wmF");
    SCMD("hexists",     hexists,     3, CMD_READONLY | CMD_FAST, "rF");
    SCMD("hget",        hget,        3, CMD_READONLY | CMD_FAST, "rF");
    SCMD("hgetall",     hgetall,     2, CMD_READONLY, "r");
    SCMD("hincrby",     hincrby,     4, CMD_WRITE | CMD_FAST, "wmF");
    SCMD("hincrbyfloat", hincrbyfloat, 4, CMD_WRITE, "wm");
    SCMD("hkeys",       hkeys,       2, CMD_READONLY, "r");
    SCMD("hlen",        hlen,        2, CMD_READONLY | CMD_FAST, "rF");
    SCMD("hmget",       hmget,      -3, CMD_READONLY, "r");
    SCMD("hmset",       hmset,      -4, CMD_WRITE, "wm");
    SCMD("hset",        hset,       -4, CMD_WRITE | CMD_FAST, "wmF");
    SCMD("hsetnx",      hsetnx,      4, CMD_WRITE | CMD_FAST, "wmF");
    SCMD("hvals",       hvals,       2, CMD_READONLY, "r");

    // ---- List ----
    SCMD("lindex",      lindex,      3, CMD_READONLY, "r");
    SCMD("llen",        llen,        2, CMD_READONLY | CMD_FAST, "rF");
    SCMD("lpop",        lpop,       -2, CMD_WRITE | CMD_FAST, "wmF");
    SCMD("lpush",       lpush,      -3, CMD_WRITE | CMD_FAST, "wmF");
    SCMD("lrange",      lrange,      4, CMD_READONLY, "r");
    SCMD("lrem",        lrem,        4, CMD_WRITE, "wm");
    SCMD("lset",        lset,        4, CMD_WRITE, "wm");
    SCMD("ltrim",       ltrim,       4, CMD_WRITE, "wm");
    SCMD("rpop",        rpop,       -2, CMD_WRITE | CMD_FAST, "wmF");
    SCMD("rpush",       rpush,      -3, CMD_WRITE | CMD_FAST, "wmF");

    // ---- Set ----
    SCMD("sadd",        sadd,       -3, CMD_WRITE | CMD_FAST, "wmF");
    SCMD("scard",       scard,       2, CMD_READONLY | CMD_FAST, "rF");
    SCMD("sismember",   sismember,   3, CMD_READONLY | CMD_FAST, "rF");
    SCMD("smembers",    smembers,    2, CMD_READONLY, "r");
    SCMD("spop",        spop,       -2, CMD_WRITE | CMD_FAST, "wmF");
    SCMD("srandmember", srandmember, 2, CMD_READONLY, "r");
    SCMD("srem",        srem,       -3, CMD_WRITE | CMD_FAST, "wmF");

    // ---- ZSet ----
    SCMD("zadd",        zadd,       -4, CMD_WRITE | CMD_FAST, "wmF");
    SCMD("zcard",       zcard,       2, CMD_READONLY | CMD_FAST, "rF");
    SCMD("zcount",      zcount,      4, CMD_READONLY, "r");
    SCMD("zincrby",     zincrby,     4, CMD_WRITE | CMD_FAST, "wmF");
    SCMD("zrange",      zrange,     -4, CMD_READONLY, "r");
    SCMD("zrangebyscore", zrangebyscore, -4, CMD_READONLY, "r");
    SCMD("zrank",       zrank,       3, CMD_READONLY | CMD_FAST, "rF");
    SCMD("zrem",        zrem,       -3, CMD_WRITE | CMD_FAST, "wmF");
    SCMD("zremrangebyrank", zremrangebyrank, 4, CMD_WRITE, "wm");
    SCMD("zremrangebyscore", zremrangebyscore, 4, CMD_WRITE, "wm");
    SCMD("zrevrange",   zrevrange,  -4, CMD_READONLY, "r");
    SCMD("zrevrank",    zrevrank,    3, CMD_READONLY | CMD_FAST, "rF");
    SCMD("zscore",      zscore,      3, CMD_READONLY | CMD_FAST, "rF");

    // ---- Server ----
    SCMD("dbsize",      cmdDbsize,   1, CMD_READONLY | CMD_FAST, "rF");
    SCMD("flushdb",     cmdFlushdb,  1, CMD_WRITE, "wm");
    SCMD("keys",        cmdKeys,     2, CMD_READONLY, "r");
    SCMD("randomkey",   cmdRandomkey, 1, CMD_READONLY, "r");

    // ---- Pub/Sub ----
    SCMD("publish",     cmdPublish,   3, CMD_PUBSUB, "w");
    SCMD("pubsub",      cmdPubsub,   -2, CMD_READONLY, "r");
    SCMD("subscribe",   cmdSubscribe, -2, CMD_PUBSUB, "w");
    SCMD("unsubscribe", cmdUnsubscribe, -1, CMD_PUBSUB, "w");
    SCMD("psubscribe",  cmdPsubscribe, -2, CMD_PUBSUB, "w");
    SCMD("punsubscribe",cmdPunsubscribe, -1, CMD_PUBSUB, "w");

    // ---- Scan ----
    SCMD("scan",        cmdScan,     -2, CMD_READONLY, "r");
    SCMD("hscan",       cmdHscan,    -3, CMD_READONLY, "r");
    SCMD("sscan",       cmdSscan,    -3, CMD_READONLY, "r");
    SCMD("zscan",       cmdZscan,    -3, CMD_READONLY, "r");

    // ---- Bitmap ----
    SCMD("setbit",      setbit,       4, CMD_WRITE, "wm");
    SCMD("getbit",      getbit,       3, CMD_READONLY, "r");
    SCMD("bitcount",    bitcount,    -2, CMD_READONLY, "r");
    SCMD("bitop",       bitop,       -4, CMD_WRITE, "wm");
    SCMD("bitpos",      bitpos,      -3, CMD_READONLY, "r");

    // ---- Set ops ----
    SCMD("sinter",      sinter,      -2, CMD_READONLY, "r");
    SCMD("sinterstore", sinterstore, -3, CMD_WRITE, "wm");
    SCMD("sunion",      sunion,      -2, CMD_READONLY, "r");
    SCMD("sunionstore", sunionstore, -3, CMD_WRITE, "wm");
    SCMD("sdiff",       sdiff,       -2, CMD_READONLY, "r");
    SCMD("sdiffstore",  sdiffstore,  -3, CMD_WRITE, "wm");
    SCMD("smismember",  smismember,  -3, CMD_READONLY, "r");

    // ---- ZSet extras ----
    SCMD("bzpopmin",    zpopmin,     -3, CMD_WRITE, "wm");
    SCMD("bzpopmax",    zpopmax,     -3, CMD_WRITE, "wm");
    SCMD("zpopmin",     zpopmin,     -2, CMD_WRITE, "wm");
    SCMD("zpopmax",     zpopmax,     -2, CMD_WRITE, "wm");
    SCMD("zrandmember", zrandmember, -2, CMD_READONLY, "r");
    SCMD("zinter",      zinter,      -3, CMD_READONLY, "r");
    SCMD("zunion",      zunion,      -3, CMD_READONLY, "r");
    SCMD("zlexcount",   zlexcount,    4, CMD_READONLY, "r");
    SCMD("zrangebylex", zrangebylex, -4, CMD_READONLY, "r");

    // ---- Hash extras ----
    SCMD("hrandfield",  hrandfield,  -2, CMD_READONLY, "r");
    SCMD("hstrlen",     hstrlen,      3, CMD_READONLY, "r");

    // ---- List extras ----
    SCMD("lpos",        lpos,        -3, CMD_READONLY, "r");
    SCMD("lmove",       lmove,        5, CMD_WRITE, "wm");

    // ---- HyperLogLog ----
    SCMD("pfadd",       pfadd,       -2, CMD_WRITE, "wm");
    SCMD("pfcount",     pfcount,     -2, CMD_READONLY, "r");
    SCMD("pfmerge",     pfmerge,     -3, CMD_WRITE, "wm");

    // ---- Geo ----
    SCMD("geoadd",      geoadd,      -5, CMD_WRITE, "wm");
    SCMD("geodist",     geodist,     -4, CMD_READONLY, "r");
    SCMD("geohash",     geohash,     -2, CMD_READONLY, "r");
    SCMD("geopos",      geopos,      -2, CMD_READONLY, "r");
    SCMD("georadius",   georadius,   -5, CMD_READONLY, "r");

    // ---- Sort ----
    SCMD("sort",        cmdSort,     -2, CMD_READONLY, "r");

    // ---- Stream ----
    SCMD("xadd",        xadd,        -4, CMD_WRITE, "wm");
    SCMD("xdel",        xdel,        -3, CMD_WRITE, "wm");
    SCMD("xlen",        xlen,         2, CMD_READONLY, "r");
    SCMD("xrange",      xrange,      -4, CMD_READONLY, "r");
    SCMD("xread",       xread,       -4, CMD_READONLY, "r");

    // ---- Server management ----
    SCMD("config",      cmdConfig,   -2, 0, "w");
    SCMD("info",        cmdInfo,     -1, CMD_READONLY, "r");
    SCMD("client",      cmdClient,   -2, 0, "w");
    SCMD("shutdown",    cmdShutdown, -1, 0, "w");
    SCMD("monitor",     cmdMonitor,   1, 0, "w");
    SCMD("slowlog",     cmdSlowlog,  -2, CMD_READONLY, "r");
    SCMD("memory",      cmdMemory,   -1, CMD_READONLY, "r");
    SCMD("bgsave",      cmdBgsave,    1, CMD_WRITE, "w");
    SCMD("copy",        cmdCopy,      3, CMD_WRITE, "wm");
    SCMD("command",     cmdCommand,  -1, CMD_READONLY, "r");
    SCMD("expiretime",  cmdExpiretime, 2, CMD_READONLY, "r");
    SCMD("hello",       cmdHello,    -1, 0, "w");
    SCMD("memory",      cmdMemory,   -1, CMD_READONLY, "r");
    SCMD("object",      cmdObject,   -3, CMD_READONLY, "r");
    SCMD("pexpiretime", cmdPexpiretime, 2, CMD_READONLY, "r");
    SCMD("restore",     cmdRestore,   -4, CMD_WRITE, "wm");
    SCMD("save",        cmdSave,      1, CMD_WRITE, "w");
    SCMD("select",      cmdSelect,    2, 0, "w");
    SCMD("smove",       smove,        4, CMD_WRITE, "wm");
    SCMD("time",        cmdTime,      1, CMD_READONLY, "r");
    SCMD("touch",       cmdTouch,    -2, CMD_READONLY, "r");
    SCMD("zdiff",       zdiff,       -3, CMD_READONLY, "r");
    SCMD("zdiffstore",  zdiffstore,  -4, CMD_WRITE, "wm");
    SCMD("zinterstore", zinterstore, -4, CMD_WRITE, "wm");
    SCMD("zremrangebylex", zremrangebylex, 4, CMD_WRITE, "wm");
    SCMD("zrevrangebyscore", zrevrangebyscore, -4, CMD_READONLY, "r");
    SCMD("zunionstore", zunionstore, -4, CMD_WRITE, "wm");

    // 排序 (二分查找要求)
    std::sort(g_commands.begin(), g_commands.end(),
        [](const CmdInfo& a, const CmdInfo& b) {
            return std::strcmp(a.name, b.name) < 0;
        });
}

} // namespace ledis
