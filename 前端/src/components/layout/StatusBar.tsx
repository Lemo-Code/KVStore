import { mockConnection } from '@/stores/appStore'
import { useAppStore } from '@/stores/appStore'
import {
  Activity,
  ChevronUp,
  Cpu,
  HardDrive,
  Users,
  Zap,
} from 'lucide-react'

export function StatusBar() {
  const { bottomPanelOpen, toggleBottomPanel, activeDb, selectedKey } = useAppStore()
  const conn = mockConnection

  return (
    <footer className="flex h-6 shrink-0 items-center justify-between border-t border-border-subtle bg-surface-1 px-3 text-[10px] text-text-muted">
      <div className="flex items-center gap-3">
        <button
          onClick={toggleBottomPanel}
          className="flex items-center gap-1 hover:text-text-secondary transition-colors"
        >
          <ChevronUp
            size={11}
            className={`transition-transform ${bottomPanelOpen ? '' : 'rotate-180'}`}
          />
          监控面板
        </button>
        <span className="flex items-center gap-1">
          <Activity size={10} className="text-success" />
          已连接
        </span>
        <span className="font-mono">db{activeDb}</span>
        {selectedKey && (
          <span className="font-mono truncate max-w-[200px]">
            选中: {selectedKey.name}
          </span>
        )}
      </div>

      <div className="flex items-center gap-4">
        <span className="flex items-center gap-1">
          <Cpu size={10} />
          CPU 2.1%
        </span>
        <span className="flex items-center gap-1">
          <HardDrive size={10} />
          {conn.memory}
        </span>
        <span className="flex items-center gap-1">
          <Zap size={10} />
          12.4k ops/s
        </span>
        <span className="flex items-center gap-1">
          <Users size={10} />
          4 协作者
        </span>
        <span className="text-text-muted/60">Redis Lab Studio v1.0</span>
      </div>
    </footer>
  )
}
