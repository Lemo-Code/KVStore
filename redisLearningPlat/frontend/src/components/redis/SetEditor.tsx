import { useState, useMemo } from 'react'
import { cn } from '@/lib/utils'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { ScrollArea } from '@/components/ui/scroll-area'
import {
  Dialog,
  DialogContent,
  DialogHeader,
  DialogTitle,
} from '@/components/ui/dialog'
import { Braces, Trash2, Plus, GitMerge, ArrowUpFromLine, Minus } from 'lucide-react'
import type { SetValue } from '@/types/redis'

interface SetEditorProps {
  value: SetValue
  onAdd: (member: string) => Promise<void>
  onRemove: (member: string) => Promise<void>
}

export default function SetEditor({ value, onAdd, onRemove }: SetEditorProps) {
  const [newMember, setNewMember] = useState('')
  const [setOperation, setSetOperation] = useState<'inter' | 'union' | 'diff' | null>(null)
  const [secondKey, setSecondKey] = useState('')
  const [operationResult, setOperationResult] = useState<string[] | null>(null)

  const handleAdd = async () => {
    if (!newMember.trim()) return
    await onAdd(newMember.trim())
    setNewMember('')
  }

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter') {
      e.preventDefault()
      handleAdd()
    }
  }

  // Simulate set operations against mock data
  const handleSetOperation = () => {
    if (!secondKey.trim()) return

    let result: string[] = []
    const secondMembers: string[] = []

    // Simulate getting the second set from mock data
    if (secondKey === 'rt:active_users') {
      secondMembers.push(...['user_1001', 'user_1002', 'user_1005', 'user_1010', 'user_1023', 'user_1042', 'user_1056', 'user_1089'])
    } else if (secondKey === 'order:pending') {
      secondMembers.push(...['ORD-2024004', 'ORD-2024005', 'ORD-2024006'])
    } else if (secondKey === 'product:categories') {
      secondMembers.push(...['books', 'electronics', 'clothing', 'food', 'sports', 'music', 'software', 'gaming'])
    }

    switch (setOperation) {
      case 'inter':
        result = value.members.filter((m) => secondMembers.includes(m))
        break
      case 'union':
        result = [...new Set([...value.members, ...secondMembers])]
        break
      case 'diff':
        result = value.members.filter((m) => !secondMembers.includes(m))
        break
    }

    setOperationResult(result)
  }

  const closeDialog = () => {
    setSetOperation(null)
    setSecondKey('')
    setOperationResult(null)
  }

  return (
    <div className="flex flex-col h-full">
      {/* Toolbar */}
      <div className="flex items-center gap-2 px-3 py-2 border-b bg-muted/30">
        <Braces className="h-4 w-4 text-muted-foreground" />
        <span className="text-xs text-muted-foreground">
          成员: <span className="font-mono font-medium text-foreground">{value.length}</span>
        </span>
        <div className="flex-1" />
        <div className="flex items-center gap-1">
          <Button
            variant="outline"
            size="sm"
            className="h-7 text-xs"
            onClick={() => setSetOperation('inter')}
          >
            <GitMerge className="h-3 w-3 mr-1" />
            SINTER
          </Button>
          <Button
            variant="outline"
            size="sm"
            className="h-7 text-xs"
            onClick={() => setSetOperation('union')}
          >
            <ArrowUpFromLine className="h-3 w-3 mr-1" />
            SUNION
          </Button>
          <Button
            variant="outline"
            size="sm"
            className="h-7 text-xs"
            onClick={() => setSetOperation('diff')}
          >
            <Minus className="h-3 w-3 mr-1" />
            SDIFF
          </Button>
        </div>
      </div>

      {/* Add member form */}
      <div className="flex items-center gap-2 px-3 py-2 border-b bg-muted/10">
        <Input
          value={newMember}
          onChange={(e) => setNewMember(e.target.value)}
          placeholder="输入成员值..."
          className="h-7 text-xs flex-1"
          onKeyDown={handleKeyDown}
        />
        <Button
          variant="default"
          size="sm"
          className="h-7 text-xs flex-shrink-0"
          onClick={handleAdd}
          disabled={!newMember.trim()}
        >
          <Plus className="h-3 w-3 mr-1" />
          SADD
        </Button>
      </div>

      {/* Member grid */}
      <ScrollArea className="flex-1">
        <div className="flex flex-wrap gap-2 p-3">
          {value.members.map((member) => (
            <div
              key={member}
              className="group flex items-center gap-1.5 rounded-lg border bg-card px-2.5 py-1.5 text-xs font-mono transition-colors hover:border-primary/50 hover:shadow-sm"
            >
              <span className="truncate max-w-[250px]">{member}</span>
              <Button
                variant="ghost"
                size="icon"
                className="h-4 w-4 opacity-0 group-hover:opacity-100 transition-opacity text-red-500 hover:text-red-700 hover:bg-red-50"
                onClick={() => {
                  if (confirm(`确认删除成员 "${member}" ?`)) {
                    onRemove(member)
                  }
                }}
              >
                <Trash2 className="h-2.5 w-2.5" />
              </Button>
            </div>
          ))}
          {value.members.length === 0 && (
            <div className="w-full flex items-center justify-center py-8 text-xs text-muted-foreground">
              集合为空
            </div>
          )}
        </div>
      </ScrollArea>

      {/* Set operation dialog */}
      {setOperation && (
        <Dialog open={true} onOpenChange={() => closeDialog()}>
          <DialogContent className="sm:max-w-[480px]">
            <DialogHeader>
              <DialogTitle>
                {setOperation === 'inter' ? 'SINTER 交集' : setOperation === 'union' ? 'SUNION 并集' : 'SDIFF 差集'}
              </DialogTitle>
            </DialogHeader>
            <div className="grid gap-4 py-2">
              <div className="grid gap-1.5">
                <label className="text-sm font-medium">当前 Key</label>
                <Input value={value.key} disabled className="text-xs font-mono" />
              </div>
              <div className="grid gap-1.5">
                <label className="text-sm font-medium">比较的目标 Key</label>
                <Input
                  value={secondKey}
                  onChange={(e) => setSecondKey(e.target.value)}
                  placeholder="例如: product:categories"
                  className="text-xs font-mono"
                  autoFocus
                />
              </div>
              <Button onClick={handleSetOperation} disabled={!secondKey.trim()}>
                执行
              </Button>

              {operationResult !== null && (
                <div className="rounded-lg border bg-muted/20 p-3">
                  <span className="text-xs text-muted-foreground">
                    结果 ({operationResult.length} 个成员):
                  </span>
                  <div className="mt-2 flex flex-wrap gap-1.5">
                    {operationResult.length > 0 ? (
                      operationResult.map((m) => (
                        <span key={m} className="px-2 py-0.5 rounded bg-background text-xs font-mono border">
                          {m}
                        </span>
                      ))
                    ) : (
                      <span className="text-xs text-muted-foreground italic">空集合</span>
                    )}
                  </div>
                </div>
              )}
            </div>
          </DialogContent>
        </Dialog>
      )}
    </div>
  )
}
