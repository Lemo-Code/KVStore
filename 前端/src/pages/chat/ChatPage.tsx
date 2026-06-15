import { ChatPanel } from '@/components/chat/ChatPanel'
import { PageHeader } from '@/components/layout/MainLayout'
import { cn } from '@/lib/utils'
import { mockChatUsers } from '@/stores/appStore'
import { Hash, Plus, Search, Users } from 'lucide-react'
import { useState } from 'react'

const chatRooms = [
  { id: 'room-1', name: 'Sorted Set 专题讨论', members: 12, unread: 3, active: true, topic: 'ZADD / ZREVRANGE' },
  { id: 'room-2', name: 'Redis 基础入门班', members: 28, unread: 0, active: false, topic: 'String & TTL' },
  { id: 'room-3', name: '生产实践交流群', members: 45, unread: 1, active: false, topic: '缓存 & 集群' },
  { id: 'room-4', name: '每日一题挑战', members: 67, unread: 0, active: false, topic: '今日: 实现分布式锁' },
  { id: 'room-5', name: '导师答疑室', members: 8, unread: 0, active: false, topic: '开放答疑' },
]

export default function ChatPage() {
  const [activeRoom, setActiveRoom] = useState('room-1')
  const [search, setSearch] = useState('')

  const filtered = chatRooms.filter((r) => r.name.includes(search))

  return (
    <div className="flex h-full flex-col">
      <PageHeader title="协作通讯" subtitle="与学习伙伴和导师实时交流，共享代码与经验" />
      <div className="flex flex-1 min-h-0">
        {/* Room list */}
        <div className="w-72 shrink-0 border-r border-border-subtle bg-surface-1 flex flex-col">
          <div className="p-3 border-b border-border-subtle">
            <div className="relative">
              <Search size={13} className="absolute left-2.5 top-1/2 -translate-y-1/2 text-text-muted" />
              <input
                value={search}
                onChange={(e) => setSearch(e.target.value)}
                placeholder="搜索房间..."
                className="w-full rounded-lg border border-border-subtle bg-surface-0 py-1.5 pl-8 pr-3 text-xs focus:outline-none focus:border-accent-teal/40 focus:ring-1 focus:ring-accent-teal/20"
              />
            </div>
          </div>

          <div className="px-3 py-2 flex items-center justify-between">
            <span className="text-[10px] font-semibold text-text-muted uppercase tracking-wider">学习房间</span>
            <button className="p-1 rounded text-text-muted hover:text-accent-teal transition-colors">
              <Plus size={14} />
            </button>
          </div>

          <div className="flex-1 overflow-y-auto px-2 pb-2 space-y-0.5">
            {filtered.map((room) => (
              <button
                key={room.id}
                onClick={() => setActiveRoom(room.id)}
                className={cn(
                  'w-full flex items-start gap-2.5 rounded-lg px-2.5 py-2.5 text-left transition-all',
                  activeRoom === room.id
                    ? 'bg-accent-teal/10 border border-accent-teal/20'
                    : 'hover:bg-surface-2 border border-transparent',
                )}
              >
                <div className="flex h-9 w-9 shrink-0 items-center justify-center rounded-lg bg-surface-3">
                  <Hash size={14} className="text-accent-teal" />
                </div>
                <div className="flex-1 min-w-0">
                  <div className="flex items-center gap-1.5">
                    <span className="text-xs font-semibold truncate">{room.name}</span>
                    {room.unread > 0 && (
                      <span className="flex h-4 min-w-4 items-center justify-center rounded-full bg-accent-red px-1 text-[9px] font-bold text-white">
                        {room.unread}
                      </span>
                    )}
                  </div>
                  <div className="text-[10px] text-text-muted truncate mt-0.5">{room.topic}</div>
                  <div className="flex items-center gap-1 mt-1 text-[9px] text-text-muted">
                    <Users size={9} />
                    {room.members} 人
                  </div>
                </div>
              </button>
            ))}
          </div>

          {/* Online sidebar */}
          <div className="border-t border-border-subtle p-3">
            <div className="text-[10px] font-semibold text-text-muted uppercase tracking-wider mb-2">
              在线成员 ({mockChatUsers.filter((u) => u.status === 'online').length})
            </div>
            <div className="flex flex-wrap gap-1.5">
              {mockChatUsers.map((user) => (
                <div
                  key={user.id}
                  title={user.name}
                  className={cn(
                    'flex h-7 w-7 items-center justify-center rounded-full text-[9px] font-bold relative',
                    user.role === 'mentor' ? 'bg-accent-amber/20 text-accent-amber' : 'bg-surface-4 text-text-secondary',
                  )}
                >
                  {user.avatar}
                  <span
                    className={cn(
                      'absolute -bottom-0.5 -right-0.5 h-2 w-2 rounded-full border border-surface-1',
                      user.status === 'online' ? 'bg-success' : user.status === 'away' ? 'bg-warning' : 'bg-text-muted',
                    )}
                  />
                </div>
              ))}
            </div>
          </div>
        </div>

        {/* Chat area */}
        <div className="flex-1 min-w-0">
          <ChatPanel />
        </div>
      </div>
    </div>
  )
}
