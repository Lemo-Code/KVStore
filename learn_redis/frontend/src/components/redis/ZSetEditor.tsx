import { useState, useMemo, useCallback } from 'react'
import { cn } from '@/lib/utils'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { ScrollArea } from '@/components/ui/scroll-area'
import { Layers, Plus, Pencil, Trash2, Save, X } from 'lucide-react'
import type { ZSetValue, ZSetMember } from '@/types/redis'

interface ZSetEditorProps {
  value: ZSetValue
  onAdd: (member: string, score: number) => Promise<void>
  onUpdateScore: (member: string, score: number) => Promise<void>
  onRemove: (member: string) => Promise<void>
}

export default function ZSetEditor({ value, onAdd, onUpdateScore, onRemove }: ZSetEditorProps) {
  const [newMember, setNewMember] = useState('')
  const [newScore, setNewScore] = useState('')
  const [scoreMin, setScoreMin] = useState('')
  const [scoreMax, setScoreMax] = useState('')
  const [editingMember, setEditingMember] = useState<string | null>(null)
  const [editScore, setEditScore] = useState('')

  // Sort by score descending
  const sortedMembers = useMemo(() => {
    let members = [...value.members].sort((a, b) => b.score - a.score)

    const min = scoreMin ? parseFloat(scoreMin) : -Infinity
    const max = scoreMax ? parseFloat(scoreMax) : Infinity

    if (scoreMin || scoreMax) {
      members = members.filter((m) => m.score >= min && m.score <= max)
    }

    return members
  }, [value.members, scoreMin, scoreMax])

  const handleAdd = async () => {
    if (!newMember.trim() || !newScore.trim()) return
    const scoreNum = parseFloat(newScore)
    if (isNaN(scoreNum)) return
    await onAdd(newMember.trim(), scoreNum)
    setNewMember('')
    setNewScore('')
  }

  const handleStartEdit = (member: ZSetMember) => {
    setEditingMember(member.member)
    setEditScore(String(member.score))
  }

  const handleSaveEdit = async (member: string) => {
    const scoreNum = parseFloat(editScore)
    if (!isNaN(scoreNum)) {
      await onUpdateScore(member, scoreNum)
    }
    setEditingMember(null)
    setEditScore('')
  }

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter') {
      e.preventDefault()
      handleAdd()
    }
  }

  return (
    <div className="flex flex-col h-full">
      {/* Toolbar */}
      <div className="flex items-center gap-2 px-3 py-2 border-b bg-muted/30">
        <Layers className="h-4 w-4 text-muted-foreground" />
        <span className="text-xs text-muted-foreground">
          成员: <span className="font-mono font-medium text-foreground">{value.length}</span>
        </span>
        <div className="flex-1" />
        <div className="flex items-center gap-2">
          <Input
            value={scoreMin}
            onChange={(e) => setScoreMin(e.target.value)}
            placeholder="最低分"
            className="h-7 w-20 text-xs"
          />
          <span className="text-xs text-muted-foreground">-</span>
          <Input
            value={scoreMax}
            onChange={(e) => setScoreMax(e.target.value)}
            placeholder="最高分"
            className="h-7 w-20 text-xs"
          />
        </div>
      </div>

      {/* Add member form */}
      <div className="flex items-center gap-2 px-3 py-2 border-b bg-muted/10">
        <Input
          value={newMember}
          onChange={(e) => setNewMember(e.target.value)}
          placeholder="成员"
          className="h-7 text-xs flex-1"
          onKeyDown={handleKeyDown}
        />
        <Input
          value={newScore}
          onChange={(e) => setNewScore(e.target.value)}
          placeholder="分数"
          type="number"
          step="any"
          className="h-7 w-24 text-xs"
          onKeyDown={handleKeyDown}
        />
        <Button
          variant="default"
          size="sm"
          className="h-7 text-xs flex-shrink-0"
          onClick={handleAdd}
          disabled={!newMember.trim() || !newScore.trim()}
        >
          <Plus className="h-3 w-3 mr-1" />
          ZADD
        </Button>
      </div>

      {/* Table */}
      <ScrollArea className="flex-1">
        <table className="w-full text-sm">
          <thead className="sticky top-0 bg-muted/50">
            <tr className="border-b">
              <th className="w-12 px-3 py-2 text-left text-xs font-medium text-muted-foreground">#</th>
              <th className="px-3 py-2 text-left text-xs font-medium text-muted-foreground">Member</th>
              <th className="w-28 px-3 py-2 text-right text-xs font-medium text-muted-foreground">Score</th>
              <th className="w-24 px-3 py-2 text-right text-xs font-medium text-muted-foreground">操作</th>
            </tr>
          </thead>
          <tbody>
            {sortedMembers.map((member, index) => (
              <tr
                key={member.member}
                className={cn('border-b transition-colors hover:bg-muted/30', index % 2 === 0 && 'bg-muted/10')}
              >
                <td className="px-3 py-1.5 text-xs text-muted-foreground tabular-nums">{index + 1}</td>
                <td className="px-3 py-1.5">
                  <span className="text-xs font-mono font-medium truncate block max-w-[300px]">
                    {member.member}
                  </span>
                </td>
                <td className="px-3 py-1.5 text-right">
                  {editingMember === member.member ? (
                    <div className="flex items-center justify-end gap-1">
                      <Input
                        value={editScore}
                        onChange={(e) => setEditScore(e.target.value)}
                        className="h-7 w-24 text-xs text-right"
                        type="number"
                        step="any"
                        autoFocus
                        onKeyDown={(e) => {
                          if (e.key === 'Enter') handleSaveEdit(member.member)
                          if (e.key === 'Escape') setEditingMember(null)
                        }}
                      />
                      <Button
                        variant="ghost"
                        size="icon"
                        className="h-6 w-6"
                        onClick={() => handleSaveEdit(member.member)}
                      >
                        <Save className="h-3 w-3 text-emerald-500" />
                      </Button>
                      <Button
                        variant="ghost"
                        size="icon"
                        className="h-6 w-6"
                        onClick={() => setEditingMember(null)}
                      >
                        <X className="h-3 w-3 text-red-500" />
                      </Button>
                    </div>
                  ) : (
                    <span className="text-xs font-mono text-blue-600 tabular-nums">{member.score.toLocaleString()}</span>
                  )}
                </td>
                <td className="px-3 py-1.5 text-right">
                  <div className="flex items-center justify-end gap-0.5">
                    <Button
                      variant="ghost"
                      size="icon"
                      className="h-6 w-6"
                      onClick={() => handleStartEdit(member)}
                    >
                      <Pencil className="h-3 w-3" />
                    </Button>
                    <Button
                      variant="ghost"
                      size="icon"
                      className="h-6 w-6 text-red-500 hover:text-red-700 hover:bg-red-50"
                      onClick={() => {
                        if (confirm(`确认删除成员 "${member.member}" ?`)) {
                          onRemove(member.member)
                        }
                      }}
                    >
                      <Trash2 className="h-3 w-3" />
                    </Button>
                  </div>
                </td>
              </tr>
            ))}
            {sortedMembers.length === 0 && (
              <tr>
                <td colSpan={4} className="px-3 py-8 text-center text-xs text-muted-foreground">
                  {scoreMin || scoreMax ? '无匹配结果' : '暂无成员'}
                </td>
              </tr>
            )}
          </tbody>
        </table>
      </ScrollArea>
    </div>
  )
}
