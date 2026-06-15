import { Button } from '@/components/ui/Button'
import { useAppStore } from '@/stores/appStore'
import { useSettingsStore } from '@/stores/settingsStore'
import Editor from '@monaco-editor/react'
import { useCallback, useEffect, useState } from 'react'
import {
  Clock,
  Copy,
  Download,
  History,
  Play,
  Trash2,
  Zap,
} from 'lucide-react'

const mockOutputs: Record<string, string> = {
  HGETALL: `1) "id"\n2) "1001"\n3) "username"\n4) "redis_learner"\n5) "email"\n6) "learner@redis.lab"`,
  ZREVRANGE: `1) "player_alpha"\n2) "9850"\n3) "player_beta"\n4) "8720"\n5) "player_gamma"\n6) "7650"`,
  SCAN: `1) "0"\n2) 1) "user:1001:profile"\n   2) "session:abc123"\n   3) "config:app"`,
  KEYS: `(error) ERR 生产环境不建议使用 KEYS，请改用 SCAN`,
  DEFAULT: `(integer) 1`,
}

const defaultCode = `# Redis 命令控制台
# 输入命令后按 Ctrl+Enter 或点击运行

HGETALL user:1001:profile

ZREVRANGE leaderboard:game 0 4 WITHSCORES

SCAN 0 MATCH user:* COUNT 10`

export function CommandConsole() {
  const queryHistory = useAppStore((s) => s.queryHistory)
  const addQueryHistory = useAppStore((s) => s.addQueryHistory)
  const editorFontSize = useSettingsStore((s) => s.editorFontSize)

  const [code, setCode] = useState(defaultCode)
  const [output, setOutput] = useState('')
  const [duration, setDuration] = useState(0)
  const [success, setSuccess] = useState(true)
  const [running, setRunning] = useState(false)

  const runCommand = useCallback(() => {
    const lines = code.split('\n').map((l) => l.trim()).filter((l) => l && !l.startsWith('#'))
    const cmd = lines[lines.length - 1] ?? ''
    if (!cmd) return

    setRunning(true)
    const start = performance.now()

    setTimeout(() => {
      const upper = cmd.toUpperCase()
      let result = mockOutputs.DEFAULT
      let ok = true
      if (upper.startsWith('HGETALL')) result = mockOutputs.HGETALL
      else if (upper.startsWith('ZREVRANGE')) result = mockOutputs.ZREVRANGE
      else if (upper.startsWith('SCAN')) result = mockOutputs.SCAN
      else if (upper.startsWith('KEYS')) { result = mockOutputs.KEYS; ok = false }

      const ms = Math.round((performance.now() - start) * 10) / 10 + Math.random() * 2
      setOutput(result)
      setDuration(ms)
      setSuccess(ok)
      setRunning(false)

      const now = new Date()
      addQueryHistory({
        id: `q-${Date.now()}`,
        command: cmd,
        result: ok ? 'OK' : 'ERR',
        duration: ms,
        timestamp: now.toLocaleTimeString('zh-CN', { hour12: false }),
        success: ok,
      })
    }, 300 + Math.random() * 400)
  }, [code, addQueryHistory])

  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      if ((e.metaKey || e.ctrlKey) && e.key === 'Enter') {
        e.preventDefault()
        runCommand()
      }
    }
    document.addEventListener('keydown', handler)
    return () => document.removeEventListener('keydown', handler)
  }, [runCommand])

  return (
    <div className="flex h-full flex-col">
      <div className="flex items-center justify-between border-b border-border-subtle px-3 py-1.5 bg-surface-2/50">
        <div className="flex items-center gap-1">
          <Button variant="accent" size="sm" onClick={runCommand} disabled={running}>
            <Play size={13} className={running ? 'animate-pulse' : ''} />
            运行 (Ctrl+Enter)
          </Button>
          <Button variant="ghost" size="sm" onClick={() => setCode(code.split('\n').map((l) => l.trim()).join('\n'))}>
            <Zap size={13} />
            格式化
          </Button>
          <Button variant="ghost" size="sm" onClick={() => navigator.clipboard.writeText(code)}>
            <Copy size={13} />
            复制
          </Button>
        </div>
        <div className="flex items-center gap-2 text-[10px] text-text-muted">
          <Clock size={11} />
          <span className="font-mono">{duration > 0 ? `${duration.toFixed(1)}ms` : '—'}</span>
          {duration > 0 && (
            <span className={success ? 'text-success' : 'text-danger'}>
              ● {success ? '成功' : '失败'}
            </span>
          )}
        </div>
      </div>

      <div className="flex flex-1 min-h-0">
        <div className="flex-1 min-w-0 border-r border-border-subtle">
          <Editor
            height="100%"
            defaultLanguage="shell"
            value={code}
            onChange={(v) => setCode(v ?? '')}
            theme="vs-dark"
            options={{
              fontFamily: 'IBM Plex Mono',
              fontSize: editorFontSize,
              lineHeight: 20,
              minimap: { enabled: false },
              scrollBeyondLastLine: false,
              padding: { top: 12 },
              renderLineHighlight: 'line',
              lineNumbers: 'on',
              folding: true,
              wordWrap: 'on',
              automaticLayout: true,
            }}
          />
        </div>

        <div className="w-52 shrink-0 flex flex-col bg-surface-1">
          <div className="flex items-center gap-1.5 border-b border-border-subtle px-3 py-2">
            <History size={12} className="text-text-muted" />
            <span className="text-[10px] font-semibold text-text-muted uppercase tracking-wider">查询历史</span>
          </div>
          <div className="flex-1 overflow-y-auto">
            {queryHistory.map((item) => (
              <button
                key={item.id}
                onClick={() => setCode(item.command)}
                className="w-full border-b border-border-subtle/50 px-3 py-2 text-left hover:bg-surface-2 transition-colors"
              >
                <div className="font-mono text-[10px] text-text-primary truncate">{item.command}</div>
                <div className="mt-1 flex items-center justify-between">
                  <span className={`text-[9px] ${item.success ? 'text-success' : 'text-danger'}`}>{item.result}</span>
                  <span className="text-[9px] text-text-muted font-mono">{item.duration.toFixed(1)}ms</span>
                </div>
              </button>
            ))}
          </div>
        </div>
      </div>

      <div className="h-36 shrink-0 border-t border-border-subtle flex flex-col">
        <div className="flex items-center justify-between px-3 py-1 bg-surface-2/50 border-b border-border-subtle">
          <span className="text-[10px] font-semibold text-text-muted uppercase tracking-wider">输出结果</span>
          <div className="flex gap-0.5">
            <Button variant="ghost" size="icon" className="h-5 w-5" onClick={() => {
              const blob = new Blob([output], { type: 'text/plain' })
              const a = document.createElement('a')
              a.href = URL.createObjectURL(blob)
              a.download = 'redis-output.txt'
              a.click()
            }}>
              <Download size={11} />
            </Button>
            <Button variant="ghost" size="icon" className="h-5 w-5" onClick={() => { setOutput(''); setDuration(0) }}>
              <Trash2 size={11} />
            </Button>
          </div>
        </div>
        <pre className="flex-1 overflow-auto p-3 font-mono text-xs text-accent-teal leading-relaxed">
          {output || <span className="text-text-muted">执行命令后结果将显示在这里...</span>}
        </pre>
      </div>
    </div>
  )
}
