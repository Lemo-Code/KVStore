import type { StringValue, HashValue, ListValue, SetValue, ZSetValue, StreamValue } from '@/types/redis'

export const mockStringValues: Record<string, StringValue> = {
  'user:1001:session': {
    key: 'user:1001:session', type: 'string',
    value: 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1c2VySWQiOjEwMDEsInVzZXJuYW1lIjoiYWxpY2UiLCJleHAiOjE3MTkwNTk2MDB9.abc123xyz',
    ttl: 1800, encoding: 'raw',
  },
  'product:5001:stock': {
    key: 'product:5001:stock', type: 'string',
    value: '1280', ttl: 300, encoding: 'int',
  },
  'product:5002:stock': {
    key: 'product:5002:stock', type: 'string',
    value: '56', ttl: 300, encoding: 'int',
  },
  'cache:api:user_list:v1': {
    key: 'cache:api:user_list:v1', type: 'string',
    value: JSON.stringify([{ id: 1, name: 'Alice' }, { id: 2, name: 'Bob' }, { id: 3, name: 'Charlie' }], null, 2),
    ttl: 300, encoding: 'raw',
  },
  'cache:html:index_page': {
    key: 'cache:html:index_page', type: 'string',
    value: '<!DOCTYPE html><html><head><title>Cached Page</title></head><body><h1>Welcome</h1></body></html>',
    ttl: 1800, encoding: 'raw',
  },
  'lock:order_create': {
    key: 'lock:order_create', type: 'string',
    value: 'locked_by_worker_3_1719059600', ttl: 30, encoding: 'raw',
  },
  'counter:page_hits': {
    key: 'counter:page_hits', type: 'string',
    value: '1528473', ttl: -1, encoding: 'int',
  },
  'counter:api_calls': {
    key: 'counter:api_calls', type: 'string',
    value: '89124', ttl: 86400, encoding: 'int',
  },
  'order:daily_count': {
    key: 'order:daily_count', type: 'string',
    value: '342', ttl: 86400, encoding: 'int',
  },
  'ratelimit:api:login:192.168.1.1': {
    key: 'ratelimit:api:login:192.168.1.1', type: 'string',
    value: '4', ttl: 60, encoding: 'int',
  },
  'rt:page_views:20260622': {
    key: 'rt:page_views:20260622', type: 'string',
    value: '48291', ttl: 86400, encoding: 'int',
  },
}

export const mockHashValues: Record<string, HashValue> = {
  'user:1001:profile': {
    key: 'user:1001:profile', type: 'hash',
    fields: [
      { field: 'username', value: 'alice' },
      { field: 'email', value: 'alice@example.com' },
      { field: 'nickname', value: 'Alice Wang' },
      { field: 'avatar', value: 'https://cdn.example.com/avatars/alice.jpg' },
      { field: 'level', value: '42' },
      { field: 'points', value: '12890' },
      { field: 'created_at', value: '2025-01-15T10:30:00Z' },
    ],
    ttl: 3600, length: 7,
  },
  'product:5001:info': {
    key: 'product:5001:info', type: 'hash',
    fields: [
      { field: 'name', value: 'Redis 实战指南（签名版）' },
      { field: 'price', value: '89.00' },
      { field: 'category', value: 'books' },
      { field: 'author', value: 'Redis 核心团队' },
      { field: 'isbn', value: '978-7-111-70001-2' },
      { field: 'stock', value: '1280' },
      { field: 'rating', value: '4.8' },
      { field: 'sales', value: '15420' },
    ],
    ttl: 7200, length: 8,
  },
  'order:ORD-2024001': {
    key: 'order:ORD-2024001', type: 'hash',
    fields: [
      { field: 'user_id', value: '1001' },
      { field: 'total', value: '267.00' },
      { field: 'status', value: 'shipped' },
      { field: 'items', value: '3' },
      { field: 'address', value: '上海市浦东新区张江高科技园区' },
      { field: 'created_at', value: '2026-06-20T14:22:00Z' },
    ],
    ttl: 259200, length: 6,
  },
  'session:sess_abc123def456': {
    key: 'session:sess_abc123def456', type: 'hash',
    fields: [
      { field: 'user_id', value: '1001' },
      { field: 'ip', value: '192.168.1.100' },
      { field: 'user_agent', value: 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7)' },
      { field: 'last_activity', value: '1719059600' },
      { field: 'csrf_token', value: 'tok_abc123def456' },
    ],
    ttl: 1800, length: 5,
  },
  'config:app:settings': {
    key: 'config:app:settings', type: 'hash',
    fields: [
      { field: 'max_connections', value: '10000' },
      { field: 'timeout_ms', value: '5000' },
      { field: 'cache_enabled', value: 'true' },
      { field: 'cache_ttl_seconds', value: '3600' },
      { field: 'log_level', value: 'info' },
      { field: 'enable_metrics', value: 'true' },
      { field: 'max_retry', value: '3' },
    ],
    ttl: -1, length: 7,
  },
  'cache:db:user_stats': {
    key: 'cache:db:user_stats', type: 'hash',
    fields: [
      { field: 'total_users', value: '125430' },
      { field: 'active_today', value: '8921' },
      { field: 'new_today', value: '156' },
      { field: 'premium_users', value: '3420' },
    ],
    ttl: 3600, length: 4,
  },
}

export const mockListValues: Record<string, ListValue> = {
  'product:5001:reviews': {
    key: 'product:5001:reviews', type: 'list',
    values: [
      { index: 0, value: '{"rating":5,"text":"非常棒的Redis学习书籍，强烈推荐！","user":"bookworm"}' },
      { index: 1, value: '{"rating":4,"text":"内容很全面，适合进阶学习","user":"coder42"}' },
      { index: 2, value: '{"rating":5,"text":"实战案例丰富，受益匪浅","user":"redis_fan"}' },
      { index: 3, value: '{"rating":3,"text":"有些章节需要更多示例代码","user":"learner99"}' },
      { index: 4, value: '{"rating":5,"text":"签名版值得收藏","user":"collector"}' },
    ],
    ttl: -1, length: 5,
  },
  'queue:email:pending': {
    key: 'queue:email:pending', type: 'list',
    values: [
      { index: 0, value: '{"to":"user1@example.com","template":"welcome","data":{"name":"Alice"}}' },
      { index: 1, value: '{"to":"user2@example.com","template":"order_confirmed","data":{"order_id":"ORD-2024001"}}' },
      { index: 2, value: '{"to":"user3@example.com","template":"password_reset","data":{"token":"reset_abc"}}' },
      { index: 3, value: '{"to":"user4@example.com","template":"weekly_digest","data":{"week":24}}' },
      { index: 4, value: '{"to":"user5@example.com","template":"promotion","data":{"coupon":"SAVE20"}}' },
      { index: 5, value: '{"to":"user6@example.com","template":"order_shipped","data":{"order_id":"ORD-2024002"}}' },
    ],
    ttl: -1, length: 6,
  },
}

export const mockSetValues: Record<string, SetValue> = {
  'product:categories': {
    key: 'product:categories', type: 'set',
    members: ['books', 'electronics', 'clothing', 'food', 'sports', 'music', 'software', 'gaming'],
    ttl: -1, length: 8,
  },
  'rt:active_users': {
    key: 'rt:active_users', type: 'set',
    members: ['user_1001', 'user_1002', 'user_1005', 'user_1010', 'user_1023', 'user_1042', 'user_1056', 'user_1089'],
    ttl: 300, length: 8,
  },
  'order:pending': {
    key: 'order:pending', type: 'set',
    members: ['ORD-2024004', 'ORD-2024005', 'ORD-2024006'],
    ttl: -1, length: 3,
  },
}

export const mockZSetValues: Record<string, ZSetValue> = {
  'product:hot_rank': {
    key: 'product:hot_rank', type: 'zset',
    members: [
      { member: 'product:5001', score: 9850.5 },
      { member: 'product:5002', score: 8720.0 },
      { member: 'product:5003', score: 7650.25 },
      { member: 'product:5004', score: 6540.0 },
      { member: 'product:5005', score: 5430.75 },
      { member: 'product:5006', score: 4320.5 },
      { member: 'product:5007', score: 3210.0 },
      { member: 'product:5008', score: 2100.25 },
    ],
    ttl: -1, length: 8,
  },
  'leaderboard:weekly': {
    key: 'leaderboard:weekly', type: 'zset',
    members: [
      { member: 'user:1042', score: 9850 },
      { member: 'user:1005', score: 9340 },
      { member: 'user:1089', score: 8720 },
      { member: 'user:1023', score: 8150 },
      { member: 'user:1010', score: 7980 },
      { member: 'user:1056', score: 7420 },
      { member: 'user:1001', score: 6890 },
      { member: 'user:1002', score: 6210 },
    ],
    ttl: 604800, length: 8,
  },
  'geo:stores': {
    key: 'geo:stores', type: 'zset',
    members: [
      { member: 'store:001:121.4737:31.2304', score: 0 },
      { member: 'store:002:121.5015:31.2432', score: 1 },
      { member: 'store:003:121.4490:31.2178', score: 2 },
      { member: 'store:004:121.4890:31.2567', score: 3 },
    ],
    ttl: -1, length: 4,
  },
}

export const mockStreamValues: Record<string, StreamValue> = {
  'stream:notifications': {
    key: 'stream:notifications', type: 'stream',
    messages: [
      { id: '1719059600000-0', fields: { type: 'order_update', user_id: '1001', message: '您的订单 ORD-2024001 已发货' } },
      { id: '1719059600000-1', fields: { type: 'promotion', user_id: 'all', message: '限时优惠：全场8折' } },
      { id: '1719059600001-0', fields: { type: 'system', user_id: 'all', message: '系统将于周日凌晨2点维护' } },
      { id: '1719059600002-0', fields: { type: 'order_update', user_id: '1002', message: '您的订单 ORD-2024002 已确认' } },
    ],
    consumerGroups: [
      { name: 'notification_workers', consumers: 3, pending: 0 },
      { name: 'email_sender', consumers: 2, pending: 5 },
    ],
    ttl: -1, length: 4,
  },
  'stream:audit_log': {
    key: 'stream:audit_log', type: 'stream',
    messages: [
      { id: '1719059600000-0', fields: { action: 'login', user: 'admin', ip: '10.0.1.50', result: 'success' } },
      { id: '1719059600001-0', fields: { action: 'delete_key', user: 'admin', key: 'temp:cache:old', result: 'success' } },
      { id: '1719059600002-0', fields: { action: 'config_change', user: 'ops', setting: 'maxmemory', old: '2gb', new: '4gb' } },
    ],
    consumerGroups: [{ name: 'audit_archiver', consumers: 1, pending: 0 }],
    ttl: 604800, length: 3,
  },
}

export function getValueByKey(key: string): typeof mockStringValues[string] | typeof mockHashValues[string] | typeof mockListValues[string] | typeof mockSetValues[string] | typeof mockZSetValues[string] | typeof mockStreamValues[string] | undefined {
  return (
    mockStringValues[key] ||
    mockHashValues[key] ||
    mockListValues[key] ||
    mockSetValues[key] ||
    mockZSetValues[key] ||
    mockStreamValues[key]
  )
}
