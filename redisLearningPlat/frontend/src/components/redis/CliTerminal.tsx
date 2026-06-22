import { useState, useRef, useEffect, useCallback, useMemo } from 'react'
import { cn } from '@/lib/utils'
import { useRedisStore } from '@/stores/redisStore'
import { redisService } from '@/services/redisService'
import { Button } from '@/components/ui/button'
import { ScrollArea } from '@/components/ui/scroll-area'
import { mockCliCommands } from '@/mock/serverInfo'
import { Terminal, Trash2, HelpCircle, CornerDownLeft } from 'lucide-react'
import type { CliCommand } from '@/types/redis'

// Commands that expect a key name as first argument
const KEY_AWARE_COMMANDS = new Set([
  'GET', 'SET', 'DEL', 'TYPE', 'TTL', 'EXISTS', 'EXPIRE', 'PERSIST',
  'HGET', 'HGETALL', 'HSET', 'HDEL', 'HLEN', 'HKEYS', 'HVALS',
  'LPUSH', 'RPUSH', 'LPOP', 'RPOP', 'LLEN', 'LRANGE',
  'SADD', 'SREM', 'SMEMBERS', 'SCARD', 'SISMEMBER',
  'ZADD', 'ZREM', 'ZRANGE', 'ZCARD', 'ZSCORE', 'ZRANK',
  'XADD', 'XREAD', 'XLEN', 'XINFO',
  'RENAME', 'OBJECT', 'DUMP', 'RESTORE',
])

interface Entry { id: number; command: string; result: string; isError: boolean }

export function CliTerminal() {
  const activeId = useRedisStore((s) => s.activeConnectionId)
  const db = useRedisStore((s) => s.activeDb)
  const conns = useRedisStore((s) => s.connections)
  const [history, setHistory] = useState<Entry[]>([])
  const [input, setInput] = useState('')
  const [histIdx, setHistIdx] = useState(-1)
  const [running, setRunning] = useState(false)
  const [showHelp, setShowHelp] = useState(false)
  const [ac, setAc] = useState<string[]>([])
  const [acIdx, setAcIdx] = useState(0)
  const inputRef = useRef<HTMLInputElement>(null)
  const outputRef = useRef<HTMLDivElement>(null)
  const idRef = useRef(0)

  const c = conns.find((x) => x.id === activeId)
  const prompt = c ? `${c.name} ${c.host}:${c.port}[${db}]>` : 'not connected>'

  // Focus input on mount and after execution
  useEffect(() => { inputRef.current?.focus() }, [])
  useEffect(() => { inputRef.current?.focus() }, [running])

  // Auto-scroll output
  useEffect(() => {
    const el = outputRef.current
    if (el) el.scrollTop = el.scrollHeight
  }, [history])

  // Smart suggestions: command names + key names
  const suggestions = useMemo(() => {
    if (!input.trim()) return []
    const parts = input.trim().split(/\s+/)
    const first = parts[0]?.toUpperCase() || ''

    // If we have a command that expects a key, and user is typing arg 1, suggest key names
    if (parts.length >= 2 && KEY_AWARE_COMMANDS.has(first)) {
      const prefix = parts.slice(1).join(' ')
      const matchingKeys = mockKeys
        .filter((k) => k.name.includes(prefix))
        .slice(0, 10)
        .map((k) => ({
          command: `${first} ${k.name}`,
          description: `${k.type} · TTL: ${k.ttl > 0 ? k.ttl + 's' : k.ttl === -1 ? '∞' : 'expired'} · ${k.size} bytes`,
          group: 'Key',
        }))
      if (matchingKeys.length > 0) return matchingKeys
    }

    // Otherwise suggest commands
    return mockCliCommands
      .filter((x) => x.command.startsWith(first) && x.command !== first)
      .slice(0, 10)
  }, [input])

  const run = useCallback(async (cmd: string) => {
    const trimmed = cmd.trim()
    if (!trimmed) return
    if (trimmed.toUpperCase() === 'CLEAR') { setHistory([]); setInput(''); return }

    setRunning(true)
    try {
      const result = await redisService.executeCommand(trimmed)
      setHistory((p) => [...p, { id: ++idRef.current, command: trimmed, result, isError: result.startsWith('(error)') }])
    } catch (e) {
      setHistory((p) => [...p, { id: ++idRef.current, command: trimmed, result: `(error) ${e instanceof Error ? e.message : 'Failed'}`, isError: true }])
    }
    setRunning(false)
    setHistIdx(-1)
  }, [])

  const cmdList = history.filter((h) => !h.command.startsWith('HELP')).map((h) => h.command)

  const onKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter') { e.preventDefault(); run(input); setInput(''); setAc([]); return }
    if (e.key === 'Tab') { e.preventDefault(); if (suggestions.length > 0) { setInput(suggestions[acIdx % suggestions.length].command + ' '); setAc([]) }; return }
    if (e.key === 'ArrowUp') { e.preventDefault(); const n = histIdx < cmdList.length - 1 ? histIdx + 1 : histIdx; setHistIdx(n); if (n >= 0) setInput(cmdList[cmdList.length - 1 - n] || ''); return }
    if (e.key === 'ArrowDown') { e.preventDefault(); const n = histIdx > 0 ? histIdx - 1 : -1; setHistIdx(n); setInput(n >= 0 ? cmdList[cmdList.length - 1 - n] || '' : ''); return }
    if (e.key === 'Escape') { setAc([]); return }
  }

  const grouped = useMemo(() => {
    const g: Record<string, CliCommand[]> = {}
    mockCliCommands.forEach((x) => { if (!g[x.group]) g[x.group] = []; g[x.group].push(x) })
    return g
  }, [])

  return (
    <div className="flex h-full gap-0">
      {/* Terminal area */}
      <div className="flex flex-1 flex-col rounded-lg border bg-[#1e1e20] overflow-hidden">
        {/* Toolbar */}
        <div className="flex items-center gap-1.5 border-b border-white/5 px-3 py-1.5">
          <Terminal className="h-3.5 w-3.5 text-white/40" />
          <span className="text-xs font-medium text-white/50 font-mono">{prompt}</span>
          <div className="flex-1" />
          <button onClick={() => setHistory([])} className="rounded p-1 text-white/30 hover:bg-white/5 hover:text-white/60" title="清屏"><Trash2 className="h-3 w-3" /></button>
          <button onClick={() => setShowHelp(!showHelp)} className={cn('rounded p-1 hover:bg-white/5', showHelp ? 'text-white' : 'text-white/30 hover:text-white/60')} title="帮助"><HelpCircle className="h-3 w-3" /></button>
        </div>

        {/* Output — plain div for reliable scroll ref */}
        <div ref={outputRef} className="flex-1 overflow-auto p-3 font-mono text-[13px] leading-relaxed">
          {history.length === 0 && (
            <div className="text-white/20 select-none">
              Redis CLI · 输入命令后按 Enter 执行 · ↑↓ 浏览历史 · Tab 自动补全 · CLEAR 清屏
            </div>
          )}
          {history.map((e) => (
            <div key={e.id} className="mb-2.5">
              <div className="flex gap-2">
                <span className="text-blue-400 shrink-0 select-none">&gt;</span>
                <span className="text-white/90">{e.command}</span>
              </div>
              <pre className={cn('mt-0.5 ml-5 whitespace-pre-wrap break-all', e.isError ? 'text-red-400' : 'text-white/60')}>{e.result}</pre>
            </div>
          ))}
        </div>

        {/* Input line */}
        <div className="border-t border-white/5">
          {ac.length > 0 && (
            <div className="flex flex-wrap gap-1 border-b border-white/5 px-3 py-1.5">
              {ac.map((cmd, i) => {
                // Extract command + description from the suggestion item
                const [cmdName, ...descParts] = cmd.split('|')
                const desc = descParts.join('|')
                return (
                  <button key={cmd} onClick={() => { setInput(cmdName.trim() + ' '); setAc([]); inputRef.current?.focus() }}
                    className={cn('rounded px-1.5 py-0.5 text-[11px] font-mono transition-colors flex items-center gap-2', i === acIdx % ac.length ? 'bg-white/10 text-white' : 'text-white/30 hover:text-white/60')}
                    title={desc}>
                    <span>{cmdName}</span>
                    {desc && <span className="text-[10px] text-white/20">{desc}</span>}
                  </button>
                )
              })}
            </div>
          )}
          <div className="flex items-center gap-2 px-3 py-2">
            <span className="shrink-0 text-blue-400 font-mono text-[13px] select-none">&gt;</span>
            <input
              ref={inputRef}
              value={input}
              onChange={(e) => { setInput(e.target.value); setAc(suggestions.map((s) => s.command + (s.description ? `|${s.description}` : ''))); setAcIdx(0) }}
              onKeyDown={onKeyDown}
              className="flex-1 bg-transparent text-[13px] text-white/90 font-mono outline-none placeholder:text-white/15"
              placeholder="输入命令..."
              spellCheck={false}
              autoComplete="off"
              disabled={running}
            />
            <button onClick={() => { run(input); setInput('') }} disabled={running}
              className="shrink-0 rounded p-1 text-white/30 hover:bg-white/5 hover:text-white/60 disabled:opacity-20">
              <CornerDownLeft className="h-3.5 w-3.5" />
            </button>
          </div>
        </div>
      </div>

      {/* Help sidebar */}
      {showHelp && (
        <div className="ml-2 w-52 shrink-0 rounded-lg border bg-card overflow-hidden hidden lg:flex flex-col">
          <div className="border-b px-3 py-2 text-xs font-semibold">命令参考</div>
          <ScrollArea className="flex-1">
            {Object.entries(grouped).map(([g, cmds]) => (
              <div key={g} className="border-b last:border-b-0 px-3 py-2">
                <div className="text-[10px] font-semibold uppercase text-muted-foreground mb-1">{g}</div>
                {cmds.map((cmd) => (
                  <button key={cmd.command} onClick={() => { setInput(cmd.command + ' '); inputRef.current?.focus(); setShowHelp(false) }}
                    className="block w-full text-left text-[11px] py-0.5 hover:text-foreground font-mono">
                    <span className="text-primary">{cmd.command}</span>
                    <span className="ml-1.5 text-muted-foreground/50">{cmd.description}</span>
                  </button>
                ))}
              </div>
            ))}
          </ScrollArea>
        </div>
      )}
    </div>
  )
}
