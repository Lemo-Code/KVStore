import type { ServerInfo, SlowLogEntry, CliCommand } from '@/types/redis'

export const mockServerInfo: Record<string, ServerInfo> = {
  'conn-1': {
    server: {
      redis_version: '7.2.4',
      redis_mode: 'standalone',
      os: 'Linux 6.8.0-117-generic x86_64',
      arch_bits: '64',
      process_id: '12345',
      tcp_port: '6379',
      uptime_in_seconds: '2592000',
      uptime_in_days: '30',
    },
    clients: {
      connected_clients: '48',
      maxclients: '10000',
      blocked_clients: '0',
      tracking_clients: '0',
    },
    memory: {
      used_memory_human: '2.45G',
      used_memory_rss_human: '2.68G',
      maxmemory_human: '4.00G',
      mem_fragmentation_ratio: '1.09',
      used_memory_peak_human: '3.12G',
    },
    stats: {
      total_connections_received: '125430',
      total_commands_processed: '89562341',
      instantaneous_ops_per_sec: '1523',
      keyspace_hits: '78456230',
      keyspace_misses: '2345678',
      hit_rate: '97.1%',
      evicted_keys: '0',
      expired_keys: '12450',
    },
    replication: {
      role: 'master',
      connected_slaves: '2',
      slave0: 'ip=10.0.1.51,port=6379,state=online,offset=9876543210',
      slave1: 'ip=10.0.1.52,port=6379,state=online,offset=9876543210',
    },
    cpu: {
      used_cpu_sys: '1245.23',
      used_cpu_user: '892.45',
    },
    cluster: { cluster_enabled: '0' },
    keyspace: {
      db0: 'keys=48,expires=15,avg_ttl=3600000',
      db1: 'keys=12,expires=3,avg_ttl=1800000',
    },
  },
}

export const mockSlowLog: SlowLogEntry[] = [
  { id: 127, timestamp: '2026-06-22T10:28:15Z', duration: 125000, command: 'KEYS user:*', clientIp: '192.168.1.100:52341' },
  { id: 126, timestamp: '2026-06-22T10:25:30Z', duration: 98000, command: 'ZRANGEBYSCORE leaderboard:alltime 0 100000 LIMIT 0 1000', clientIp: '192.168.1.50:42123' },
  { id: 125, timestamp: '2026-06-22T09:50:12Z', duration: 85000, command: 'HGETALL product:5001:info', clientIp: '10.0.1.20:33012' },
  { id: 124, timestamp: '2026-06-22T08:15:00Z', duration: 72000, command: 'SMEMBERS rt:active_users', clientIp: '192.168.1.80:56234' },
  { id: 123, timestamp: '2026-06-21T22:30:45Z', duration: 156000, command: 'SORT product:hot_rank BY *->score DESC', clientIp: '10.0.1.10:45231' },
]

export const mockCliCommands: CliCommand[] = [
  { command: 'SET', description: '设置一个 key 的值', group: 'String', syntax: 'SET key value [EX seconds] [NX|XX]' },
  { command: 'GET', description: '获取一个 key 的值', group: 'String', syntax: 'GET key' },
  { command: 'DEL', description: '删除一个或多个 key', group: 'Generic', syntax: 'DEL key [key ...]' },
  { command: 'EXISTS', description: '检查 key 是否存在', group: 'Generic', syntax: 'EXISTS key' },
  { command: 'EXPIRE', description: '设置 key 的过期时间（秒）', group: 'Generic', syntax: 'EXPIRE key seconds' },
  { command: 'TTL', description: '查看 key 的剩余生存时间', group: 'Generic', syntax: 'TTL key' },
  { command: 'TYPE', description: '查看 key 的数据类型', group: 'Generic', syntax: 'TYPE key' },
  { command: 'KEYS', description: '查找匹配模式的 key', group: 'Generic', syntax: 'KEYS pattern' },
  { command: 'SCAN', description: '增量迭代 key', group: 'Generic', syntax: 'SCAN cursor [MATCH pattern] [COUNT count]' },
  { command: 'HGET', description: '获取 Hash 中 field 的值', group: 'Hash', syntax: 'HGET key field' },
  { command: 'HSET', description: '设置 Hash 中 field 的值', group: 'Hash', syntax: 'HSET key field value' },
  { command: 'HGETALL', description: '获取 Hash 中所有 field 和 value', group: 'Hash', syntax: 'HGETALL key' },
  { command: 'LPUSH', description: '从列表左侧插入元素', group: 'List', syntax: 'LPUSH key value [value ...]' },
  { command: 'LRANGE', description: '获取列表指定范围的元素', group: 'List', syntax: 'LRANGE key start stop' },
  { command: 'SADD', description: '向集合添加成员', group: 'Set', syntax: 'SADD key member [member ...]' },
  { command: 'SMEMBERS', description: '获取集合所有成员', group: 'Set', syntax: 'SMEMBERS key' },
  { command: 'ZADD', description: '向有序集合添加成员', group: 'ZSet', syntax: 'ZADD key score member' },
  { command: 'ZRANGE', description: '获取有序集合指定范围的成员', group: 'ZSet', syntax: 'ZRANGE key min max [BYSCORE]' },
  { command: 'XADD', description: '向 Stream 添加消息', group: 'Stream', syntax: 'XADD key * field value' },
  { command: 'XREAD', description: '读取 Stream 消息', group: 'Stream', syntax: 'XREAD COUNT n STREAMS key id' },
  { command: 'INFO', description: '获取服务器信息', group: 'Server', syntax: 'INFO [section]' },
  { command: 'CLIENT LIST', description: '列出连接的客户端', group: 'Server', syntax: 'CLIENT LIST' },
  { command: 'SLOWLOG', description: '查看慢查询日志', group: 'Server', syntax: 'SLOWLOG GET [count]' },
  { command: 'PUBLISH', description: '发布消息到频道', group: 'PubSub', syntax: 'PUBLISH channel message' },
  { command: 'SUBSCRIBE', description: '订阅频道', group: 'PubSub', syntax: 'SUBSCRIBE channel' },
  { command: 'DBSIZE', description: '返回当前数据库 key 的数量', group: 'Server', syntax: 'DBSIZE' },
  { command: 'FLUSHDB', description: '删除当前数据库所有 key', group: 'Server', syntax: 'FLUSHDB [ASYNC]' },
  { command: 'PING', description: '测试连接', group: 'Server', syntax: 'PING [message]' },
  { command: 'RENAME', description: '重命名 key', group: 'Generic', syntax: 'RENAME key newkey' },
]
