import { useState, useMemo } from 'react'
import { cn } from '@/lib/utils'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { ScrollArea } from '@/components/ui/scroll-area'
import { Search, Plus, Pencil, Trash2, Save, X, Hash } from 'lucide-react'
import type { HashValue, HashField } from '@/types/redis'

interface HashEditorProps {
  value: HashValue
  onAddField: (field: string, value: string) => Promise<void>
  onEditField: (field: string, value: string) => Promise<void>
  onDeleteField: (field: string) => Promise<void>
}

export default function HashEditor({ value, onAddField, onEditField, onDeleteField }: HashEditorProps) {
  const [filter, setFilter] = useState('')
  const [newField, setNewField] = useState('')
  const [newValue, setNewValue] = useState('')
  const [editingField, setEditingField] = useState<string | null>(null)
  const [editValue, setEditValue] = useState('')

  const filteredFields = useMemo(() => {
    if (!filter.trim()) return value.fields
    const lower = filter.toLowerCase()
    return value.fields.filter(
      (f) => f.field.toLowerCase().includes(lower) || f.value.toLowerCase().includes(lower)
    )
  }, [value.fields, filter])

  const handleAdd = async () => {
    if (!newField.trim() || !newValue.trim()) return
    await onAddField(newField.trim(), newValue.trim())
    setNewField('')
    setNewValue('')
  }

  const handleStartEdit = (field: HashField) => {
    setEditingField(field.field)
    setEditValue(field.value)
  }

  const handleSaveEdit = async (field: string) => {
    if (editValue.trim()) {
      await onEditField(field, editValue.trim())
    }
    setEditingField(null)
    setEditValue('')
  }

  const handleCancelEdit = () => {
    setEditingField(null)
    setEditValue('')
  }

  const handleKeyDown = (e: React.KeyboardEvent, action: () => void) => {
    if (e.key === 'Enter') {
      e.preventDefault()
      action()
    }
  }

  return (
    <div className="flex flex-col h-full">
      {/* Toolbar */}
      <div className="flex items-center gap-2 px-3 py-2 border-b bg-muted/30">
        <Hash className="h-4 w-4 text-muted-foreground" />
        <span className="text-xs text-muted-foreground">
          共 <span className="font-mono font-medium text-foreground">{value.fields.length}</span> 个字段
        </span>
        <div className="flex-1" />
        <div className="relative w-48">
          <Search className="absolute left-2 top-1/2 -translate-y-1/2 h-3 w-3 text-muted-foreground" />
          <Input
            value={filter}
            onChange={(e) => setFilter(e.target.value)}
            placeholder="搜索字段/值..."
            className="h-7 pl-7 text-xs"
          />
        </div>
      </div>

      {/* Add new field row */}
      <div className="flex items-center gap-2 px-3 py-2 border-b bg-muted/10">
        <Input
          value={newField}
          onChange={(e) => setNewField(e.target.value)}
          placeholder="字段名"
          className="h-7 text-xs flex-1"
          onKeyDown={(e) => handleKeyDown(e, handleAdd)}
        />
        <Input
          value={newValue}
          onChange={(e) => setNewValue(e.target.value)}
          placeholder="值"
          className="h-7 text-xs flex-1"
          onKeyDown={(e) => handleKeyDown(e, handleAdd)}
        />
        <Button
          variant="default"
          size="sm"
          className="h-7 text-xs flex-shrink-0"
          onClick={handleAdd}
          disabled={!newField.trim() || !newValue.trim()}
        >
          <Plus className="h-3 w-3 mr-1" />
          添加
        </Button>
      </div>

      {/* Table */}
      <ScrollArea className="flex-1">
        <table className="w-full text-sm">
          <thead className="sticky top-0 bg-muted/50">
            <tr className="border-b">
              <th className="w-12 px-3 py-2 text-left text-xs font-medium text-muted-foreground">#</th>
              <th className="px-3 py-2 text-left text-xs font-medium text-muted-foreground">Field</th>
              <th className="px-3 py-2 text-left text-xs font-medium text-muted-foreground">Value</th>
              <th className="w-24 px-3 py-2 text-right text-xs font-medium text-muted-foreground">操作</th>
            </tr>
          </thead>
          <tbody>
            {filteredFields.map((field, index) => (
              <tr
                key={field.field}
                className={cn('border-b transition-colors hover:bg-muted/30', index % 2 === 0 && 'bg-muted/10')}
              >
                <td className="px-3 py-1.5 text-xs text-muted-foreground tabular-nums">{index + 1}</td>
                <td className="px-3 py-1.5">
                  <span className="text-xs font-mono font-medium">{field.field}</span>
                </td>
                <td className="px-3 py-1.5 max-w-[300px]">
                  {editingField === field.field ? (
                    <div className="flex items-center gap-1">
                      <Input
                        value={editValue}
                        onChange={(e) => setEditValue(e.target.value)}
                        className="h-7 text-xs"
                        autoFocus
                        onKeyDown={(e) => handleKeyDown(e, () => handleSaveEdit(field.field))}
                      />
                      <Button
                        variant="ghost"
                        size="icon"
                        className="h-6 w-6 flex-shrink-0"
                        onClick={() => handleSaveEdit(field.field)}
                      >
                        <Save className="h-3 w-3 text-emerald-500" />
                      </Button>
                      <Button
                        variant="ghost"
                        size="icon"
                        className="h-6 w-6 flex-shrink-0"
                        onClick={handleCancelEdit}
                      >
                        <X className="h-3 w-3 text-red-500" />
                      </Button>
                    </div>
                  ) : (
                    <span className="text-xs font-mono truncate block max-w-[300px]">{field.value}</span>
                  )}
                </td>
                <td className="px-3 py-1.5 text-right">
                  <div className="flex items-center justify-end gap-0.5">
                    <Button
                      variant="ghost"
                      size="icon"
                      className="h-6 w-6"
                      onClick={() => handleStartEdit(field)}
                    >
                      <Pencil className="h-3 w-3" />
                    </Button>
                    <Button
                      variant="ghost"
                      size="icon"
                      className="h-6 w-6 text-red-500 hover:text-red-700 hover:bg-red-50"
                      onClick={() => {
                        if (confirm(`确认删除字段 "${field.field}" ?`)) {
                          onDeleteField(field.field)
                        }
                      }}
                    >
                      <Trash2 className="h-3 w-3" />
                    </Button>
                  </div>
                </td>
              </tr>
            ))}
            {filteredFields.length === 0 && (
              <tr>
                <td colSpan={4} className="px-3 py-8 text-center text-xs text-muted-foreground">
                  {filter ? '无匹配结果' : '暂无字段'}
                </td>
              </tr>
            )}
          </tbody>
        </table>
      </ScrollArea>
    </div>
  )
}
