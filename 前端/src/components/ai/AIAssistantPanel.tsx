import { Button } from '@/components/ui/Button'
import { useAppStore } from '@/stores/appStore'
import { AnimatePresence, motion } from 'framer-motion'
import {
  Bot,
  Copy,
  Lightbulb,
  Send,
  Sparkles,
  ThumbsDown,
  ThumbsUp,
} from 'lucide-react'
import { useState } from 'react'

export function AIAssistantPanel() {
  const { aiMessages, addAIMessage } = useAppStore()
  const [input, setInput] = useState('')
  const [isTyping, setIsTyping] = useState(false)

  const handleSend = (text?: string) => {
    const content = text ?? input
    if (!content.trim()) return

    addAIMessage({
      id: `ai-user-${Date.now()}`,
      role: 'user',
      content,
      timestamp: new Date().toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit' }),
    })
    setInput('')
    setIsTyping(true)

    setTimeout(() => {
      setIsTyping(false)
      addAIMessage({
        id: `ai-resp-${Date.now()}`,
        role: 'assistant',
        content:
          '这是一个 UI 演示回复。在实际接入后，AI 将基于你当前选中的 Redis 键和命令历史，提供上下文感知的智能分析。',
        timestamp: new Date().toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit' }),
        suggestions: ['分析当前键结构', '推荐优化方案', '生成练习题'],
      })
    }, 1200)
  }

  return (
    <div className="flex h-full flex-col">
      {/* Header */}
      <div className="border-b border-border-subtle px-4 py-3">
        <div className="flex items-center gap-2">
          <div className="flex h-8 w-8 items-center justify-center rounded-lg bg-gradient-to-br from-accent-purple to-accent-blue shadow-lg shadow-accent-purple/20">
            <Bot size={16} className="text-white" />
          </div>
          <div>
            <div className="flex items-center gap-1.5">
              <span className="text-sm font-semibold">AI Redis 导师</span>
              <Sparkles size={12} className="text-accent-amber" />
            </div>
            <span className="text-[10px] text-text-muted">上下文感知 · 命令分析 · 学习辅导</span>
          </div>
        </div>
      </div>

      {/* Quick actions */}
      <div className="flex gap-1.5 px-4 py-2 border-b border-border-subtle overflow-x-auto">
        {['解释当前键', '命令纠错', '性能建议', '生成练习'].map((action) => (
          <button
            key={action}
            onClick={() => handleSend(action)}
            className="shrink-0 rounded-full border border-border-subtle bg-surface-2 px-2.5 py-1 text-[10px] text-text-secondary hover:border-accent-purple/30 hover:text-accent-purple transition-colors"
          >
            <Lightbulb size={10} className="inline mr-1" />
            {action}
          </button>
        ))}
      </div>

      {/* Messages */}
      <div className="flex-1 overflow-y-auto px-4 py-3 space-y-4">
        <AnimatePresence initial={false}>
          {aiMessages.map((msg) => (
            <motion.div
              key={msg.id}
              initial={{ opacity: 0, y: 10 }}
              animate={{ opacity: 1, y: 0 }}
              className={msg.role === 'user' ? 'flex justify-end' : ''}
            >
              {msg.role === 'assistant' && (
                <div className="flex gap-2.5">
                  <div className="flex h-7 w-7 shrink-0 items-center justify-center rounded-lg bg-accent-purple/15">
                    <Bot size={14} className="text-accent-purple" />
                  </div>
                  <div className="flex-1 min-w-0 space-y-2">
                    <div className="rounded-lg rounded-tl-sm bg-surface-3 border border-border-subtle px-3 py-2.5 text-xs leading-relaxed text-text-primary whitespace-pre-wrap">
                      {msg.content.split('**').map((part, i) =>
                        i % 2 === 1 ? (
                          <strong key={i} className="text-accent-amber font-semibold">
                            {part}
                          </strong>
                        ) : (
                          <span key={i}>{part}</span>
                        ),
                      )}
                    </div>

                    {msg.codeBlock && (
                      <div className="rounded-lg border border-border bg-surface-0 overflow-hidden">
                        <div className="flex items-center justify-between px-3 py-1 bg-surface-2 border-b border-border-subtle">
                          <span className="text-[9px] font-mono text-text-muted">redis</span>
                          <button className="text-text-muted hover:text-text-secondary">
                            <Copy size={11} />
                          </button>
                        </div>
                        <pre className="p-3 font-mono text-[11px] text-accent-teal leading-relaxed overflow-x-auto">
                          {msg.codeBlock}
                        </pre>
                      </div>
                    )}

                    {msg.suggestions && (
                      <div className="flex flex-wrap gap-1.5">
                        {msg.suggestions.map((s) => (
                          <button
                            key={s}
                            onClick={() => handleSend(s)}
                            className="rounded-md border border-accent-purple/20 bg-accent-purple/5 px-2 py-1 text-[10px] text-accent-purple hover:bg-accent-purple/10 transition-colors"
                          >
                            {s}
                          </button>
                        ))}
                      </div>
                    )}

                    <div className="flex gap-1">
                      <button className="p-1 rounded text-text-muted hover:text-success transition-colors">
                        <ThumbsUp size={12} />
                      </button>
                      <button className="p-1 rounded text-text-muted hover:text-danger transition-colors">
                        <ThumbsDown size={12} />
                      </button>
                    </div>
                  </div>
                </div>
              )}

              {msg.role === 'user' && (
                <div className="max-w-[85%] rounded-lg rounded-tr-sm bg-accent-red/10 border border-accent-red/20 px-3 py-2 text-xs text-text-primary">
                  {msg.content}
                </div>
              )}
            </motion.div>
          ))}
        </AnimatePresence>

        {isTyping && (
          <motion.div
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            className="flex items-center gap-2 text-[10px] text-text-muted"
          >
            <div className="flex gap-1">
              {[0, 1, 2].map((i) => (
                <span
                  key={i}
                  className="h-1.5 w-1.5 rounded-full bg-accent-purple animate-pulse-dot"
                  style={{ animationDelay: `${i * 0.2}s` }}
                />
              ))}
            </div>
            AI 正在分析...
          </motion.div>
        )}
      </div>

      {/* Input */}
      <div className="border-t border-border-subtle p-3">
        <div className="rounded-lg border border-border-subtle bg-surface-0 focus-within:border-accent-purple/40 focus-within:ring-1 focus-within:ring-accent-purple/20">
          <textarea
            value={input}
            onChange={(e) => setInput(e.target.value)}
            onKeyDown={(e) => {
              if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault()
                handleSend()
              }
            }}
            placeholder="向 AI 导师提问 Redis 相关问题..."
            rows={2}
            className="w-full resize-none bg-transparent px-3 pt-2.5 text-xs text-text-primary placeholder:text-text-muted focus:outline-none"
          />
          <div className="flex items-center justify-between px-2 pb-2">
            <span className="text-[9px] text-text-muted">Shift+Enter 换行</span>
            <Button
              variant="accent"
              size="sm"
              onClick={() => handleSend()}
              className="!bg-accent-purple hover:!bg-accent-purple/80 shadow-accent-purple/20"
            >
              <Send size={12} />
              发送
            </Button>
          </div>
        </div>
      </div>
    </div>
  )
}
