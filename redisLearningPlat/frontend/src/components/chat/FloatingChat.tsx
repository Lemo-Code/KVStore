import { useState, useRef, useEffect } from 'react'
import { MessageSquare, X, Send, Minimize2, Maximize2, Users, Hash, Circle } from 'lucide-react'
import { Button } from '@/components/ui/button'
import { ScrollArea } from '@/components/ui/scroll-area'
import { Badge } from '@/components/ui/badge'
import { cn } from '@/lib/utils'

interface Room { id: string; name: string; unread: number; online: number; lastMsg: string }
interface Message { id: string; userId: string; userName: string; content: string; time: string }

const ROOMS: Room[] = [
  { id: '1', name: 'Redis 新手互助', unread: 3, online: 12, lastMsg: '有人知道 RDB 和 AOF 的区别吗？' },
  { id: '2', name: '缓存策略讨论组', unread: 0, online: 8, lastMsg: '推荐看下缓存穿透的解决方案' },
  { id: '3', name: '集群运维交流', unread: 5, online: 15, lastMsg: 'Cluster 扩容有没有最佳实践？' },
  { id: '4', name: 'Redis Stack 新特性', unread: 0, online: 6, lastMsg: 'JSON 数据类型真方便' },
  { id: '5', name: '性能优化分享', unread: 2, online: 9, lastMsg: '分享一个 Pipeline 批量写入的案例' },
]

const MOCK_MSGS: Record<string, Message[]> = {
  '1': [
    { id: 'm1', userId: 'alice', userName: 'Alice', content: '大家早！最近在学习 Redis Stream', time: '10:28' },
    { id: 'm2', userId: 'bob', userName: 'Bob', content: 'Stream 做消息队列很好用', time: '10:30' },
    { id: 'm3', userId: 'alice', userName: 'Alice', content: '和 Kafka 比有什么优势？', time: '10:31' },
    { id: 'm4', userId: 'charlie', userName: 'Charlie', content: '更轻量，适合中小规模场景', time: '10:33' },
    { id: 'm5', userId: 'bob', userName: 'Bob', content: '而且部署简单，不需要额外组件', time: '10:35' },
    { id: 'm6', userId: 'alice', userName: 'Alice', content: '明白了，感谢！🙏', time: '10:36' },
  ],
  '3': [
    { id: 'm7', userId: 'charlie', userName: 'Charlie', content: 'Cluster 扩容节点有什么注意事项？', time: '09:15' },
    { id: 'm8', userId: 'dave', userName: 'Dave', content: '注意槽位迁移时的性能影响', time: '09:18' },
    { id: 'm9', userId: 'charlie', userName: 'Charlie', content: '用 redis-cli --cluster rebalance 吗？', time: '09:20' },
  ],
}

export default function FloatingChat({ onClose }: { onClose: () => void }) {
  const [minimized, setMinimized] = useState(false)
  const [activeRoom, setActiveRoom] = useState<string | null>(null)
  const [messages, setMessages] = useState<Message[]>([])
  const [input, setInput] = useState('')
  const [showRooms, setShowRooms] = useState(true)
  const scrollRef = useRef<HTMLDivElement>(null)

  useEffect(() => { scrollRef.current?.scrollTo({ top: scrollRef.current.scrollHeight, behavior: 'smooth' }) }, [messages])

  const selectRoom = (roomId: string) => {
    setActiveRoom(roomId)
    setMessages(MOCK_MSGS[roomId] || [])
    setShowRooms(false)
  }

  const send = () => {
    if (!input.trim()) return
    const now = new Date()
    setMessages((p) => [...p, {
      id: Date.now().toString(), userId: 'me', userName: '我',
      content: input,
      time: `${now.getHours().toString().padStart(2, '0')}:${now.getMinutes().toString().padStart(2, '0')}`,
    }])
    setInput('')
  }

  const room = ROOMS.find((r) => r.id === activeRoom)

  return (
    <div className={cn(
      'fixed z-50 flex flex-col overflow-hidden rounded-2xl border bg-card shadow-2xl transition-all',
      minimized ? 'bottom-6 right-6 h-12 w-64' : 'bottom-6 right-20 h-[520px] w-[480px]',
    )}>
      {/* Header */}
      <div className="flex items-center gap-2 border-b bg-gradient-to-r from-emerald-500 to-teal-500 px-4 py-2.5 text-white shrink-0">
        <MessageSquare className="h-4 w-4" />
        {room ? (
          <span className="flex-1 text-sm font-semibold truncate">{room.name}</span>
        ) : (
          <span className="flex-1 text-sm font-semibold">学习交流</span>
        )}
        <Button variant="ghost" size="icon-sm" className="text-white hover:bg-white/10 h-7 w-7"
          onClick={() => setShowRooms(!showRooms)} title="房间列表">
          <Hash className="h-3.5 w-3.5" />
        </Button>
        <Button variant="ghost" size="icon-sm" className="text-white hover:bg-white/10 h-7 w-7"
          onClick={() => setMinimized(!minimized)}>
          {minimized ? <Maximize2 className="h-3.5 w-3.5" /> : <Minimize2 className="h-3.5 w-3.5" />}
        </Button>
        <Button variant="ghost" size="icon-sm" className="text-white hover:bg-white/10 h-7 w-7"
          onClick={() => { onClose(); setActiveRoom(null) }}>
          <X className="h-3.5 w-3.5" />
        </Button>
      </div>

      {!minimized && (
        <>
          {showRooms && !activeRoom ? (
            /* Room list */
            <ScrollArea className="flex-1">
              {ROOMS.map((r) => (
                <button key={r.id} onClick={() => selectRoom(r.id)}
                  className="flex w-full items-start gap-3 border-b px-4 py-3 text-left transition-colors hover:bg-muted/50">
                  <div className="mt-0.5 flex h-9 w-9 shrink-0 items-center justify-center rounded-full bg-gradient-to-br from-emerald-400 to-teal-500 text-xs font-semibold text-white">
                    {r.name[0]}
                  </div>
                  <div className="min-w-0 flex-1">
                    <div className="flex items-center justify-between">
                      <span className="text-sm font-semibold">{r.name}</span>
                      {r.unread > 0 && (
                        <Badge variant="default" className="h-4 min-w-4 px-1 text-[10px]">{r.unread}</Badge>
                      )}
                    </div>
                    <p className="mt-0.5 truncate text-xs text-muted-foreground">{r.lastMsg}</p>
                    <div className="mt-1 flex items-center gap-1 text-[10px] text-muted-foreground">
                      <Circle className="h-1.5 w-1.5 fill-emerald-500 text-emerald-500" />
                      {r.online} 人在线
                    </div>
                  </div>
                </button>
              ))}
            </ScrollArea>
          ) : (
            /* Chat area */
            <>
              {/* Room info bar */}
              {room && (
                <div className="flex items-center gap-2 border-b px-4 py-1.5 text-xs text-muted-foreground shrink-0">
                  <button onClick={() => { setShowRooms(true); setActiveRoom(null) }}
                    className="hover:text-foreground">← 房间列表</button>
                  <span className="flex-1" />
                  <Users className="h-3 w-3" />
                  <span>{room.online} 在线</span>
                </div>
              )}

              {/* Messages */}
              <ScrollArea className="flex-1" ref={scrollRef}>
                <div className="space-y-1 p-3">
                  {messages.map((m, i) => {
                    const me = m.userId === 'me'
                    const show = i === 0 || messages[i - 1]?.userId !== m.userId
                    return (
                      <div key={m.id} className={cn('flex gap-2', me && 'flex-row-reverse', show ? 'mt-3' : 'mt-0.5')}>
                        {show ? (
                          <div className={cn('flex h-7 w-7 shrink-0 items-center justify-center rounded-full text-[10px] font-semibold text-white', me ? 'bg-blue-500' : 'bg-gradient-to-br from-gray-400 to-gray-500')}>
                            {m.userName[0]}
                          </div>
                        ) : <div className="w-7 shrink-0" />}
                        <div className={cn('max-w-[70%]', me && 'items-end')}>
                          {show && <p className="mb-0.5 text-[11px] font-medium">{m.userName} <span className="font-normal text-muted-foreground ml-1">{m.time}</span></p>}
                          <div className={cn('rounded-xl px-3 py-2 text-[13px] leading-relaxed', me ? 'bg-primary text-primary-foreground rounded-br-md' : 'bg-muted rounded-bl-md')}>
                            {m.content}
                          </div>
                        </div>
                      </div>
                    )
                  })}
                </div>
              </ScrollArea>

              {/* Input */}
              <div className="border-t p-3 shrink-0">
                <div className="flex gap-2">
                  <input value={input} onChange={(e) => setInput(e.target.value)}
                    onKeyDown={(e) => e.key === 'Enter' && !e.shiftKey && (e.preventDefault(), send())}
                    className="flex-1 rounded-lg border bg-background px-3 py-2 text-[13px] outline-none focus:ring-2 focus:ring-emerald-500/20"
                    placeholder={`发送消息到 ${room?.name || '...'}...`} />
                  <Button size="icon-sm" className="h-9 w-9 bg-emerald-500 hover:bg-emerald-600"
                    onClick={send} disabled={!input.trim()}>
                    <Send className="h-3.5 w-3.5" />
                  </Button>
                </div>
              </div>
            </>
          )}
        </>
      )}
    </div>
  )
}
