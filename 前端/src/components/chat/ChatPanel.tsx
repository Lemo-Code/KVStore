import { Button } from '@/components/ui/Button'
import { useAppStore } from '@/stores/appStore'
import { mockChatUsers } from '@/stores/appStore'
import { cn } from '@/lib/utils'
import { AnimatePresence, motion } from 'framer-motion'
import {
  Hash,
  Paperclip,
  Send,
  Smile,
  Users,
  Volume2,
} from 'lucide-react'
import { useState } from 'react'

export function ChatPanel() {
  const { chatMessages, addChatMessage } = useAppStore()
  const [input, setInput] = useState('')
  const onlineCount = mockChatUsers.filter((u) => u.status === 'online').length

  const handleSend = () => {
    if (!input.trim()) return
    addChatMessage({
      id: `m-${Date.now()}`,
      userId: 'me',
      content: input,
      timestamp: new Date().toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit' }),
      type: 'text',
    })
    setInput('')
  }

  const getUser = (userId: string) => {
    if (userId === 'me') return { name: '我', avatar: 'ME', role: 'student' as const }
    if (userId === 'system') return { name: '系统', avatar: 'SYS', role: 'admin' as const }
    return mockChatUsers.find((u) => u.id === userId)
  }

  return (
    <div className="flex h-full flex-col">
      {/* Header */}
      <div className="flex items-center justify-between border-b border-border-subtle px-4 py-3">
        <div>
          <div className="flex items-center gap-2">
            <Users size={14} className="text-accent-teal" />
            <span className="text-sm font-semibold">Redis 学习协作室</span>
          </div>
          <span className="text-[10px] text-text-muted">
            {onlineCount} 人在线 · Sorted Set 专题讨论
          </span>
        </div>
        <Button variant="ghost" size="icon" className="h-7 w-7">
          <Volume2 size={14} />
        </Button>
      </div>

      {/* Online users */}
      <div className="flex items-center gap-2 border-b border-border-subtle px-4 py-2 overflow-x-auto">
        {mockChatUsers.map((user) => (
          <div key={user.id} className="flex items-center gap-1.5 shrink-0">
            <div className="relative">
              <div
                className={cn(
                  'flex h-6 w-6 items-center justify-center rounded-full text-[9px] font-bold',
                  user.role === 'mentor'
                    ? 'bg-accent-amber/20 text-accent-amber'
                    : 'bg-surface-4 text-text-secondary',
                )}
              >
                {user.avatar}
              </div>
              <span
                className={cn(
                  'absolute -bottom-0.5 -right-0.5 h-2 w-2 rounded-full border border-surface-1',
                  user.status === 'online' && 'bg-success',
                  user.status === 'away' && 'bg-warning',
                  user.status === 'offline' && 'bg-text-muted',
                )}
              />
            </div>
            <span className="text-[10px] text-text-secondary">{user.name}</span>
            {user.role === 'mentor' && (
              <span className="text-[8px] text-accent-amber font-semibold">导师</span>
            )}
          </div>
        ))}
      </div>

      {/* Messages */}
      <div className="flex-1 overflow-y-auto px-4 py-3 space-y-3">
        <AnimatePresence initial={false}>
          {chatMessages.map((msg) => {
            const user = getUser(msg.userId)
            if (!user) return null

            if (msg.type === 'system') {
              return (
                <motion.div
                  key={msg.id}
                  initial={{ opacity: 0, y: 8 }}
                  animate={{ opacity: 1, y: 0 }}
                  className="text-center text-[10px] text-text-muted py-1"
                >
                  {msg.content}
                </motion.div>
              )
            }

            const isMe = msg.userId === 'me'

            return (
              <motion.div
                key={msg.id}
                initial={{ opacity: 0, y: 8 }}
                animate={{ opacity: 1, y: 0 }}
                className={cn('flex gap-2', isMe && 'flex-row-reverse')}
              >
                <div
                  className={cn(
                    'flex h-7 w-7 shrink-0 items-center justify-center rounded-full text-[9px] font-bold',
                    isMe ? 'bg-accent-red/20 text-accent-red' : 'bg-surface-4 text-text-secondary',
                  )}
                >
                  {user.avatar}
                </div>
                <div className={cn('max-w-[85%]', isMe && 'items-end')}>
                  <div className={cn('flex items-center gap-2 mb-0.5', isMe && 'flex-row-reverse')}>
                    <span className="text-[10px] font-medium text-text-secondary">{user.name}</span>
                    <span className="text-[9px] text-text-muted">{msg.timestamp}</span>
                  </div>
                  {msg.type === 'code' ? (
                    <pre className="rounded-lg border border-border bg-surface-0 px-3 py-2 font-mono text-[11px] text-accent-teal leading-relaxed whitespace-pre-wrap">
                      {msg.content}
                    </pre>
                  ) : (
                    <div
                      className={cn(
                        'rounded-lg px-3 py-2 text-xs leading-relaxed',
                        isMe
                          ? 'bg-accent-red/15 text-text-primary border border-accent-red/20'
                          : 'bg-surface-3 text-text-primary',
                      )}
                    >
                      {msg.content}
                    </div>
                  )}
                </div>
              </motion.div>
            )
          })}
        </AnimatePresence>
      </div>

      {/* Input */}
      <div className="border-t border-border-subtle p-3">
        <div className="flex items-center gap-1 rounded-lg border border-border-subtle bg-surface-0 px-2 py-1.5 focus-within:border-accent-teal/40 focus-within:ring-1 focus-within:ring-accent-teal/20">
          <Button variant="ghost" size="icon" className="h-6 w-6 shrink-0">
            <Paperclip size={13} />
          </Button>
          <Button variant="ghost" size="icon" className="h-6 w-6 shrink-0">
            <Hash size={13} />
          </Button>
          <input
            value={input}
            onChange={(e) => setInput(e.target.value)}
            onKeyDown={(e) => e.key === 'Enter' && !e.shiftKey && handleSend()}
            placeholder="发送消息，使用 # 引用 Redis 键..."
            className="flex-1 bg-transparent text-xs text-text-primary placeholder:text-text-muted focus:outline-none"
          />
          <Button variant="ghost" size="icon" className="h-6 w-6 shrink-0">
            <Smile size={13} />
          </Button>
          <Button variant="accent" size="icon" className="h-6 w-6 shrink-0" onClick={handleSend}>
            <Send size={12} />
          </Button>
        </div>
      </div>
    </div>
  )
}
