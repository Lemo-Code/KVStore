import { create } from 'zustand'
import type {
  AIMessage,
  ChatMessage,
  ChatUser,
  LearningModule,
  QueryHistoryItem,
  RedisConnection,
  RedisDatabase,
  RedisKey,
  TabItem,
} from '@/types'

export const mockConnection: RedisConnection = {
  id: 'conn-1',
  name: '本地 Redis 学习环境',
  host: '127.0.0.1',
  port: 6379,
  status: 'connected',
  version: '7.2.4',
  memory: '12.4 MB',
  keys: 847,
  uptime: '3天 14小时',
}

export const mockDatabases: RedisDatabase[] = [
  { id: 0, name: 'db0', keyCount: 312, expanded: true },
  { id: 1, name: 'db1', keyCount: 89 },
  { id: 2, name: 'db2', keyCount: 156 },
  { id: 3, name: 'db3', keyCount: 45 },
]

export const mockKeys: RedisKey[] = [
  { name: 'user:1001:profile', type: 'hash', ttl: -1, size: 256, encoding: 'hashtable' },
  { name: 'session:abc123', type: 'string', ttl: 3600, size: 128 },
  { name: 'leaderboard:game', type: 'zset', ttl: -1, size: 2048 },
  { name: 'cart:user:42', type: 'list', ttl: 7200, size: 512 },
  { name: 'tags:article:99', type: 'set', ttl: -1, size: 96 },
  { name: 'events:stream', type: 'stream', ttl: -1, size: 4096 },
  { name: 'cache:homepage', type: 'string', ttl: 300, size: 8192 },
  { name: 'config:app', type: 'hash', ttl: -1, size: 384 },
  { name: 'queue:jobs', type: 'list', ttl: -1, size: 1536 },
  { name: 'online:users', type: 'set', ttl: 60, size: 64 },
]

export const mockHashData = [
  { field: 'id', value: '1001' },
  { field: 'username', value: 'redis_learner' },
  { field: 'email', value: 'learner@redis.lab' },
  { field: 'level', value: 'intermediate' },
  { field: 'score', value: '2840' },
  { field: 'created_at', value: '2025-03-15T08:30:00Z' },
]

export const mockZSetData = [
  { member: 'player_alpha', score: 9850 },
  { member: 'player_beta', score: 8720 },
  { member: 'player_gamma', score: 7650 },
  { member: 'player_delta', score: 6430 },
  { member: 'player_epsilon', score: 5210 },
]

export const mockListData = ['item:sword+3', 'item:shield', 'item:potion_x5', 'item:key_gold']

export const mockSetData = ['redis', 'database', 'cache', 'nosql', 'in-memory']

export const mockChatUsers: ChatUser[] = [
  { id: 'u1', name: '张明', avatar: 'ZM', status: 'online', role: 'mentor' },
  { id: 'u2', name: '李雪', avatar: 'LX', status: 'online', role: 'student' },
  { id: 'u3', name: '王浩', avatar: 'WH', status: 'away', role: 'student' },
  { id: 'u4', name: '陈悦', avatar: 'CY', status: 'offline', role: 'student' },
]

export const mockChatMessages: ChatMessage[] = [
  {
    id: 'm1',
    userId: 'u1',
    content: '大家好，今天我们学习 Redis 的 Sorted Set 数据结构',
    timestamp: '10:32',
    type: 'text',
  },
  {
    id: 'm2',
    userId: 'u2',
    content: '老师，ZADD 和 ZINCRBY 有什么区别？',
    timestamp: '10:33',
    type: 'text',
  },
  {
    id: 'm3',
    userId: 'u1',
    content: 'ZADD 用于添加或更新成员分数，ZINCRBY 用于增量修改已有成员的分数',
    timestamp: '10:34',
    type: 'text',
  },
  {
    id: 'm4',
    userId: 'u1',
    content: 'ZADD leaderboard:game 100 "player1"\nZINCRBY leaderboard:game 50 "player1"',
    timestamp: '10:35',
    type: 'code',
  },
  {
    id: 'm5',
    userId: 'system',
    content: '王浩 加入了学习房间',
    timestamp: '10:36',
    type: 'system',
  },
]

export const mockAIMessages: AIMessage[] = [
  {
    id: 'ai1',
    role: 'assistant',
    content:
      '你好！我是 Redis Lab AI 导师。我可以帮你理解 Redis 命令、分析数据结构选择、调试命令问题，以及制定个性化学习路径。',
    timestamp: '10:30',
    suggestions: ['解释 Hash vs String', 'TTL 最佳实践', '排行榜怎么设计'],
  },
  {
    id: 'ai2',
    role: 'user',
    content: '如何用 Redis 实现一个实时排行榜？',
    timestamp: '10:31',
  },
  {
    id: 'ai3',
    role: 'assistant',
    content:
      '实时排行榜最适合使用 **Sorted Set (ZSET)**。核心思路：\n\n1. 用 `ZADD` 添加/更新玩家分数\n2. 用 `ZREVRANGE` 获取 Top N\n3. 用 `ZRANK` 查询某玩家排名\n4. 用 `ZINCRBY` 实现分数增量更新\n\n时间复杂度均为 O(log N)，非常适合高频更新场景。',
    timestamp: '10:31',
    codeBlock: `# 添加玩家分数
ZADD leaderboard:game 9850 "player_alpha"
ZADD leaderboard:game 8720 "player_beta"

# 获取 Top 10
ZREVRANGE leaderboard:game 0 9 WITHSCORES

# 查询玩家排名 (0-based)
ZREVRANK leaderboard:game "player_alpha"

# 增量更新分数
ZINCRBY leaderboard:game 100 "player_alpha"`,
    suggestions: ['看看我的 leaderboard:game 键', '如何设置过期时间', '排行榜分片方案'],
  },
]

export const mockLearningModules: LearningModule[] = [
  {
    id: 'mod1',
    title: 'Redis 基础入门',
    description: 'String、Key 管理、过期策略',
    progress: 100,
    lessons: 8,
    completedLessons: 8,
    difficulty: 'beginner',
    tags: ['String', 'TTL', 'DEL'],
  },
  {
    id: 'mod2',
    title: '数据结构深度解析',
    description: 'Hash、List、Set、ZSet 实战',
    progress: 65,
    lessons: 12,
    completedLessons: 8,
    difficulty: 'intermediate',
    tags: ['Hash', 'List', 'Set', 'ZSet'],
  },
  {
    id: 'mod3',
    title: '高级特性与生产实践',
    description: 'Stream、Pub/Sub、Pipeline、事务',
    progress: 20,
    lessons: 10,
    completedLessons: 2,
    difficulty: 'advanced',
    tags: ['Stream', 'Pub/Sub', 'Pipeline'],
  },
  {
    id: 'mod4',
    title: '性能优化与内存管理',
    description: '内存分析、慢查询、集群架构',
    progress: 0,
    lessons: 8,
    completedLessons: 0,
    difficulty: 'advanced',
    tags: ['Memory', 'Slowlog', 'Cluster'],
  },
]

export const mockQueryHistory: QueryHistoryItem[] = [
  {
    id: 'q1',
    command: 'KEYS user:*',
    result: '3 keys found',
    duration: 2.1,
    timestamp: '10:28:15',
    success: true,
  },
  {
    id: 'q2',
    command: 'HGETALL user:1001:profile',
    result: '6 fields',
    duration: 0.8,
    timestamp: '10:28:42',
    success: true,
  },
  {
    id: 'q3',
    command: 'ZREVRANGE leaderboard:game 0 4 WITHSCORES',
    result: '5 members',
    duration: 1.2,
    timestamp: '10:29:05',
    success: true,
  },
  {
    id: 'q4',
    command: 'FLUSHDB',
    result: 'ERR operation not permitted',
    duration: 0.3,
    timestamp: '10:29:30',
    success: false,
  },
]

interface AppState {
  activeDb: number
  selectedKey: RedisKey | null
  searchQuery: string
  activeTab: string
  tabs: TabItem[]
  rightPanel: 'ai' | 'chat' | 'learning'
  bottomPanelOpen: boolean
  aiMessages: AIMessage[]
  chatMessages: ChatMessage[]
  queryHistory: QueryHistoryItem[]
  setActiveDb: (db: number) => void
  setSelectedKey: (key: RedisKey | null) => void
  setSearchQuery: (q: string) => void
  setActiveTab: (id: string) => void
  setRightPanel: (panel: 'ai' | 'chat' | 'learning') => void
  toggleBottomPanel: () => void
  addAIMessage: (msg: AIMessage) => void
  addChatMessage: (msg: ChatMessage) => void
  openKeyTab: (key: RedisKey) => void
  closeTab: (id: string) => void
  addConsoleTab: () => void
  addQueryHistory: (item: QueryHistoryItem) => void
}

export const useAppStore = create<AppState>((set, get) => ({
  activeDb: 0,
  selectedKey: mockKeys[0],
  searchQuery: '',
  activeTab: 'tab-console',
  tabs: [
    { id: 'tab-console', title: '命令控制台', type: 'console' },
    { id: 'tab-key-1', title: 'user:1001:profile', type: 'key', keyName: 'user:1001:profile', keyType: 'hash' },
  ],
  rightPanel: 'ai',
  bottomPanelOpen: true,
  aiMessages: mockAIMessages,
  chatMessages: mockChatMessages,
  queryHistory: mockQueryHistory,
  setActiveDb: (db) => set({ activeDb: db }),
  setSelectedKey: (key) => {
    set({ selectedKey: key })
    if (key) get().openKeyTab(key)
  },
  setSearchQuery: (q) => set({ searchQuery: q }),
  setActiveTab: (id) => set({ activeTab: id }),
  setRightPanel: (panel) => set({ rightPanel: panel }),
  toggleBottomPanel: () => set((s) => ({ bottomPanelOpen: !s.bottomPanelOpen })),
  addAIMessage: (msg) => set((s) => ({ aiMessages: [...s.aiMessages, msg] })),
  addChatMessage: (msg) => set((s) => ({ chatMessages: [...s.chatMessages, msg] })),
  openKeyTab: (key) =>
    set((s) => {
      const existing = s.tabs.find((t) => t.keyName === key.name)
      if (existing) return { activeTab: existing.id, selectedKey: key }
      const id = `tab-key-${Date.now()}`
      return {
        tabs: [...s.tabs, { id, title: key.name, type: 'key', keyName: key.name, keyType: key.type }],
        activeTab: id,
        selectedKey: key,
      }
    }),
  closeTab: (id) =>
    set((s) => {
      const next = s.tabs.filter((t) => t.id !== id)
      if (next.length === 0) {
        const consoleTab = { id: 'tab-console', title: '命令控制台', type: 'console' as const }
        return { tabs: [consoleTab], activeTab: consoleTab.id }
      }
      const activeTab = s.activeTab === id ? next[next.length - 1].id : s.activeTab
      return { tabs: next, activeTab }
    }),
  addConsoleTab: () =>
    set((s) => {
      const id = `tab-console-${Date.now()}`
      return {
        tabs: [...s.tabs, { id, title: '新查询', type: 'console' }],
        activeTab: id,
      }
    }),
  addQueryHistory: (item) =>
    set((s) => ({ queryHistory: [item, ...s.queryHistory].slice(0, 20) })),
}))
