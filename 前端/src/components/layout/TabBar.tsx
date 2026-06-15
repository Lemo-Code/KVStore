import { useAppStore } from '@/stores/appStore'
import { cn } from '@/lib/utils'
import { Terminal, X } from 'lucide-react'
import { KeyTypeIcon } from '@/components/ui/KeyTypeIcon'
import type { RedisKeyType } from '@/types'

export function TabBar() {
  const { tabs, activeTab, setActiveTab, closeTab, addConsoleTab } = useAppStore()

  return (
    <div className="flex h-9 shrink-0 items-end border-b border-border-subtle bg-surface-1 px-1 gap-px overflow-x-auto">
      {tabs.map((tab) => {
        const isActive = activeTab === tab.id
        return (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className={cn(
              'group flex items-center gap-1.5 rounded-t-md px-3 py-1.5 text-xs transition-all shrink-0',
              isActive
                ? 'bg-surface-2 text-text-primary border-t border-x border-border-subtle -mb-px'
                : 'text-text-muted hover:text-text-secondary hover:bg-surface-2/50',
            )}
          >
            {tab.type === 'console' ? (
              <Terminal size={12} className="text-accent-teal" />
            ) : (
              <KeyTypeIcon type={(tab.keyType ?? 'hash') as RedisKeyType} size={12} />
            )}
            <span className="max-w-[140px] truncate">{tab.title}</span>
            {tab.dirty && <span className="h-1.5 w-1.5 rounded-full bg-accent-amber" />}
            <X
              size={11}
              onClick={(e) => { e.stopPropagation(); closeTab(tab.id) }}
              className="opacity-0 group-hover:opacity-60 hover:!opacity-100 transition-opacity ml-0.5"
            />
          </button>
        )
      })}
      <button
        onClick={addConsoleTab}
        className="flex h-7 w-7 items-center justify-center rounded text-text-muted hover:text-text-secondary hover:bg-surface-3 text-lg leading-none shrink-0 mb-0.5"
        title="新建查询标签"
      >
        +
      </button>
    </div>
  )
}
