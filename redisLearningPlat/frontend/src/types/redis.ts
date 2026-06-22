export type RedisDataType = 'string' | 'hash' | 'list' | 'set' | 'zset' | 'stream' | 'json' | 'none'

export interface RedisConnection {
  id: string
  name: string
  host: string
  port: number
  password?: string
  db: number
  status: 'connected' | 'disconnected' | 'connecting'
  ssl: boolean
  group?: string
  createdAt: string
  lastConnectedAt?: string
}

export interface RedisKey {
  name: string
  type: RedisDataType
  ttl: number // -1 = no expiry, -2 = expired
  size: number // bytes
  db: number
}

export interface StringValue {
  key: string
  type: 'string'
  value: string
  ttl: number
  encoding: 'raw' | 'int' | 'embstr'
}

export interface HashField {
  field: string
  value: string
}

export interface HashValue {
  key: string
  type: 'hash'
  fields: HashField[]
  ttl: number
  length: number
}

export interface ListValue {
  key: string
  type: 'list'
  values: { index: number; value: string }[]
  ttl: number
  length: number
}

export interface SetValue {
  key: string
  type: 'set'
  members: string[]
  ttl: number
  length: number
}

export interface ZSetMember {
  member: string
  score: number
}

export interface ZSetValue {
  key: string
  type: 'zset'
  members: ZSetMember[]
  ttl: number
  length: number
}

export interface StreamMessage {
  id: string
  fields: Record<string, string>
}

export interface StreamValue {
  key: string
  type: 'stream'
  messages: StreamMessage[]
  consumerGroups: { name: string; consumers: number; pending: number }[]
  ttl: number
  length: number
}

export type RedisValue =
  | StringValue
  | HashValue
  | ListValue
  | SetValue
  | ZSetValue
  | StreamValue

export interface ServerInfo {
  server: Record<string, string>
  clients: Record<string, string>
  memory: Record<string, string>
  stats: Record<string, string>
  replication: Record<string, string>
  cpu: Record<string, string>
  cluster: Record<string, string>
  keyspace: Record<string, string>
}

export interface SlowLogEntry {
  id: number
  timestamp: string
  duration: number // microseconds
  command: string
  clientIp: string
}

export interface PubSubChannel {
  name: string
  subscribers: number
  pattern?: string
}

export interface PubSubMessage {
  channel: string
  pattern?: string
  message: string
  timestamp: string
}

export interface CliCommand {
  command: string
  description: string
  group: string
  syntax?: string
}
