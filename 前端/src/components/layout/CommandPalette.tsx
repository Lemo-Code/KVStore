import { cn } from '@/lib/utils'
import { mockKeys, mockLearningModules } from '@/stores/appStore'
import { mockExercises, usePlatformStore } from '@/stores/platformStore'
import { AnimatePresence, motion } from 'framer-motion'
import {
  ArrowRight,
  BookOpen,
  Bot,
  Code2,
  Database,
  MessageCircle,
  Search,
  Settings,
  User,
  Zap,
} from 'lucide-react'
import { useEffect, useMemo, useState } from 'react'
import { useNavigate } from 'react-router-dom'

const staticPages = [
  { id: 'dashboard', label: '仪表盘', path: '/dashboard', icon: Zap, category: '页面' },
  { id: 'workspace', label: 'Redis 工作台', path: '/workspace', icon: Database, category: '页面' },
  { id: 'connections', label: '连接管理', path: '/connections', icon: Database, category: '页面' },
  { id: 'chat', label: '协作通讯', path: '/chat', icon: MessageCircle, category: '页面' },
  { id: 'ai', label: 'AI 导师', path: '/ai', icon: Bot, category: '页面' },
  { id: 'learning', label: '学习中心', path: '/learning', icon: BookOpen, category: '页面' },
  { id: 'exercises', label: '练习场', path: '/exercises', icon: Code2, category: '页面' },
  { id: 'help', label: '帮助中心', path: '/help', icon: BookOpen, category: '页面' },
  { id: 'profile', label: '个人资料', path: '/profile', icon: User, category: '页面' },
  { id: 'settings', label: '设置', path: '/settings', icon: Settings, category: '页面' },
]

export function CommandPalette() {
  const open = usePlatformStore((s) => s.commandPaletteOpen)
  const setOpen = usePlatformStore((s) => s.setCommandPaletteOpen)
  const [query, setQuery] = useState('')
  const [selected, setSelected] = useState(0)
  const navigate = useNavigate()

  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      if ((e.metaKey || e.ctrlKey) && e.key === 'k') {
        e.preventDefault()
        setOpen(!open)
      }
    }
    document.addEventListener('keydown', handler)
    return () => document.removeEventListener('keydown', handler)
  }, [open, setOpen])

  useEffect(() => {
    if (!open) {
      setQuery('')
      setSelected(0)
    }
  }, [open])

  const results = useMemo(() => {
    const q = query.toLowerCase().trim()
    const items: { id: string; label: string; sub?: string; path: string; icon: typeof Search; category: string }[] = []

    staticPages.forEach((p) => {
      if (!q || p.label.toLowerCase().includes(q)) items.push({ ...p, sub: p.category })
    })

    mockKeys.forEach((k) => {
      if (!q || k.name.toLowerCase().includes(q)) {
        items.push({
          id: `key-${k.name}`,
          label: k.name,
          sub: `Redis 键 · ${k.type}`,
          path: '/workspace',
          icon: Database,
          category: 'Redis 键',
        })
      }
    })

    mockLearningModules.forEach((m) => {
      if (!q || m.title.toLowerCase().includes(q)) {
        items.push({
          id: `course-${m.id}`,
          label: m.title,
          sub: m.description,
          path: `/learning/${m.id}`,
          icon: BookOpen,
          category: '课程',
        })
      }
    })

    mockExercises.forEach((e) => {
      if (!q || e.title.toLowerCase().includes(q)) {
        items.push({
          id: `ex-${e.id}`,
          label: e.title,
          sub: e.tags.join(' · '),
          path: `/exercises/${e.id}`,
          icon: Code2,
          category: '练习',
        })
      }
    })

    return items.slice(0, 12)
  }, [query])

  const handleSelect = (index: number) => {
    const item = results[index]
    if (!item) return
    navigate(item.path)
    setOpen(false)
  }

  const onKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === 'ArrowDown') {
      e.preventDefault()
      setSelected((s) => Math.min(s + 1, results.length - 1))
    } else if (e.key === 'ArrowUp') {
      e.preventDefault()
      setSelected((s) => Math.max(s - 1, 0))
    } else if (e.key === 'Enter') {
      e.preventDefault()
      handleSelect(selected)
    }
  }

  return (
    <AnimatePresence>
      {open && (
        <motion.div
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          exit={{ opacity: 0 }}
          className="fixed inset-0 z-[200] flex items-start justify-center pt-[15vh] bg-black/50 backdrop-blur-sm px-4"
          onClick={() => setOpen(false)}
        >
          <motion.div
            initial={{ scale: 0.96, opacity: 0, y: -10 }}
            animate={{ scale: 1, opacity: 1, y: 0 }}
            exit={{ scale: 0.96, opacity: 0, y: -10 }}
            className="w-full max-w-xl rounded-2xl border border-border-subtle bg-surface-1 shadow-2xl overflow-hidden"
            onClick={(e) => e.stopPropagation()}
          >
            <div className="flex items-center gap-3 px-4 border-b border-border-subtle">
              <Search size={18} className="text-text-muted shrink-0" />
              <input
                autoFocus
                value={query}
                onChange={(e) => { setQuery(e.target.value); setSelected(0) }}
                onKeyDown={onKeyDown}
                placeholder="搜索页面、Redis 键、课程、练习..."
                className="flex-1 py-4 bg-transparent text-sm focus:outline-none placeholder:text-text-muted"
              />
              <kbd className="hidden sm:inline text-[10px] text-text-muted bg-surface-3 px-1.5 py-0.5 rounded border border-border-subtle">
                ESC
              </kbd>
            </div>

            <div className="max-h-80 overflow-y-auto py-2">
              {results.length === 0 ? (
                <div className="py-8 text-center text-sm text-text-muted">无匹配结果</div>
              ) : (
                results.map((item, i) => {
                  const Icon = item.icon
                  return (
                    <button
                      key={item.id}
                      onClick={() => handleSelect(i)}
                      onMouseEnter={() => setSelected(i)}
                      className={cn(
                        'w-full flex items-center gap-3 px-4 py-2.5 text-left transition-colors',
                        selected === i ? 'bg-accent-red/10' : 'hover:bg-surface-2',
                      )}
                    >
                      <div className={cn(
                        'flex h-8 w-8 items-center justify-center rounded-lg',
                        selected === i ? 'bg-accent-red/15' : 'bg-surface-3',
                      )}>
                        <Icon size={14} className={selected === i ? 'text-accent-red' : 'text-text-muted'} />
                      </div>
                      <div className="flex-1 min-w-0">
                        <div className="text-sm font-medium truncate">{item.label}</div>
                        {item.sub && <div className="text-[11px] text-text-muted truncate">{item.sub}</div>}
                      </div>
                      <span className="text-[9px] text-text-muted uppercase">{item.category}</span>
                      {selected === i && <ArrowRight size={14} className="text-accent-red" />}
                    </button>
                  )
                })
              )}
            </div>

            <div className="flex items-center gap-4 px-4 py-2 border-t border-border-subtle text-[10px] text-text-muted">
              <span><kbd className="bg-surface-3 px-1 rounded">↑↓</kbd> 导航</span>
              <span><kbd className="bg-surface-3 px-1 rounded">↵</kbd> 打开</span>
              <span><kbd className="bg-surface-3 px-1 rounded">⌘K</kbd> 切换</span>
            </div>
          </motion.div>
        </motion.div>
      )}
    </AnimatePresence>
  )
}
