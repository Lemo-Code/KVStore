import { useEffect, useRef } from 'react'
import { cn } from '@/lib/utils'
import { Eye, Pencil, Trash2, Copy, Clock } from 'lucide-react'

interface KeyContextMenuProps {
  x: number
  y: number
  keyName: string
  onClose: () => void
  onOpen: (keyName: string) => void
  onRename: (keyName: string, newName?: string) => void
  onDelete: (keyName: string) => void
  onCopy: (keyName: string) => void
  onSetTTL: (keyName: string) => void
}

interface MenuItem {
  label: string
  icon: React.ReactNode
  action: () => void
  destructive?: boolean
}

export default function KeyContextMenu({
  x,
  y,
  keyName,
  onClose,
  onOpen,
  onRename,
  onDelete,
  onCopy,
  onSetTTL,
}: KeyContextMenuProps) {
  const ref = useRef<HTMLDivElement>(null)

  useEffect(() => {
    const handleClickOutside = (e: MouseEvent) => {
      if (ref.current && !ref.current.contains(e.target as Node)) {
        onClose()
      }
    }

    const handleEscape = (e: KeyboardEvent) => {
      if (e.key === 'Escape') {
        onClose()
      }
    }

    document.addEventListener('mousedown', handleClickOutside)
    document.addEventListener('keydown', handleEscape)
    return () => {
      document.removeEventListener('mousedown', handleClickOutside)
      document.removeEventListener('keydown', handleEscape)
    }
  }, [onClose])

  // Adjust position to avoid overflow
  useEffect(() => {
    if (ref.current) {
      const rect = ref.current.getBoundingClientRect()
      const currentX = parseFloat(ref.current.style.left || '0')
      const currentY = parseFloat(ref.current.style.top || '0')

      if (rect.right > window.innerWidth) {
        ref.current.style.left = `${currentX - rect.width}px`
      }
      if (rect.bottom > window.innerHeight) {
        ref.current.style.top = `${currentY - rect.height}px`
      }
    }
  }, [x, y])

  const items: MenuItem[] = [
    {
      label: '打开',
      icon: <Eye className="h-4 w-4" />,
      action: () => onOpen(keyName),
    },
    {
      label: '重命名',
      icon: <Pencil className="h-4 w-4" />,
      action: () => {
        const newName = prompt('输入新的 Key 名称:', keyName)
        if (newName && newName.trim()) {
          onRename(keyName, newName.trim())
        }
      },
    },
    {
      label: '删除',
      icon: <Trash2 className="h-4 w-4" />,
      action: () => {
        if (confirm(`确认删除 Key "${keyName}" ?`)) {
          onDelete(keyName)
        }
      },
      destructive: true,
    },
    {
      label: '复制 Key 名',
      icon: <Copy className="h-4 w-4" />,
      action: () => onCopy(keyName),
    },
    {
      label: '设置 TTL',
      icon: <Clock className="h-4 w-4" />,
      action: () => {
        const ttl = prompt('输入 TTL（秒）:')
        if (ttl !== null) {
          onSetTTL(keyName)
        }
      },
    },
  ]

  return (
    <div
      ref={ref}
      className="fixed z-50 min-w-[180px] rounded-lg border bg-popover p-1 shadow-md animate-in fade-in-0 zoom-in-95"
      style={{ left: `${x}px`, top: `${y}px` }}
    >
      <div className="px-2 py-1.5 text-xs font-medium text-muted-foreground border-b mb-1 truncate max-w-[220px]">
        {keyName}
      </div>
      {items.map((item) => (
        <button
          key={item.label}
          className={cn(
            'flex w-full items-center gap-2 rounded-md px-2 py-1.5 text-sm transition-colors',
            item.destructive
              ? 'text-red-600 hover:bg-red-50'
              : 'hover:bg-accent hover:text-accent-foreground'
          )}
          onClick={item.action}
        >
          {item.icon}
          <span>{item.label}</span>
        </button>
      ))}
    </div>
  )
}
