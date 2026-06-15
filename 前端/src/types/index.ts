export type RedisKeyType = 'string' | 'hash' | 'list' | 'set' | 'zset' | 'stream'

export interface RedisConnection {
  id: string
  name: string
  host: string
  port: number
  status: 'connected' | 'disconnected' | 'connecting'
  version: string
  memory: string
  keys: number
  uptime: string
}

export interface RedisDatabase {
  id: number
  name: string
  keyCount: number
  expanded?: boolean
}

export interface RedisKey {
  name: string
  type: RedisKeyType
  ttl: number
  size: number
  encoding?: string
}

export interface HashField {
  field: string
  value: string
}

export interface ZSetMember {
  member: string
  score: number
}

export interface StreamEntry {
  id: string
  field: string
  value: string
}

export interface ChatUser {
  id: string
  name: string
  avatar: string
  status: 'online' | 'away' | 'offline'
  role: 'student' | 'mentor' | 'admin'
}

export interface ChatRoom {
  id: string
  name: string
  topic: string
  members: number
  unread: number
}

export interface ChatMessage {
  id: string
  userId: string
  content: string
  timestamp: string
  type: 'text' | 'code' | 'system'
  replyTo?: string
}

export interface AIMessage {
  id: string
  role: 'user' | 'assistant' | 'system'
  content: string
  timestamp: string
  suggestions?: string[]
  codeBlock?: string
}

export interface LearningModule {
  id: string
  title: string
  description: string
  progress: number
  lessons: number
  completedLessons: number
  difficulty: 'beginner' | 'intermediate' | 'advanced'
  tags: string[]
}

export interface QueryHistoryItem {
  id: string
  command: string
  result: string
  duration: number
  timestamp: string
  success: boolean
}

export interface TabItem {
  id: string
  title: string
  type: 'console' | 'key' | 'query' | 'learning'
  keyName?: string
  keyType?: RedisKeyType
  dirty?: boolean
}

export interface Exercise {
  id: string
  title: string
  description: string
  difficulty: 'easy' | 'medium' | 'hard'
  type: RedisKeyType | 'general'
  prompt: string
  hint: string
  expectedCommand: string
  tags: string[]
  completed?: boolean
}

export interface PlatformNotification {
  id: string
  type: 'chat' | 'ai' | 'learn' | 'system'
  title: string
  content: string
  time: string
  read: boolean
  link?: string
}

export interface UserPreferences {
  goal?: string
  interests: string[]
  env?: string
}

export interface AdminUser {
  id: string
  username: string
  email: string
  role: 'student' | 'mentor' | 'admin'
  status: 'active' | 'banned'
  joinDate: string
  lastActive: string
}
