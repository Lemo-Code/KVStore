import { useState, useRef, useEffect } from 'react'
import { Bot, Send, Loader2, X, Copy, Check, Sparkles } from 'lucide-react'
import { Button } from '@/components/ui/button'
import { ScrollArea } from '@/components/ui/scroll-area'
import { cn } from '@/lib/utils'

interface Props { onClose: () => void }

const prompts = ['Redis Cluster 原理?', '分布式锁实现?', '持久化RDB vs AOF?']

export default function AIPanel({ onClose }: Props) {
  const [msgs, setMsgs] = useState<{ id: string; role: 'user' | 'ai'; content: string }[]>([])
  const [input, setInput] = useState('')
  const [streaming, setStreaming] = useState(false)
  const [copied, setCopied] = useState<string | null>(null)
  const ref = useRef<HTMLDivElement>(null)
  useEffect(() => { ref.current?.scrollIntoView({ behavior: 'smooth' }) }, [msgs])

  const send = (text: string) => {
    if (!text.trim() || streaming) return
    const u = { id: Date.now().toString(), role: 'user' as const, content: text }
    const a = { id: (Date.now() + 1).toString(), role: 'ai' as const, content: '' }
    setMsgs((p) => [...p, u, a])
    setInput('')
    setStreaming(true)
    const resp = `这是关于 "${text.slice(0, 30)}..." 的 AI 回复。\n\n当后端 LLM 服务接入后，这里会展示真实的流式 AI 响应。`
    let i = 0
    const iv = setInterval(() => {
      if (i < resp.length) { setMsgs((p) => p.map((m) => m.id === a.id ? { ...m, content: m.content + resp[i] } : m)); i++ }
      else { clearInterval(iv); setStreaming(false) }
    }, 25)
  }

  return (
    <div className="flex h-full flex-col">
      <div className="flex items-center justify-between border-b px-4 py-2.5">
        <div className="flex items-center gap-2"><Bot className="h-4 w-4 text-violet-500" /><span className="text-sm font-semibold">AI 助手</span></div>
        <Button variant="ghost" size="icon-sm" onClick={onClose}><X className="h-4 w-4" /></Button>
      </div>
      <ScrollArea className="flex-1">
        <div className="p-4 space-y-4">
          {msgs.length === 0 ? (
            <div className="pt-8 text-center">
              <Sparkles className="mx-auto h-8 w-8 text-violet-400" />
              <p className="mt-3 text-sm font-medium">有什么可以帮你的？</p>
              <div className="mt-3 space-y-2">
                {prompts.map((p) => <button key={p} onClick={() => send(p)} className="block w-full rounded-lg border p-2.5 text-left text-[13px] hover:bg-muted transition-colors">{p}</button>)}
              </div>
            </div>
          ) : (
            msgs.map((m) => (
              <div key={m.id} className={cn('flex gap-2.5', m.role === 'user' && 'flex-row-reverse')}>
                <div className={cn('flex h-7 w-7 shrink-0 items-center justify-center rounded-full text-[11px] font-medium text-white', m.role === 'ai' ? 'bg-violet-500' : 'bg-primary')}>{m.role === 'ai' ? <Bot className="h-3.5 w-3.5" /> : '我'}</div>
                <div className={cn('max-w-[80%] rounded-xl px-3 py-2 text-[13px] leading-relaxed', m.role === 'user' ? 'bg-primary text-primary-foreground rounded-br-md' : 'bg-muted rounded-bl-md')}>
                  {m.content || <Loader2 className="h-3.5 w-3.5 animate-spin" />}
                </div>
                {m.role === 'ai' && m.content && !streaming && (
                  <button onClick={() => { navigator.clipboard.writeText(m.content); setCopied(m.id); setTimeout(() => setCopied(null), 2000) }}
                    className="text-[11px] text-muted-foreground hover:text-foreground">{copied === m.id ? <Check className="h-3 w-3 text-emerald-500" /> : <Copy className="h-3 w-3" />}</button>
                )}
              </div>
            ))
          )}
          <div ref={ref} />
        </div>
      </ScrollArea>
      <div className="border-t p-3">
        <div className="flex gap-2">
          <input value={input} onChange={(e) => setInput(e.target.value)} onKeyDown={(e) => e.key === 'Enter' && send(input)}
            className="flex-1 rounded-lg border bg-background px-3 py-2 text-[13px] outline-none focus:ring-2 focus:ring-ring" placeholder="输入问题..."
            disabled={streaming} />
          <Button size="icon-sm" className="h-9 w-9" onClick={() => send(input)} disabled={streaming}>
            {streaming ? <Loader2 className="h-3.5 w-3.5 animate-spin" /> : <Send className="h-3.5 w-3.5" />}
          </Button>
        </div>
      </div>
    </div>
  )
}
