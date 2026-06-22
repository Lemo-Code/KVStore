import { api } from './api'
import { useRedisStore } from '@/stores/redisStore'
import { mockConnections } from '@/mock/connections'
import { getKeysByConnection } from '@/mock/keys'
import { getValueByKey } from '@/mock/values'
import { mockServerInfo, mockSlowLog, mockCliCommands } from '@/mock/serverInfo'
import type { RedisConnection, RedisKey, RedisValue, ServerInfo, SlowLogEntry, CliCommand } from '@/types/redis'

function connCtx(connectionId?: string, db?: number) {
  const state = useRedisStore.getState()
  return { connectionId: connectionId || state.activeConnectionId || '', db: db ?? state.activeDb }
}

const API = '/redis'

// Try API first, fall back to mock
async function tryApi<T>(apiCall: () => Promise<T>, mock: T): Promise<T> {
  try { return await apiCall() } catch { return mock }
}

export const redisService = {
  // ---- Connections ----
  async getConnections(): Promise<RedisConnection[]> {
    return tryApi(() => api.get<RedisConnection[]>(`${API}/connections`), mockConnections)
  },
  async addConnection(conn: Omit<RedisConnection, 'id' | 'status' | 'createdAt'>): Promise<RedisConnection> {
    return api.post<RedisConnection>(`${API}/connections`, conn)
  },
  async updateConnection(id: string, updates: Partial<RedisConnection>): Promise<RedisConnection> {
    return api.put<RedisConnection>(`${API}/connections/${id}`, updates)
  },
  async removeConnection(id: string): Promise<void> {
    return api.delete(`${API}/connections/${id}`)
  },
  async testConnection(
    idOrConfig: string | { host: string; port: number; password?: string; ssl?: boolean }
  ): Promise<{ success: boolean; latency: number; version?: string }> {
    if (typeof idOrConfig === 'string') {
      return api.post(`${API}/connections/${idOrConfig}/test`)
    }
    return api.post(`${API}/connections/test`, idOrConfig)
  },

  // ---- Keys ----
  async getKeys(pattern?: string, connectionId?: string, db?: number): Promise<RedisKey[]> {
    const { connectionId: cid, db: d } = connCtx(connectionId, db)
    const mock = getKeysByConnection(cid, d, pattern)
    return tryApi(() => api.get<RedisKey[]>(`${API}/connections/${cid}/keys`, { db: String(d), ...(pattern ? { pattern } : {}) }), mock)
  },
  async getValue(key: string, connectionId?: string, db?: number): Promise<RedisValue | null> {
    const mock = getValueByKey(key) || null
    return tryApi(() => {
      const { connectionId: cid, db: d } = connCtx(connectionId, db)
      return api.get<RedisValue | null>(`${API}/connections/${cid}/keys/${encodeURIComponent(key)}`, { db: String(d) })
    }, mock)
  },
  async deleteKeys(keys: string[], connectionId?: string, db?: number): Promise<{ deleted: number }> {
    const { connectionId: cid, db: d } = connCtx(connectionId, db)
    return api.post<{ deleted: number }>(`${API}/connections/${cid}/keys/delete`, { db: d, keys })
  },
  async renameKey(oldKey: string, newKey: string, connectionId?: string, db?: number): Promise<void> {
    const { connectionId: cid, db: d } = connCtx(connectionId, db)
    return api.post(`${API}/connections/${cid}/keys/${encodeURIComponent(oldKey)}/rename`, { db: d, newKey })
  },
  async setKeyTTL(key: string, ttl: number, connectionId?: string, db?: number): Promise<void> {
    const { connectionId: cid, db: d } = connCtx(connectionId, db)
    return api.post(`${API}/connections/${cid}/keys/${encodeURIComponent(key)}/ttl`, { db: d, ttl })
  },

  // ---- Type-specific operations ----
  async setStringValue(key: string, value: string, connectionId?: string, db?: number): Promise<void> {
    const { connectionId: cid, db: d } = connCtx(connectionId, db)
    return api.put(`${API}/connections/${cid}/keys/${encodeURIComponent(key)}`, { db: d, type: 'string', value })
  },
  async hashSet(key: string, field: string, value: string, connectionId?: string, db?: number): Promise<void> {
    const { connectionId: cid, db: d } = connCtx(connectionId, db)
    return api.post(`${API}/connections/${cid}/keys/${encodeURIComponent(key)}/hash`, { db: d, field, value })
  },
  async hashDel(key: string, field: string, connectionId?: string, db?: number): Promise<void> {
    const { connectionId: cid, db: d } = connCtx(connectionId, db)
    return api.post(`${API}/connections/${cid}/keys/${encodeURIComponent(key)}/hash/delete`, { db: d, field })
  },
  async listPush(key: string, value: string, direction: 'left' | 'right', connectionId?: string, db?: number): Promise<void> {
    const { connectionId: cid, db: d } = connCtx(connectionId, db)
    return api.post(`${API}/connections/${cid}/keys/${encodeURIComponent(key)}/list`, { db: d, action: direction === 'left' ? 'lpush' : 'rpush', value })
  },
  async listRemove(key: string, index: number, connectionId?: string, db?: number): Promise<void> {
    const { connectionId: cid, db: d } = connCtx(connectionId, db)
    return api.post(`${API}/connections/${cid}/keys/${encodeURIComponent(key)}/list/delete`, { db: d, index })
  },
  async setAdd(key: string, member: string, connectionId?: string, db?: number): Promise<void> {
    const { connectionId: cid, db: d } = connCtx(connectionId, db)
    return api.post(`${API}/connections/${cid}/keys/${encodeURIComponent(key)}/set`, { db: d, member })
  },
  async setRemove(key: string, member: string, connectionId?: string, db?: number): Promise<void> {
    const { connectionId: cid, db: d } = connCtx(connectionId, db)
    return api.post(`${API}/connections/${cid}/keys/${encodeURIComponent(key)}/set/delete`, { db: d, member })
  },
  async zsetAdd(key: string, member: string, score: number, connectionId?: string, db?: number): Promise<void> {
    const { connectionId: cid, db: d } = connCtx(connectionId, db)
    return api.post(`${API}/connections/${cid}/keys/${encodeURIComponent(key)}/zset`, { db: d, member, score })
  },
  async zsetRemove(key: string, member: string, connectionId?: string, db?: number): Promise<void> {
    const { connectionId: cid, db: d } = connCtx(connectionId, db)
    return api.post(`${API}/connections/${cid}/keys/${encodeURIComponent(key)}/zset/delete`, { db: d, member })
  },

  // ---- Command execution ----
  async executeCommand(command: string, connectionId?: string, db?: number): Promise<string> {
    return tryApi(async () => {
      const { connectionId: cid, db: d } = connCtx(connectionId, db)
      const res = await api.post<{ result: string }>(`${API}/connections/${cid}/exec`, { db: d, command })
      return res.result
    }, mockExec(command))
  },

  // ---- Server info ----
  async getServerInfo(connectionId?: string): Promise<ServerInfo | null> {
    const { connectionId: cid } = connCtx(connectionId)
    const mock = mockServerInfo[cid] || null
    return tryApi(() => api.get<ServerInfo | null>(`${API}/connections/${cid}/info`), mock)
  },
  async getSlowLog(count = 10, connectionId?: string): Promise<SlowLogEntry[]> {
    const { connectionId: cid } = connCtx(connectionId)
    return tryApi(() => api.get<SlowLogEntry[]>(`${API}/connections/${cid}/slowlog`, { count: String(count) }), mockSlowLog.slice(0, count))
  },
  async getCommands(): Promise<CliCommand[]> {
    return tryApi(() => api.get<CliCommand[]>(`${API}/commands`), mockCliCommands)
  },
}

/** Simple mock command executor for development without backend */
function mockExec(command: string): string {
  const upper = command.trim().toUpperCase()
  const args = command.trim().split(/\s+/)
  const cmd = args[0]?.toUpperCase() || ''
  switch (cmd) {
    case 'PING': return 'PONG'
    case 'DBSIZE': return '(integer) 48'
    case 'INFO': return '# Server\nredis_version:7.2.4\n\n# Memory\nused_memory_human:2.45G\n\n# Stats\ninstantaneous_ops_per_sec:1523'
    case 'KEYS': return 'user:1001:profile\nuser:1002:profile\nproduct:5001:info\ncache:api:user_list:v1\nconfig:app:settings'
    case 'GET': return `"${args[1] || 'nil'}"`
    case 'SET': return 'OK'
    case 'TYPE': return 'hash'
    case 'TTL': return '(integer) 3600'
    case 'EXISTS': return '(integer) 1'
    case 'DEL': return '(integer) 1'
    case 'CLIENT': return 'id=3 addr=192.168.1.100:52341 name=app age=1234 cmd=ping'
    case 'SLOWLOG': return '1) 125000us KEYS user:* @ 192.168.1.100:52341'
    case 'HELP': return 'PING GET SET KEYS INFO DBSIZE TYPE TTL EXISTS DEL CLIENT SLOWLOG'
    default: return 'OK'
  }
}
