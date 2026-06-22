import { useState, useRef, useCallback, useEffect, useMemo } from 'react'
import { Play, PlayCircle, Trash2, Copy, Check, ChevronDown, Zap, Clock, Lightbulb } from 'lucide-react'
import { Button } from '@/components/ui/button'
import { ScrollArea } from '@/components/ui/scroll-area'
import { useRedisStore } from '@/stores/redisStore'
import { redisService } from '@/services/redisService'
import { mockCliCommands } from '@/mock/serverInfo'
import { mockKeys } from '@/mock/keys'
import { cn } from '@/lib/utils'

// Commands that expect a key name as first argument
const KEY_AWARE = new Set([
  'GET', 'SET', 'DEL', 'TYPE', 'TTL', 'EXISTS', 'EXPIRE', 'PERSIST',
  'HGET', 'HGETALL', 'HSET', 'HDEL', 'HLEN',
  'LPUSH', 'RPUSH', 'LPOP', 'RPOP', 'LLEN', 'LRANGE',
  'SADD', 'SREM', 'SMEMBERS', 'SCARD',
  'ZADD', 'ZREM', 'ZRANGE', 'ZCARD',
  'XADD', 'XREAD', 'XLEN',
  'RENAME',
])

interface QueryResult {
  id: number
  command: string
  result: string
  isError: boolean
  duration: number
  timestamp: string
}

const SNIPPETS = [
  { label: 'SET + GET', code: 'SET mykey "hello"\nGET mykey' },
  { label: 'Hash 操作', code: 'HSET user:1001 name "Alice"\nHGET user:1001 name\nHGETALL user:1001' },
  { label: 'List 操作', code: 'LPUSH queue:tasks "task1"\nLPUSH queue:tasks "task2"\nLRANGE queue:tasks 0 -1' },
  { label: 'Set 操作', code: 'SADD tags:redis "fast"\nSADD tags:redis "memory"\nSMEMBERS tags:redis' },
  { label: 'ZSet 操作', code: 'ZADD leaderboard 100 "player1"\nZADD leaderboard 200 "player2"\nZRANGE leaderboard 0 -1 WITHSCORES' },
  { label: '查询 INFO', code: 'INFO server\nINFO memory\nDBSIZE' },
  { label: 'Key 扫描', code: 'KEYS user:*\nKEYS product:*\nSCAN 0 MATCH cache:* COUNT 10' },
  { label: 'Pipeline 模拟', code: 'SET key1 "v1"\nSET key2 "v2"\nSET key3 "v3"\nGET key1\nGET key2\nGET key3\nDEL key1 key2 key3' },
]

export default function QueryEditor() {
  const activeConnectionId = useRedisStore((s) => s.activeConnectionId)
  const activeDb = useRedisStore((s) => s.activeDb)
  const connections = useRedisStore((s) => s.connections)

  const [code, setCode] = useState('')
  const [results, setResults] = useState<QueryResult[]>([])
  const [isRunning, setIsRunning] = useState(false)
  const [copiedId, setCopiedId] = useState<string | null>(null)
  const [showSnippets, setShowSnippets] = useState(false)
  const [selectedText, setSelectedText] = useState<string | null>(null)
  // Autocomplete
  const [acItems, setAcItems] = useState<{ text: string; desc: string; type: 'cmd' | 'key' }[]>([])
  const [acIndex, setAcIndex] = useState(0)
  const [acVisible, setAcVisible] = useState(false)
  const [acPos, setAcPos] = useState({ top: 0, left: 0 })
  const textareaRef = useRef<HTMLTextAreaElement>(null)
  const resultsRef = useRef<HTMLDivElement>(null)
  const lineNumbersRef = useRef<HTMLDivElement>(null)
  const idCounter = useRef(0)

  const activeConnection = connections.find((c) => c.id === activeConnectionId)
  const connLabel = activeConnection
    ? `${activeConnection.host}:${activeConnection.port}[${activeDb}]`
    : '未连接'

  // Sync scroll between textarea and line numbers
  const syncScroll = useCallback(() => {
    if (textareaRef.current && lineNumbersRef.current) {
      lineNumbersRef.current.scrollTop = textareaRef.current.scrollTop
    }
  }, [])

  // Auto-scroll results
  useEffect(() => {
    if (resultsRef.current) {
      resultsRef.current.scrollTop = resultsRef.current.scrollHeight
    }
  }, [results])

  // Track selected text on mouseup
  const handleSelection = useCallback(() => {
    const ta = textareaRef.current
    if (!ta) return
    const sel = ta.value.substring(ta.selectionStart, ta.selectionEnd)
    setSelectedText(sel.trim() || null)
  }, [])

  // Smart autocomplete trigger
  const triggerAutocomplete = useCallback(() => {
    const ta = textareaRef.current
    if (!ta) return
    const pos = ta.selectionStart
    const text = ta.value

    // Get current line text up to cursor
    const lineStart = text.lastIndexOf('\n', pos - 1) + 1
    const lineText = text.slice(lineStart, pos)
    const words = lineText.trim().split(/\s+/)
    const currentWord = words.length > 0 ? words[words.length - 1] : ''
    const firstWord = words.length > 0 ? words[0].toUpperCase() : ''

    let items: { text: string; desc: string; type: 'cmd' | 'key' }[] = []

    if (words.length >= 2 && KEY_AWARE.has(firstWord) && currentWord) {
      // User is typing a key name after a key-aware command
      items = mockKeys
        .filter((k) => k.name.includes(currentWord))
        .slice(0, 8)
        .map((k) => ({ text: k.name, desc: `${k.type} · ${k.ttl > 0 ? k.ttl + 's' : k.ttl === -1 ? '∞' : 'expired'}`, type: 'key' as const }))
    } else if (words.length <= 1 && currentWord) {
      // User is typing a command
      const upper = currentWord.toUpperCase()
      items = mockCliCommands
        .filter((c) => c.command.startsWith(upper) && c.command !== upper)
        .slice(0, 8)
        .map((c) => ({ text: c.command, desc: c.description, type: 'cmd' as const }))
    }

    if (items.length > 0) {
      // Calculate dropdown position from cursor
      const lineHeight = 22 // approximate px per line in the textarea
      const charWidth = 8.4 // approximate px per char for monospace
      const linesBeforeCursor = text.slice(0, pos).split('\n').length - 1
      const charsInCurrentLine = pos - lineStart
      setAcPos({ top: (linesBeforeCursor + 1) * lineHeight + 4, left: charsInCurrentLine * charWidth + 48 })
      setAcItems(items)
      setAcIndex(0)
      setAcVisible(true)
    } else {
      setAcVisible(false)
    }
  }, [])

  // Insert suggestion at cursor
  const insertSuggestion = useCallback((item: { text: string; type: 'cmd' | 'key' }) => {
    const ta = textareaRef.current
    if (!ta) return
    const pos = ta.selectionStart
    const text = ta.value
    const lineStart = text.lastIndexOf('\n', pos - 1) + 1
    const lineText = text.slice(lineStart, pos)
    const words = lineText.trim().split(/\s+/)
    const currentWord = words.length > 0 ? words[words.length - 1] : ''

    // Replace current word with suggestion
    const before = text.slice(0, pos - currentWord.length)
    const after = text.slice(pos)
    const replacement = item.type === 'cmd' ? item.text + ' ' : item.text
    const newText = before + replacement + after
    setCode(newText)
    setAcVisible(false)

    // Move cursor after inserted text
    setTimeout(() => {
      if (ta) {
        const newPos = pos - currentWord.length + replacement.length
        ta.selectionStart = ta.selectionEnd = newPos
        ta.focus()
      }
    }, 0)
  }, [])

  const runSelected = useCallback(async () => {
    if (!selectedText || isRunning) return
    setIsRunning(true)

    const commandsToRun = selectedText
      .split('\n')
      .map((l) => l.trim())
      .filter((l) => l.length > 0 && !l.startsWith('#') && !l.startsWith('//'))

    if (commandsToRun.length === 0) {
      setIsRunning(false)
      return
    }

    const newResults: QueryResult[] = []
    for (const cmd of commandsToRun) {
      const startTime = performance.now()
      try {
        const result = await redisService.executeCommand(cmd)
        const duration = Math.round(performance.now() - startTime)
        newResults.push({
          id: ++idCounter.current, command: cmd, result,
          isError: result.startsWith('(error)'), duration,
          timestamp: new Date().toISOString(),
        })
      } catch (err) {
        newResults.push({
          id: ++idCounter.current, command: cmd,
          result: `(error) ${err instanceof Error ? err.message : '执行失败'}`,
          isError: true, duration: Math.round(performance.now() - startTime),
          timestamp: new Date().toISOString(),
        })
      }
    }
    setResults(newResults)
    setIsRunning(false)
  }, [selectedText, isRunning])

  const runAll = useCallback(async () => {
    if (!code.trim() || isRunning) return
    setIsRunning(true)

    const commandsToRun = code.split('\n')
      .map((l) => l.trim())
      .filter((l) => l.length > 0 && !l.startsWith('#') && !l.startsWith('//'))

    if (commandsToRun.length === 0) {
      setIsRunning(false)
      return
    }

    const newResults: QueryResult[] = []
    for (const cmd of commandsToRun) {
      const startTime = performance.now()
      try {
        const result = await redisService.executeCommand(cmd)
        const duration = Math.round(performance.now() - startTime)
        newResults.push({
          id: ++idCounter.current, command: cmd, result,
          isError: result.startsWith('(error)'), duration,
          timestamp: new Date().toISOString(),
        })
      } catch (err) {
        newResults.push({
          id: ++idCounter.current, command: cmd,
          result: `(error) ${err instanceof Error ? err.message : '执行失败'}`,
          isError: true, duration: Math.round(performance.now() - startTime),
          timestamp: new Date().toISOString(),
        })
      }
    }
    setResults(newResults)
    setIsRunning(false)
  }, [code, isRunning, activeConnectionId])

  const handleKeyDown = useCallback(
    (e: React.KeyboardEvent<HTMLTextAreaElement>) => {
      // Autocomplete keyboard
      if (acVisible) {
        if (e.key === 'Escape') { e.preventDefault(); setAcVisible(false); return }
        if (e.key === 'ArrowDown') { e.preventDefault(); setAcIndex((i) => Math.min(i + 1, acItems.length - 1)); return }
        if (e.key === 'ArrowUp') { e.preventDefault(); setAcIndex((i) => Math.max(i - 1, 0)); return }
        if (e.key === 'Enter' || e.key === 'Tab') {
          e.preventDefault()
          if (acItems[acIndex]) insertSuggestion(acItems[acIndex])
          return
        }
      }

      // Ctrl+Space → trigger autocomplete
      if ((e.ctrlKey || e.metaKey) && e.key === ' ') {
        e.preventDefault()
        triggerAutocomplete()
        return
      }

      // Ctrl+Enter or Cmd+Enter to run all
      if ((e.ctrlKey || e.metaKey) && e.key === 'Enter') {
        e.preventDefault()
        runAll()
        return
      }
      // Tab for indentation
      if (e.key === 'Tab') {
        e.preventDefault()
        const ta = e.currentTarget
        const start = ta.selectionStart
        const end = ta.selectionEnd
        setCode(code.substring(0, start) + '  ' + code.substring(end))
        setTimeout(() => {
          ta.selectionStart = ta.selectionEnd = start + 2
        }, 0)
      }
    },
    [runAll, code, acVisible, acItems, acIndex, insertSuggestion, triggerAutocomplete],
  )

  const insertSnippet = (snippet: string) => {
    setCode(snippet)
    setShowSnippets(false)
    textareaRef.current?.focus()
  }

  const clearAll = () => {
    setCode('')
    setResults([])
    setSelectedText(null)
  }

  const copyResult = (text: string, id: string) => {
    navigator.clipboard.writeText(text)
    setCopiedId(id)
    setTimeout(() => setCopiedId(null), 2000)
  }

  const lineCount = code.split('\n').length
  const lines = Array.from({ length: Math.max(lineCount, 1) }, (_, i) => i + 1)

  return (
    <div className="flex h-full flex-col">
      {/* Toolbar */}
      <div className="flex items-center gap-2 border-b px-3 py-1.5">
        <span className="text-xs font-medium text-muted-foreground">{connLabel}</span>
        <div className="flex-1" />
        <div className="relative">
          <Button
            variant="ghost"
            size="sm"
            className="h-7 gap-1 text-xs"
            onClick={() => setShowSnippets(!showSnippets)}
          >
            <Zap className="h-3 w-3" />
            模板
            <ChevronDown className="h-3 w-3" />
          </Button>
          {showSnippets && (
            <div className="absolute right-0 top-full z-50 mt-1 w-64 rounded-md border bg-popover p-1 shadow-lg">
              {SNIPPETS.map((s) => (
                <button
                  key={s.label}
                  className="w-full rounded-sm px-3 py-2 text-left text-xs hover:bg-accent"
                  onClick={() => insertSnippet(s.code)}
                >
                  <span className="font-medium">{s.label}</span>
                  <code className="mt-0.5 block text-[11px] text-muted-foreground truncate">
                    {s.code.split('\n')[0]}...
                  </code>
                </button>
              ))}
            </div>
          )}
        </div>
        <Button variant="ghost" size="sm" className="h-7 gap-1 text-xs" onClick={clearAll}>
          <Trash2 className="h-3 w-3" />
          清空
        </Button>
        <Button
          size="sm"
          className="h-7 gap-1.5 text-xs"
          onClick={runSelected}
          disabled={isRunning || !selectedText}
        >
          {isRunning ? (
            <span className="animate-spin">⟳</span>
          ) : (
            <Play className="h-3 w-3" />
          )}
          {selectedText
            ? `运行选中 (${selectedText.split('\n').filter(l => l.trim()).length} 条)`
            : '选中命令后运行'}
        </Button>
        <Button
          variant="outline"
          size="sm"
          className="h-7 gap-1 text-xs"
          onClick={runAll}
          disabled={isRunning || !code.trim()}
        >
          全部运行 Ctrl+Enter
        </Button>
      </div>

      {/* Editor + Results split */}
      <div className="flex flex-1 flex-col overflow-hidden">
        {/* Editor area */}
        <div className="flex border-b" style={{ height: selectedText ? '55%' : '60%' }}>
          {/* Line numbers */}
          <div
            ref={lineNumbersRef}
            className="select-none overflow-hidden bg-muted/20 px-2 py-3 text-right font-mono text-xs text-muted-foreground"
            style={{ width: 48, minWidth: 48 }}
          >
            {lines.map((n) => (
              <div key={n} className="leading-[1.7]">
                {n}
              </div>
            ))}
          </div>

          {/* Code textarea */}
          <div className="relative flex-1">
          <textarea
            ref={textareaRef}
            value={code}
            onChange={(e) => {
              setCode(e.target.value)
              setSelectedText(null)
              triggerAutocomplete()
            }}
            onKeyDown={handleKeyDown}
            onScroll={syncScroll}
            onMouseUp={handleSelection}
            onKeyUp={handleSelection}
            className="w-full h-full resize-none bg-transparent px-3 py-3 font-mono text-sm outline-none placeholder:text-muted-foreground/50 leading-[1.7]"
            placeholder={`# Redis 查询编辑器 · Ctrl+Space 代码提示 · Ctrl+Enter 运行全部
# 每行一条命令，选中后点击运行只执行选中命令
# 以 # 或 // 开头的行会被忽略

PING
SET mykey "hello redis"
GET mykey
DBSIZE
INFO stats`}
            spellCheck={false}
          />

          {/* Autocomplete dropdown */}
          {acVisible && acItems.length > 0 && (
            <div
              className="absolute z-50 max-h-48 w-72 overflow-auto rounded-lg border bg-popover p-1 shadow-lg"
              style={{ top: acPos.top, left: acPos.left }}
            >
              {acItems.map((item, i) => (
                <button
                  key={item.text}
                  onClick={() => insertSuggestion(item)}
                  onMouseEnter={() => setAcIndex(i)}
                  className={cn(
                    'flex w-full items-center gap-2 rounded-sm px-2 py-1.5 text-left text-xs transition-colors',
                    i === acIndex && 'bg-accent text-accent-foreground',
                  )}
                >
                  <span className={cn('shrink-0 rounded px-1 py-0.5 text-[10px] font-bold', item.type === 'cmd' ? 'bg-blue-100 text-blue-700' : 'bg-emerald-100 text-emerald-700')}>
                    {item.type === 'cmd' ? 'CMD' : 'KEY'}
                  </span>
                  <span className="flex-1 truncate font-mono">{item.text}</span>
                  <span className="shrink-0 text-[10px] text-muted-foreground">{item.desc}</span>
                </button>
              ))}
            </div>
          )}
          </div>
        </div>

        {/* Results area */}
        <div className="flex flex-1 flex-col bg-[#1e1e20]">
          <div className="flex items-center gap-2 border-b border-white/5 px-3 py-1.5">
            <PlayCircle className="h-3.5 w-3.5 text-white/30" />
            <span className="text-xs text-white/50 font-medium">结果</span>
            <span className="rounded bg-white/10 px-1.5 py-0.5 text-[10px] text-white/30">{results.length}</span>
            {results.length > 0 && (
              <button className="ml-auto text-[10px] text-white/30 hover:text-white/60" onClick={() => setResults([])}>清除结果</button>
            )}
          </div>
          <ScrollArea ref={resultsRef} className="flex-1">
            {results.length === 0 ? (
              <div className="flex h-full items-center justify-center"><span className="text-xs text-white/20">Ctrl+Enter 运行命令，结果将显示在这里</span></div>
            ) : (
              <div className="divide-y divide-white/5">
                {results.map((r) => (
                  <div key={r.id} className="group px-3 py-2">
                    <div className="mb-1 flex items-center gap-2">
                      <span className="shrink-0 text-xs text-blue-400 font-mono">&gt;</span>
                      <code className="flex-1 text-xs text-white/80 font-mono">{r.command}</code>
                      <div className="flex items-center gap-1 opacity-0 group-hover:opacity-100 transition-opacity">
                        <span className="text-[10px] text-white/30 flex items-center gap-1"><Clock className="h-2.5 w-2.5" />{r.duration}ms</span>
                        <button className="text-white/30 hover:text-white/60" onClick={() => copyResult(r.result, `res-${r.id}`)}>
                          {copiedId === `res-${r.id}` ? <Check className="h-3 w-3 text-emerald-400" /> : <Copy className="h-3 w-3" />}
                        </button>
                      </div>
                    </div>
                    <pre className={cn('ml-4 whitespace-pre-wrap break-all font-mono text-xs border-l-2 pl-3 py-0.5', r.isError ? 'text-red-400 border-red-800' : 'text-emerald-400 border-emerald-800')}>{r.result}</pre>
                  </div>
                ))}
              </div>
            )}
          </ScrollArea>
        </div>
      </div>
    </div>
  )
}
