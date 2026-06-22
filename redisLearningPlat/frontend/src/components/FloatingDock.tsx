import { useState } from 'react'
import { MessageSquare, Bot } from 'lucide-react'
import { useRedisStore } from '@/stores/redisStore'
import FloatingChat from '@/components/chat/FloatingChat'
import FloatingAI from '@/components/ai/FloatingAI'

type Panel = 'chat' | 'ai' | null

export default function FloatingDock() {
  const [active, setActive] = useState<Panel>(null)
  const selectedKey = useRedisStore((s) => s.selectedKey)

  return (
    <>
      {active === 'chat' && <FloatingChat onClose={() => setActive(null)} />}
      {active === 'ai' && <FloatingAI onClose={() => setActive(null)} />}

      {!active && (
        <div className="fixed bottom-6 right-6 z-50 flex flex-col gap-1.5 rounded-2xl border bg-white/80 p-1.5 shadow-[0_4px_24px_rgba(0,0,0,0.08)] backdrop-blur-xl dark:bg-zinc-900/80 dark:shadow-[0_4px_24px_rgba(0,0,0,0.4)]">
          <button onClick={() => setActive('chat')}
            className="group relative flex h-10 w-10 items-center justify-center rounded-xl text-zinc-400 transition-all hover:bg-emerald-50 hover:text-emerald-600 dark:hover:bg-emerald-950 dark:hover:text-emerald-400">
            <MessageSquare className="h-5 w-5" />
            <span className="absolute -right-1 -top-1 flex h-4 min-w-4 items-center justify-center rounded-full bg-red-500 text-[9px] font-bold text-white ring-2 ring-white dark:ring-zinc-900">3</span>
            <span className="pointer-events-none absolute right-14 top-1/2 -translate-y-1/2 whitespace-nowrap rounded-lg bg-zinc-800 px-2.5 py-1.5 text-xs text-white opacity-0 transition-all group-hover:opacity-100 dark:bg-white dark:text-zinc-800">学习交流</span>
          </button>
          <div className="mx-auto h-px w-6 bg-zinc-200 dark:bg-zinc-700" />
          <button onClick={() => setActive('ai')}
            className="group relative flex h-10 w-10 items-center justify-center rounded-xl text-zinc-400 transition-all hover:bg-violet-50 hover:text-violet-600 dark:hover:bg-violet-950 dark:hover:text-violet-400">
            <Bot className="h-5 w-5" />
            {selectedKey && <span className="absolute -right-0.5 -top-0.5 h-2.5 w-2.5 rounded-full bg-violet-500 ring-2 ring-white dark:ring-zinc-900" />}
            <span className="pointer-events-none absolute right-14 top-1/2 -translate-y-1/2 whitespace-nowrap rounded-lg bg-zinc-800 px-2.5 py-1.5 text-xs text-white opacity-0 transition-all group-hover:opacity-100 dark:bg-white dark:text-zinc-800">
              AI Agent{selectedKey ? ` · ${selectedKey.split(':').pop()}` : ''}
            </span>
          </button>
        </div>
      )}

      {active && (
        <button onClick={() => setActive(null)}
          className="fixed bottom-6 right-6 z-50 flex h-10 w-10 items-center justify-center rounded-2xl border bg-white/80 shadow-lg backdrop-blur-xl transition-all hover:bg-zinc-100 dark:bg-zinc-900/80 dark:hover:bg-zinc-800">
          <span className="text-lg leading-none text-zinc-400">×</span>
        </button>
      )}
    </>
  )
}
