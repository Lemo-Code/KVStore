import { useState, useRef, useEffect } from 'react'
import { Bot, X, Send, Loader2, Copy, Check, Sparkles, Minimize2, Maximize2, Brain, Zap, ChevronRight, Terminal, Search, Trash2, Clock, AlertTriangle } from 'lucide-react'
import { Button } from '@/components/ui/button'
import { ScrollArea } from '@/components/ui/scroll-area'
import { Badge } from '@/components/ui/badge'
import { useRedisStore } from '@/stores/redisStore'
import { redisService } from '@/services/redisService'
import { useToast } from '@/hooks/use-toast'
import { cn } from '@/lib/utils'

// ============================================================
// Agent step types
// ============================================================
type StepStatus = 'pending' | 'running' | 'done' | 'error'

interface AgentStep {
  id: number
  description: string  // human-readable "检查内存使用率"
  command?: string     // actual Redis command
  result?: string
  status: StepStatus
}

interface AgentMessage {
  id: string
  role: 'user' | 'agent'
  content: string
  thinking?: string     // agent's reasoning
  plan?: string[]       // high-level plan
  steps?: AgentStep[]   // execution steps
  time?: string
}

// ============================================================
// Mock Agent — simulates multi-step agentic workflow
// ============================================================
function mockAgentThink(prompt: string): { thinking: string; plan: string[]; steps: AgentStep[] } {
  const t = prompt.toLowerCase()

  // Memory analysis
  if (t.includes('内存') || t.includes('memory')) {
    return {
      thinking: '用户想了解 Redis 内存使用情况。我需要先获取 INFO memory，解析使用率，然后给出建议。',
      plan: ['获取服务器 INFO 信息', '解析内存使用率', '评估是否需要清理', '给出优化建议'],
      steps: [
        { id: 1, description: '获取内存信息', command: 'INFO memory', status: 'pending' },
        { id: 2, description: '检查当前 Key 数量', command: 'DBSIZE', status: 'pending' },
        { id: 3, description: '扫描过期 Key', command: 'KEYS *', status: 'pending' },
      ],
    }
  }

  // Key search / list
  if (t.includes('查看') || t.includes('列出') || t.includes('查找') || t.includes('搜索') || t.includes('有哪些')) {
    const pat = t.match(/(user|product|order|cache|config|session|stream|queue|lock|counter|ratelimit)\S*/i)
    const pattern = pat ? pat[0] + '*' : '*'
    return {
      thinking: `用户想查找 Redis 中的 Key。我将使用 SCAN 和 KEYS 来搜索匹配 "${pattern}" 的 Key，然后分析它们的类型分布。`,
      plan: ['搜索匹配的 Key', '分析 Key 类型分布', '检查过期策略'],
      steps: [
        { id: 1, description: `搜索 ${pattern}`, command: `KEYS ${pattern}`, status: 'pending' },
        { id: 2, description: '统计数据库总 Key 数', command: 'DBSIZE', status: 'pending' },
      ],
    }
  }

  // Set/modify
  if (t.includes('设置') || t.includes('修改') || t.includes('改成') || t.includes('更新')) {
    const m = t.match(/(?:设置|修改|改成|更新)\s*(\S+)\s*(?:为|to|=)\s*(.+)/i)
    const key = m ? m[1] : 'mykey'
    const val = m ? m[2].trim() : 'value'
    return {
      thinking: `用户想修改 "${key}" 的值。我需要先检查这个 Key 是否存在，当前值是什么，然后执行更新，最后验证结果。`,
      plan: ['检查 Key 是否存在', '查看当前值', '执行更新', '验证结果'],
      steps: [
        { id: 1, description: `检查 ${key} 是否存在`, command: `EXISTS ${key}`, status: 'pending' },
        { id: 2, description: `查看 ${key} 当前值`, command: `GET ${key}`, status: 'pending' },
        { id: 3, description: `设置 ${key} = ${val}`, command: `SET ${key} "${val}"`, status: 'pending' },
        { id: 4, description: `验证更新结果`, command: `GET ${key}`, status: 'pending' },
      ],
    }
  }

  // Delete
  if (t.includes('删除') || t.includes('移除') || t.includes('清理')) {
    const m = t.match(/(?:删除|移除|清理)\s*(\S+)/i)
    const key = m ? m[1] : 'unknown'
    return {
      thinking: `用户想删除 "${key}"。⚠️ 这是危险操作。我需要先确认 Key 存在，展示当前数据，删除后验证。`,
      plan: ['确认 Key 存在', '查看当前数据（最后确认）', '执行删除', '验证已删除'],
      steps: [
        { id: 1, description: `检查 ${key} 是否存在`, command: `EXISTS ${key}`, status: 'pending' },
        { id: 2, description: `查看 ${key} 类型和 TTL`, command: `TYPE ${key}`, status: 'pending' },
        { id: 3, description: `⚠️ 删除 ${key}`, command: `DEL ${key}`, status: 'pending' },
        { id: 4, description: `验证删除结果`, command: `EXISTS ${key}`, status: 'pending' },
      ],
    }
  }

  // Info / status
  if (t.includes('状态') || t.includes('info') || t.includes('运行') || t.includes('服务器')) {
    return {
      thinking: '用户想了解 Redis 服务器整体状态。我将获取 INFO、客户端列表、慢查询日志。',
      plan: ['获取服务器 INFO', '检查客户端连接', '查看慢查询', '总结服务器健康状态'],
      steps: [
        { id: 1, description: '获取服务器信息', command: 'INFO server', status: 'pending' },
        { id: 2, description: '检查连接客户端', command: 'CLIENT LIST', status: 'pending' },
        { id: 3, description: '查看慢查询日志', command: 'SLOWLOG GET 5', status: 'pending' },
      ],
    }
  }

  // Default — general analysis
  return {
    thinking: `用户询问: "${prompt.slice(0, 80)}"。让我从 Redis 角度分析，获取相关信息后给出回答。`,
    plan: ['分析用户意图', '查询相关数据', '给出建议'],
    steps: [
      { id: 1, description: 'PING 检查连接', command: 'PING', status: 'pending' },
      { id: 2, description: '获取基本统计', command: 'INFO stats', status: 'pending' },
    ],
  }
}

// ============================================================
// Component
// ============================================================
export default function FloatingAI({ onClose }: { onClose: () => void }) {
  const [msgs, setMsgs] = useState<AgentMessage[]>([{
    id: 'welcome', role: 'agent',
    content: '你好！我是 Redis AI Agent 🤖\n\n我可以**自动规划并执行**多步操作。比如：\n\n• "分析内存使用情况"\n• "查看所有 user 相关的 key"\n• "修改配置并验证"\n• "清理过期缓存"\n\n告诉我你想做什么，我来规划执行。',
  }])
  const [input, setInput] = useState('')
  const [busy, setBusy] = useState(false)
  const [minimized, setMinimized] = useState(false)
  const [copied, setCopied] = useState<string | null>(null)
  const scrollRef = useRef<HTMLDivElement>(null)
  const selectedKey = useRedisStore((s) => s.selectedKey)
  const { add: toast } = useToast()

  useEffect(() => { scrollRef.current?.scrollTo({ top: scrollRef.current.scrollHeight, behavior: 'smooth' }) }, [msgs])

  // ========== AGENT EXECUTION ==========
  const runAgent = async (prompt: string) => {
    setBusy(true)

    // 1. Create agent message with thinking
    const agentMsg: AgentMessage = {
      id: Date.now().toString(), role: 'agent',
      content: '', thinking: '', plan: [], steps: [],
      time: new Date().toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit' }),
    }

    // 2. Simulate agent "thinking" phase
    const { thinking, plan, steps } = mockAgentThink(prompt)
    agentMsg.thinking = thinking
    agentMsg.plan = plan
    agentMsg.steps = steps
    agentMsg.content = '正在分析你的请求...'
    setMsgs((p) => [...p, agentMsg])
    await sleep(800)

    // 3. Execute each step
    for (const step of agentMsg.steps) {
      // Mark step as running
      step.status = 'running'
      agentMsg.content = `执行步骤 ${step.id}/${steps.length}: ${step.description}`
      setMsgs((p) => p.map((m) => m.id === agentMsg.id ? { ...agentMsg } : m))
      await sleep(500)

      // Execute command
      try {
        const result = await redisService.executeCommand(step.command || 'PING')
        step.result = result
        step.status = result.startsWith('(error)') ? 'error' : 'done'
      } catch (e) {
        step.result = `Error: ${e instanceof Error ? e.message : 'Unknown'}`
        step.status = 'error'
      }

      agentMsg.content = `步骤 ${step.id}/${steps.length}: ${step.description}`
      setMsgs((p) => p.map((m) => m.id === agentMsg.id ? { ...agentMsg } : m))
      await sleep(400)
    }

    // 4. Generate summary
    const done = agentMsg.steps.filter((s) => s.status === 'done').length
    const failed = agentMsg.steps.filter((s) => s.status === 'error').length
    agentMsg.content = `✅ 执行完成: ${done}/${steps.length} 个步骤成功` + (failed > 0 ? `, ${failed} 个失败` : '')
    setMsgs((p) => p.map((m) => m.id === agentMsg.id ? { ...agentMsg } : m))

    toast(done === steps.length ? 'success' : 'warning', `Agent 执行完成`, `${done} 成功, ${failed} 失败`)
    setBusy(false)
  }

  const send = (text: string) => {
    if (!text.trim() || busy) return
    const userMsg: AgentMessage = { id: Date.now().toString(), role: 'user', content: text }
    setMsgs((p) => [...p, userMsg])
    setInput('')
    runAgent(text)
  }

  return (
    <div className={cn(
      'fixed z-50 flex flex-col overflow-hidden rounded-2xl border bg-card shadow-2xl transition-all',
      minimized ? 'bottom-6 right-6 h-12 w-64' : 'bottom-6 right-20 h-[560px] w-[480px]',
    )}>
      {/* Header */}
      <div className="flex items-center gap-2 border-b bg-gradient-to-r from-violet-500 to-purple-600 px-4 py-2.5 text-white shrink-0">
        <Brain className="h-4 w-4" />
        <span className="flex-1 text-sm font-semibold">Redis AI Agent</span>
        <Badge variant="secondary" className="bg-white/20 text-white text-[10px]">Auto</Badge>
        <button onClick={() => setMinimized(!minimized)} className="rounded p-1 text-white/70 hover:bg-white/10 hover:text-white">
          {minimized ? <Maximize2 className="h-3.5 w-3.5" /> : <Minimize2 className="h-3.5 w-3.5" />}
        </button>
        <button onClick={onClose} className="rounded p-1 text-white/70 hover:bg-white/10 hover:text-white">
          <X className="h-3.5 w-3.5" />
        </button>
      </div>

      {!minimized && (
        <>
          {/* Context bar */}
          {selectedKey && (
            <div className="border-b bg-violet-50 px-4 py-1.5 text-[11px] text-violet-700 flex items-center gap-1.5 shrink-0 dark:bg-violet-950 dark:text-violet-300">
              <Sparkles className="h-3 w-3" /> 上下文: <code className="font-mono font-semibold">{selectedKey}</code>
            </div>
          )}

          {/* Messages */}
          <ScrollArea className="flex-1" ref={scrollRef}>
            <div className="space-y-4 p-4">
              {msgs.map((m) => (
                <div key={m.id}>
                  {/* User message */}
                  {m.role === 'user' && (
                    <div className="flex gap-2.5 justify-end">
                      <div className="max-w-[85%] rounded-xl bg-primary text-primary-foreground rounded-br-md px-3 py-2 text-[13px] leading-relaxed">
                        {m.content}
                      </div>
                      <div className="flex h-7 w-7 shrink-0 items-center justify-center rounded-full bg-primary text-[11px] font-medium text-primary-foreground">我</div>
                    </div>
                  )}

                  {/* Agent message */}
                  {m.role === 'agent' && (
                    <div className="space-y-3">
                      <div className="flex gap-2.5">
                        <div className="flex h-7 w-7 shrink-0 items-center justify-center rounded-full bg-gradient-to-br from-violet-500 to-purple-600 text-[11px] font-medium text-white"><Brain className="h-3.5 w-3.5" /></div>
                        <div className="min-w-0 flex-1">
                          <span className="text-[11px] font-medium text-muted-foreground">AI Agent {m.time}</span>

                          {/* Thinking process */}
                          {m.thinking && (
                            <div className="mt-1.5 rounded-lg border border-amber-200 bg-amber-50 p-3 dark:border-amber-800 dark:bg-amber-950">
                              <div className="flex items-center gap-1.5 text-[11px] font-semibold text-amber-700 dark:text-amber-400 mb-1">
                                <Brain className="h-3 w-3" /> 思考过程
                              </div>
                              <p className="text-[12px] text-amber-800 dark:text-amber-300 leading-relaxed">{m.thinking}</p>
                            </div>
                          )}

                          {/* Plan */}
                          {m.plan && m.plan.length > 0 && (
                            <div className="mt-2 rounded-lg border bg-muted/30 p-3">
                              <div className="flex items-center gap-1.5 text-[11px] font-semibold text-muted-foreground mb-2">
                                <Zap className="h-3 w-3" /> 执行计划
                              </div>
                              <div className="space-y-1">
                                {m.plan.map((s, i) => {
                                  const step = m.steps?.[i]
                                  const done = step?.status === 'done'
                                  const running = step?.status === 'running'
                                  const err = step?.status === 'error'
                                  return (
                                    <div key={i} className="flex items-center gap-2 text-[12px]">
                                      {done ? <Check className="h-3 w-3 text-emerald-500 shrink-0" /> :
                                       err ? <AlertTriangle className="h-3 w-3 text-red-500 shrink-0" /> :
                                       running ? <Loader2 className="h-3 w-3 animate-spin text-blue-500 shrink-0" /> :
                                       <span className="w-3 h-3 rounded-full border border-muted-foreground/30 shrink-0" />}
                                      <span className={cn(
                                        done && 'text-emerald-700 dark:text-emerald-400 line-through',
                                        err && 'text-red-500',
                                        running && 'text-blue-600 font-medium',
                                      )}>{s}</span>
                                    </div>
                                  )
                                })}
                              </div>
                            </div>
                          )}

                          {/* Execution steps with results */}
                          {m.steps && m.steps.length > 0 && (
                            <div className="mt-2 space-y-2">
                              {m.steps.map((step) => (
                                <div key={step.id} className={cn(
                                  'rounded-lg border p-3 transition-all',
                                  step.status === 'running' && 'border-blue-200 bg-blue-50/50 dark:border-blue-800 dark:bg-blue-950/50',
                                  step.status === 'done' && 'border-emerald-200 bg-emerald-50/30 dark:border-emerald-800 dark:bg-emerald-950/30',
                                  step.status === 'error' && 'border-red-200 bg-red-50/30 dark:border-red-800 dark:bg-red-950/30',
                                  step.status === 'pending' && 'opacity-50',
                                )}>
                                  <div className="flex items-center gap-2 mb-1.5">
                                    {step.status === 'running' && <Loader2 className="h-3.5 w-3.5 animate-spin text-blue-500" />}
                                    {step.status === 'done' && <Check className="h-3.5 w-3.5 text-emerald-500" />}
                                    {step.status === 'error' && <AlertTriangle className="h-3.5 w-3.5 text-red-500" />}
                                    {step.status === 'pending' && <span className="h-3.5 w-3.5 rounded-full border border-muted-foreground/30" />}
                                    <span className="text-[12px] font-medium">{step.description}</span>
                                  </div>
                                  {step.command && (
                                    <div className="ml-5.5 rounded bg-black/5 px-2 py-1 font-mono text-[11px] dark:bg-white/5">
                                      <span className="text-violet-500">$</span> {step.command}
                                    </div>
                                  )}
                                  {step.result && (
                                    <pre className={cn(
                                      'ml-5.5 mt-1 whitespace-pre-wrap text-[11px] font-mono px-2 py-1 rounded',
                                      step.status === 'error' ? 'text-red-600 bg-red-50 dark:text-red-400 dark:bg-red-950/50' : 'text-muted-foreground bg-muted/30',
                                    )}>{step.result.length > 500 ? step.result.slice(0, 500) + '\n... (truncated)' : step.result}</pre>
                                  )}
                                </div>
                              ))}
                            </div>
                          )}

                          {/* Summary */}
                          {m.content && m.steps && m.steps.every((s) => s.status === 'done' || s.status === 'error') && (
                            <div className={cn(
                              'mt-2 rounded-lg p-3 text-[13px]',
                              m.steps.some((s) => s.status === 'error')
                                ? 'bg-red-50 text-red-800 dark:bg-red-950 dark:text-red-300'
                                : 'bg-emerald-50 text-emerald-800 dark:bg-emerald-950 dark:text-emerald-300',
                            )}>
                              {m.content}
                            </div>
                          )}
                        </div>
                      </div>

                      {/* Copy button */}
                      <div className="ml-9">
                        <button onClick={() => { navigator.clipboard.writeText(JSON.stringify({ plan: m.plan, steps: m.steps?.map((s) => ({ desc: s.description, cmd: s.command, result: s.result })) }, null, 2)); setCopied(m.id); setTimeout(() => setCopied(null), 2000) }}
                          className="text-[11px] text-muted-foreground hover:text-foreground flex items-center gap-1">
                          {copied === m.id ? <Check className="h-3 w-3 text-emerald-500" /> : <Copy className="h-3 w-3" />}
                          复制结果
                        </button>
                      </div>
                    </div>
                  )}
                </div>
              ))}
            </div>
          </ScrollArea>

          {/* Input */}
          <div className="border-t p-3 shrink-0">
            <div className="flex gap-2">
              <input
                value={input}
                onChange={(e) => setInput(e.target.value)}
                onKeyDown={(e) => e.key === 'Enter' && send(input)}
                className="flex-1 rounded-lg border bg-background px-3 py-2 text-[13px] outline-none focus:ring-2 focus:ring-violet-500/20"
                placeholder="告诉 Agent 你想做什么..."
                disabled={busy}
              />
              <button onClick={() => send(input)} disabled={busy || !input.trim()}
                className="flex h-9 w-9 shrink-0 items-center justify-center rounded-lg bg-violet-500 text-white transition-all hover:bg-violet-600 active:scale-95 disabled:opacity-50">
                {busy ? <Loader2 className="h-4 w-4 animate-spin" /> : <Send className="h-4 w-4" />}
              </button>
            </div>
          </div>
        </>
      )}
    </div>
  )
}

function sleep(ms: number) { return new Promise((r) => setTimeout(r, ms)) }
