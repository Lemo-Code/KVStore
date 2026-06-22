import { useState, useEffect, useCallback } from 'react'
import { cn } from '@/lib/utils'
import { Button } from '@/components/ui/button'
import { Separator } from '@/components/ui/separator'
import { Copy, Edit3, Eye, Save, X, Braces } from 'lucide-react'
import type { StringValue } from '@/types/redis'

interface StringEditorProps {
  value: StringValue
  onSave?: (newValue: string) => void
}

function isJSON(str: string): boolean {
  try {
    JSON.parse(str)
    return true
  } catch {
    return false
  }
}

function prettyJSON(str: string): string {
  try {
    return JSON.stringify(JSON.parse(str), null, 2)
  } catch {
    return str
  }
}

export default function StringEditor({ value, onSave }: StringEditorProps) {
  const [isEditing, setIsEditing] = useState(false)
  const [editText, setEditText] = useState(value.value)
  const [showJSON, setShowJSON] = useState(false)
  const [copied, setCopied] = useState(false)

  const jsonCapable = isJSON(value.value)

  useEffect(() => {
    setEditText(value.value)
    setIsEditing(false)
    setShowJSON(false)
  }, [value.key, value.value])

  const handleCopy = useCallback(async () => {
    const textToCopy = showJSON && jsonCapable ? prettyJSON(value.value) : value.value
    try {
      await navigator.clipboard.writeText(textToCopy)
    } catch {
      // Fallback
      const textarea = document.createElement('textarea')
      textarea.value = textToCopy
      document.body.appendChild(textarea)
      textarea.select()
      document.execCommand('copy')
      document.body.removeChild(textarea)
    }
    setCopied(true)
    setTimeout(() => setCopied(false), 2000)
  }, [value.value, showJSON, jsonCapable])

  const handleSave = () => {
    onSave?.(editText)
    setIsEditing(false)
  }

  const handleCancel = () => {
    setEditText(value.value)
    setIsEditing(false)
  }

  const displayValue = showJSON && jsonCapable ? prettyJSON(value.value) : value.value

  return (
    <div className="flex flex-col h-full">
      {/* Toolbar */}
      <div className="flex items-center gap-2 px-3 py-2 border-b bg-muted/30">
        <div className="flex items-center gap-1">
          <Button
            variant={isEditing ? 'default' : 'ghost'}
            size="sm"
            className="h-7 text-xs"
            onClick={() => setIsEditing(!isEditing)}
          >
            {isEditing ? (
              <>
                <Eye className="h-3.5 w-3.5 mr-1" />
                查看
              </>
            ) : (
              <>
                <Edit3 className="h-3.5 w-3.5 mr-1" />
                编辑
              </>
            )}
          </Button>

          {jsonCapable && !isEditing && (
            <Button
              variant={showJSON ? 'default' : 'ghost'}
              size="sm"
              className="h-7 text-xs"
              onClick={() => setShowJSON(!showJSON)}
            >
              <Braces className="h-3.5 w-3.5 mr-1" />
              JSON View
            </Button>
          )}

          <Button
            variant="ghost"
            size="sm"
            className="h-7 text-xs"
            onClick={handleCopy}
          >
            <Copy className="h-3.5 w-3.5 mr-1" />
            {copied ? '已复制' : '复制'}
          </Button>
        </div>

        <div className="flex-1" />

        {isEditing && (
          <div className="flex items-center gap-1">
            <Button variant="ghost" size="sm" className="h-7 text-xs" onClick={handleCancel}>
              <X className="h-3.5 w-3.5 mr-1" />
              取消
            </Button>
            <Button variant="default" size="sm" className="h-7 text-xs" onClick={handleSave}>
              <Save className="h-3.5 w-3.5 mr-1" />
              保存
            </Button>
          </div>
        )}
      </div>

      {/* Metadata bar */}
      <div className="flex items-center gap-3 px-3 py-1.5 text-xs text-muted-foreground border-b bg-muted/20">
        <span>长度: <span className="font-mono text-foreground">{value.value.length}</span></span>
        <span>编码: <span className="font-mono text-foreground">{value.encoding}</span></span>
      </div>

      {/* Content */}
      <div className="flex-1 overflow-auto">
        {isEditing ? (
          <textarea
            className="w-full h-full min-h-[200px] bg-background p-3 font-mono text-sm resize-none outline-none border-0 focus:ring-0"
            value={editText}
            onChange={(e) => setEditText(e.target.value)}
            spellCheck={false}
          />
        ) : (
          <pre
            className={cn(
              'p-3 font-mono text-sm whitespace-pre-wrap break-all',
              showJSON && jsonCapable && 'text-blue-700'
            )}
          >
            {displayValue || <span className="text-muted-foreground italic">(空字符串)</span>}
          </pre>
        )}
      </div>
    </div>
  )
}
