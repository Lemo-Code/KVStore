import { create } from 'zustand'
import { persist } from 'zustand/middleware'

export type NotifType = 'chat' | 'ai' | 'learn' | 'system'

export interface PlatformNotification {
  id: string
  type: NotifType
  title: string
  content: string
  time: string
  read: boolean
  link?: string
}

export interface ChatRoom {
  id: string
  name: string
  members: number
  unread: number
  topic: string
  messages: import('@/types').ChatMessage[]
}

export interface Exercise {
  id: string
  title: string
  description: string
  difficulty: 'easy' | 'medium' | 'hard'
  type: 'command' | 'design' | 'debug'
  tags: string[]
  completed: boolean
  hint?: string
  expectedCommand?: string
}

const defaultNotifications: PlatformNotification[] = [
  { id: '1', type: 'chat', title: '协作室新消息', content: '张明在 Sorted Set 专题讨论中 @了你', time: '5 分钟前', read: false, link: '/chat' },
  { id: '2', type: 'ai', title: 'AI 导师回复', content: '你关于排行榜设计的问题已收到详细解答', time: '30 分钟前', read: false, link: '/ai' },
  { id: '3', type: 'learn', title: '课程提醒', content: '今日学习目标：Sorted Set 排行榜实战', time: '1 小时前', read: false, link: '/learning/mod2' },
  { id: '4', type: 'chat', title: '新成员加入', content: '王浩 加入了 Sorted Set 专题讨论', time: '2 小时前', read: true, link: '/chat' },
  { id: '5', type: 'learn', title: '成就解锁', content: '恭喜获得「连续学习 7 天」徽章！', time: '昨天', read: true, link: '/profile' },
  { id: '6', type: 'system', title: '系统更新', content: 'Redis Lab Studio v1.1 已发布', time: '2 天前', read: true },
]

export const mockExercises: Exercise[] = [
  {
    id: 'ex1',
    title: 'String 基础：SET 与 GET',
    description: '使用 SET 命令存储用户名，再用 GET 读取',
    difficulty: 'easy',
    type: 'command',
    tags: ['String', 'SET', 'GET'],
    completed: true,
    expectedCommand: 'SET user:name "redis_learner"',
    hint: '格式: SET key value',
  },
  {
    id: 'ex2',
    title: 'Hash 用户信息',
    description: '用 HSET 为 user:1001 添加 email 和 level 字段',
    difficulty: 'easy',
    type: 'command',
    tags: ['Hash', 'HSET'],
    completed: true,
    expectedCommand: 'HSET user:1001 email learner@redis.lab',
    hint: 'HSET key field value [field value ...]',
  },
  {
    id: 'ex3',
    title: '排行榜 Top 5',
    description: '获取 leaderboard:game 中分数最高的 5 名玩家',
    difficulty: 'medium',
    type: 'command',
    tags: ['ZSet', 'ZREVRANGE'],
    completed: false,
    expectedCommand: 'ZREVRANGE leaderboard:game 0 4 WITHSCORES',
    hint: 'ZREVRANGE 按分数降序排列',
  },
  {
    id: 'ex4',
    title: '设计购物车',
    description: '为电商场景选择合适的 Redis 数据结构存储购物车',
    difficulty: 'medium',
    type: 'design',
    tags: ['List', 'Hash', '设计'],
    completed: false,
    hint: '考虑商品顺序、快速增删、用户隔离',
  },
  {
    id: 'ex5',
    title: '调试慢查询',
    description: '分析为什么 KEYS user:* 在生产环境不推荐',
    difficulty: 'hard',
    type: 'debug',
    tags: ['SCAN', '性能'],
    completed: false,
    hint: '思考时间复杂度和阻塞问题',
  },
  {
    id: 'ex6',
    title: 'Set 标签去重',
    description: '向 tags:article:99 添加新标签并检查是否存在',
    difficulty: 'easy',
    type: 'command',
    tags: ['Set', 'SADD', 'SISMEMBER'],
    completed: false,
    expectedCommand: 'SADD tags:article:99 redis',
  },
]

export const mockChatRooms: ChatRoom[] = [
  {
    id: 'room-1',
    name: 'Sorted Set 专题讨论',
    members: 12,
    unread: 3,
    topic: 'ZADD / ZREVRANGE',
    messages: [
      { id: 'm1', userId: 'u1', content: '大家好，今天我们学习 Redis 的 Sorted Set 数据结构', timestamp: '10:32', type: 'text' },
      { id: 'm2', userId: 'u2', content: '老师，ZADD 和 ZINCRBY 有什么区别？', timestamp: '10:33', type: 'text' },
      { id: 'm3', userId: 'u1', content: 'ZADD 用于添加或更新成员分数，ZINCRBY 用于增量修改', timestamp: '10:34', type: 'text' },
      { id: 'm4', userId: 'u1', content: 'ZADD leaderboard:game 100 "player1"\nZINCRBY leaderboard:game 50 "player1"', timestamp: '10:35', type: 'code' },
    ],
  },
  {
    id: 'room-2',
    name: 'Redis 基础入门班',
    members: 28,
    unread: 0,
    topic: 'String & TTL',
    messages: [
      { id: 'r2m1', userId: 'u1', content: '欢迎新同学！今天从 String 类型开始', timestamp: '09:00', type: 'text' },
    ],
  },
  {
    id: 'room-3',
    name: '生产实践交流群',
    members: 45,
    unread: 1,
    topic: '缓存 & 集群',
    messages: [
      { id: 'r3m1', userId: 'u2', content: '大家线上 Redis 用什么持久化策略？', timestamp: '昨天', type: 'text' },
    ],
  },
  {
    id: 'room-4',
    name: '每日一题挑战',
    members: 67,
    unread: 0,
    topic: '今日: 实现分布式锁',
    messages: [],
  },
  {
    id: 'room-5',
    name: '导师答疑室',
    members: 8,
    unread: 0,
    topic: '开放答疑',
    messages: [],
  },
]

interface PlatformState {
  notifications: PlatformNotification[]
  chatRooms: ChatRoom[]
  activeChatRoom: string
  exercises: Exercise[]
  onboardingPrefs: { goal: string[]; interest: string[]; env: string[] }
  commandPaletteOpen: boolean
  unreadNotifications: () => number
  unreadChatTotal: () => number
  markNotificationRead: (id: string) => void
  markAllNotificationsRead: () => void
  setActiveChatRoom: (id: string) => void
  addChatRoomMessage: (roomId: string, msg: import('@/types').ChatMessage) => void
  clearRoomUnread: (roomId: string) => void
  completeExercise: (id: string) => void
  setOnboardingPrefs: (prefs: PlatformState['onboardingPrefs']) => void
  setCommandPaletteOpen: (open: boolean) => void
}

export const usePlatformStore = create<PlatformState>()(
  persist(
    (set, get) => ({
      notifications: defaultNotifications,
      chatRooms: mockChatRooms,
      activeChatRoom: 'room-1',
      exercises: mockExercises,
      onboardingPrefs: { goal: [], interest: [], env: [] },
      commandPaletteOpen: false,

      unreadNotifications: () => get().notifications.filter((n) => !n.read).length,
      unreadChatTotal: () => get().chatRooms.reduce((a, r) => a + r.unread, 0),

      markNotificationRead: (id) =>
        set((s) => ({
          notifications: s.notifications.map((n) => (n.id === id ? { ...n, read: true } : n)),
        })),

      markAllNotificationsRead: () =>
        set((s) => ({
          notifications: s.notifications.map((n) => ({ ...n, read: true })),
        })),

      setActiveChatRoom: (id) =>
        set((s) => ({
          activeChatRoom: id,
          chatRooms: s.chatRooms.map((r) => (r.id === id ? { ...r, unread: 0 } : r)),
        })),

      addChatRoomMessage: (roomId, msg) =>
        set((s) => ({
          chatRooms: s.chatRooms.map((r) =>
            r.id === roomId ? { ...r, messages: [...r.messages, msg] } : r,
          ),
        })),

      clearRoomUnread: (roomId) =>
        set((s) => ({
          chatRooms: s.chatRooms.map((r) => (r.id === roomId ? { ...r, unread: 0 } : r)),
        })),

      completeExercise: (id) =>
        set((s) => ({
          exercises: s.exercises.map((e) => (e.id === id ? { ...e, completed: true } : e)),
        })),

      setOnboardingPrefs: (prefs) => set({ onboardingPrefs: prefs }),
      setCommandPaletteOpen: (open) => set({ commandPaletteOpen: open }),
    }),
    {
      name: 'redis-lab-platform',
      partialize: (s) => ({
        notifications: s.notifications,
        exercises: s.exercises,
        onboardingPrefs: s.onboardingPrefs,
        activeChatRoom: s.activeChatRoom,
      }),
    },
  ),
)
