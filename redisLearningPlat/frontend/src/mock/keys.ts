import type { RedisKey, RedisValue } from '@/types/redis'

export const mockKeys: RedisKey[] = [
  // User cache keys
  { name: 'user:1001:profile', type: 'hash', ttl: 3600, size: 512, db: 0 },
  { name: 'user:1001:session', type: 'string', ttl: 1800, size: 128, db: 0 },
  { name: 'user:1001:cart', type: 'hash', ttl: 86400, size: 1024, db: 0 },
  { name: 'user:1002:profile', type: 'hash', ttl: 3600, size: 480, db: 0 },
  { name: 'user:1002:session', type: 'string', ttl: 1800, size: 128, db: 0 },
  { name: 'user:1003:profile', type: 'hash', ttl: 3600, size: 520, db: 0 },

  // Product cache
  { name: 'product:5001:info', type: 'hash', ttl: 7200, size: 2048, db: 0 },
  { name: 'product:5001:stock', type: 'string', ttl: 300, size: 32, db: 0 },
  { name: 'product:5001:reviews', type: 'list', ttl: -1, size: 8192, db: 0 },
  { name: 'product:5002:info', type: 'hash', ttl: 7200, size: 1536, db: 0 },
  { name: 'product:5002:stock', type: 'string', ttl: 300, size: 32, db: 0 },
  { name: 'product:5002:images', type: 'list', ttl: -1, size: 4096, db: 0 },
  { name: 'product:categories', type: 'set', ttl: -1, size: 256, db: 0 },
  { name: 'product:hot_rank', type: 'zset', ttl: -1, size: 1024, db: 0 },

  // Order cache
  { name: 'order:ORD-2024001', type: 'hash', ttl: 259200, size: 3072, db: 0 },
  { name: 'order:ORD-2024002', type: 'hash', ttl: 259200, size: 2560, db: 0 },
  { name: 'order:ORD-2024003', type: 'hash', ttl: 259200, size: 4096, db: 0 },
  { name: 'order:pending', type: 'set', ttl: -1, size: 128, db: 0 },
  { name: 'order:daily_count', type: 'string', ttl: 86400, size: 16, db: 0 },

  // Rate limiting
  { name: 'ratelimit:api:login:192.168.1.1', type: 'string', ttl: 60, size: 16, db: 0 },
  { name: 'ratelimit:api:search:10.0.0.5', type: 'string', ttl: 60, size: 16, db: 0 },
  { name: 'ratelimit:api:upload:192.168.1.100', type: 'string', ttl: 300, size: 16, db: 0 },

  // Session store
  { name: 'session:sess_abc123def456', type: 'hash', ttl: 1800, size: 768, db: 0 },
  { name: 'session:sess_ghi789jkl012', type: 'hash', ttl: 1800, size: 640, db: 0 },
  { name: 'session:sess_mno345pqr678', type: 'hash', ttl: 1200, size: 512, db: 0 },

  // Cache keys
  { name: 'cache:api:user_list:v1', type: 'string', ttl: 300, size: 12288, db: 0 },
  { name: 'cache:api:product_feed:v2', type: 'string', ttl: 600, size: 24576, db: 0 },
  { name: 'cache:db:user_stats', type: 'hash', ttl: 3600, size: 4096, db: 0 },
  { name: 'cache:html:index_page', type: 'string', ttl: 1800, size: 32768, db: 0 },

  // Real-time data
  { name: 'rt:active_users', type: 'set', ttl: 300, size: 512, db: 0 },
  { name: 'rt:page_views:20260622', type: 'string', ttl: 86400, size: 64, db: 0 },
  { name: 'rt:ws_connections', type: 'zset', ttl: -1, size: 256, db: 0 },

  // Queue / Stream
  { name: 'stream:notifications', type: 'stream', ttl: -1, size: 16384, db: 0 },
  { name: 'stream:audit_log', type: 'stream', ttl: 604800, size: 65536, db: 0 },
  { name: 'queue:email:pending', type: 'list', ttl: -1, size: 4096, db: 0 },
  { name: 'queue:sms:failed', type: 'list', ttl: 86400, size: 1024, db: 0 },

  // Leaderboard
  { name: 'leaderboard:weekly', type: 'zset', ttl: 604800, size: 2048, db: 0 },
  { name: 'leaderboard:monthly', type: 'zset', ttl: 2592000, size: 3072, db: 0 },
  { name: 'leaderboard:alltime', type: 'zset', ttl: -1, size: 8192, db: 0 },

  // Config
  { name: 'config:app:settings', type: 'hash', ttl: -1, size: 2048, db: 0 },
  { name: 'config:feature_flags', type: 'hash', ttl: -1, size: 1024, db: 0 },
  { name: 'config:email_templates', type: 'hash', ttl: -1, size: 4096, db: 0 },

  // Locks
  { name: 'lock:order_create', type: 'string', ttl: 30, size: 32, db: 0 },
  { name: 'lock:inventory_update', type: 'string', ttl: 30, size: 32, db: 0 },
  { name: 'lock:batch_job:cleanup', type: 'string', ttl: 300, size: 32, db: 0 },

  // Geo / Location
  { name: 'geo:stores', type: 'zset', ttl: -1, size: 512, db: 0 },
  { name: 'geo:drivers:nearby', type: 'zset', ttl: 60, size: 256, db: 0 },

  // Counter
  { name: 'counter:page_hits', type: 'string', ttl: -1, size: 16, db: 0 },
  { name: 'counter:api_calls', type: 'string', ttl: 86400, size: 16, db: 0 },
  { name: 'counter:user_signups', type: 'string', ttl: -1, size: 16, db: 0 },
]

export function getKeysByConnection(connectionId: string, db: number, pattern?: string): RedisKey[] {
  let keys = mockKeys.filter((k) => k.db === db)
  if (pattern) {
    const regex = new RegExp(
      '^' + pattern.replace(/\*/g, '.*').replace(/\?/g, '.').replace(/\[/g, '\\[') + '$',
    )
    keys = keys.filter((k) => regex.test(k.name))
  }
  return keys
}

export type { RedisValue }
