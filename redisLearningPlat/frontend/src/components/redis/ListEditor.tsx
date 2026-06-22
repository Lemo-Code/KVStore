import { useState } from 'react'
import { cn } from '@/lib/utils'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { ScrollArea } from '@/components/ui/scroll-area'
import { List as ListIcon, Trash2, ArrowLeftToLine, ArrowRightToLine } from 'lucide-react'
import type { ListValue } from '@/types/redis'

interface ListEditorProps {
  value: ListValue
  onPush: (value: string, direction: 'left' | 'right') => Promise<void>
  onRemove: (index: number) => Promise<void>
}

export default function ListEditor({ value, onPush, onRemove }: ListEditorProps) {
  const [pushValue, setPushValue] = useState('')

  const handlePush = async (direction: 'left' | 'right') => {
    if (!pushValue.trim()) return
    await onPush(pushValue.trim(), direction)
    setPushValue('')
  }

  const handleKeyDown = (e: React.KeyboardEvent, direction: 'left' | 'right') => {
    if (e.key === 'Enter') {
      e.preventDefault()
      handlePush(direction)
    }
  }

  return (
    <div className="flex flex-col h-full">
      {/* Toolbar */}
      <div className="flex items-center gap-2 px-3 py-2 border-b bg-muted/30">
        <ListIcon className="h-4 w-4 text-muted-foreground" />
        <span className="text-xs text-muted-foreground">
          长度: <span className="font-mono font-medium text-foreground">{value.length}</span>
        </span>
      </div>

      {/* Push form */}
      <div className="flex items-center gap-2 px-3 py-2 border-b bg-muted/10">
        <Input
          value={pushValue}
          onChange={(e) => setPushValue(e.target.value)}
          placeholder="输入值..."
          className="h-7 text-xs flex-1"
          onKeyDown={(e) => handleKeyDown(e, 'left')}
        />
        <Button
          variant="outline"
          size="sm"
          className="h-7 text-xs flex-shrink-0"
          onClick={() => handlePush('left')}
          disabled={!pushValue.trim()}
        >
          <ArrowLeftToLine className="h-3 w-3 mr-1" />
          LPUSH
        </Button>
        <Button
          variant="outline"
          size="sm"
          className="h-7 text-xs flex-shrink-0"
          onClick={() => handlePush('right')}
          disabled={!pushValue.trim()}
        >
          RPUSH
          <ArrowRightToLine className="h-3 w-3 ml-1" />
        </Button>
      </div>

      {/* List items */}
      <ScrollArea className="flex-1">
        <div className="divide-y">
          {value.values.map((item) => (
            <div
              key={item.index}
              className="flex items-center gap-3 px-3 py-2 hover:bg-muted/30 transition-colors"
            >
              <span className="text-xs text-muted-foreground font-mono tabular-nums min-w-[3rem]">
                [{item.index}]
              </span>
              <span className="text-xs font-mono flex-1 truncate">{item.value}</span>
              <Button
                variant="ghost"
                size="icon"
                className="h-6 w-6 text-red-500 hover:text-red-700 hover:bg-red-50 flex-shrink-0"
                onClick={() => {
                  if (confirm(`确认删除索引 ${item.index} 处的元素 ?`)) {
                    onRemove(item.index)
                  }
                }}
              >
                <Trash2 className="h-3 w-3" />
              </Button>
            </div>
          ))}
          {value.values.length === 0 && (
            <div className="flex items-center justify-center py-8 text-xs text-muted-foreground">
              列表为空
            </div>
          )}
        </div>
      </ScrollArea>
    </div>
  )
}
