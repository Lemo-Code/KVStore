import { useState, useRef, useEffect } from 'react'
import { Send, X } from 'lucide-react'
import { Button } from '@/components/ui/button'
import { ScrollArea } from '@/components/ui/scroll-area'
import { cn } from '@/lib/utils'

interface Props { onClose: () => void }

const msgs = [
  { id: '1', user: 'Alice', content: '有人知道 Redis Stack 和普通 Redis 的区别吗？', time: '10:28' },
  { id: '2', user: 'Bob', content: 'Redis Stack 内置了 JSON/Search/TimeSeries 等模块', time: '10:30' },
  { id: '3', user: 'Charlie', content: '对，还有 Bloom Filter 和 Cuckoo Filter', time: '10:33' },
]

export default function ChatPanel({ onClose }: Props) {
  const [input, setInput] = useState('')
  const [list, setList] = useState(msgs)
  const ref = useRef<HTMLDivElement>(null)
  useEffect(() => { ref.current?.scrollIntoView({ behavior: 'smooth' }) }, [list])

  const send = () => {
    if (!input.trim()) return
    setList([...list, { id: Date.now().toString(), user: '我', content: input, time: new Date().toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit' }) }])
    setInput('')
  }

  return (
    <div className="flex h-full flex-col">
      <div className="flex items-center justify-between border-b px-4 py-2.5">
        <span className="text-sm font-semibold">学习交流</span>
        <Button variant="ghost" size="icon-sm" onClick={onClose}><X className="h-4 w-4" /></Button>
      </div>
      <ScrollArea className="flex-1">
        <div className="space-y-3 p-4">
          {list.map((m) => (
            <div key={m.id} className={cn('flex gap-2.5', m.user === '我' && 'flex-row-reverse')}>
              <div className="flex h-7 w-7 shrink-0 items-center justify-center rounded-full bg-muted text-[11px] font-medium">{m.user[0]}</div>
              <div className={cn('max-w-[75%]', m.user === '我' && 'items-end')}>
                <div className="flex items-center gap-1.5 mb-0.5"><span className="text-[11px] font-medium">{m.user}</span><span className="text-[10px] text-muted-foreground">{m.time}</span></div>
                <div className={cn('rounded-xl px-3 py-2 text-[13px] leading-relaxed', m.user === '我' ? 'bg-primary text-primary-foreground rounded-br-md' : 'bg-muted rounded-bl-md')}>{m.content}</div>
              </div>
            </div>
          ))}
          <div ref={ref} />
        </div>
      </ScrollArea>
      <div className="border-t p-3">
        <div className="flex gap-2">
          <input value={input} onChange={(e) => setInput(e.target.value)} onKeyDown={(e) => e.key === 'Enter' && send()}
            className="flex-1 rounded-lg border bg-background px-3 py-2 text-[13px] outline-none focus:ring-2 focus:ring-ring" placeholder="输入消息..." />
          <Button size="icon-sm" className="h-9 w-9" onClick={send}><Send className="h-3.5 w-3.5" /></Button>
        </div>
      </div>
    </div>
  )
}
