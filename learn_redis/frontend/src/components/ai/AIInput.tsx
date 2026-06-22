import React, { useState, useRef, useCallback, type KeyboardEvent } from 'react'
import { Sparkles, Loader2 } from 'lucide-react'
import { Button } from '@/components/ui/button'
import { ContextChip } from '@/components/ai/ContextChip'
import { useAIStore } from '@/stores/aiStore'
import { cn } from '@/lib/utils'

interface AIInputProps {
  onSend: (content: string) => void
  isStreaming: boolean
  placeholder?: string
}

export function AIInput({
  onSend,
  isStreaming,
  placeholder = '向 AI 提问...',
}: AIInputProps) {
  const [value, setValue] = useState('')
  const textareaRef = useRef<HTMLTextAreaElement>(null)

  const context = useAIStore((s) => s.context)
  const clearContext = useAIStore((s) => s.clearContext)

  const isEmpty = value.trim().length === 0
  const canSend = !isEmpty && !isStreaming

  const handleSend = useCallback(() => {
    if (!canSend) return
    onSend(value.trim())
    setValue('')
    if (textareaRef.current) {
      textareaRef.current.style.height = 'auto'
    }
  }, [canSend, onSend, value])

  const handleKeyDown = useCallback(
    (e: KeyboardEvent<HTMLTextAreaElement>) => {
      if (e.key === 'Enter' && !e.shiftKey) {
        e.preventDefault()
        handleSend()
      }
    },
    [handleSend],
  )

  const handleInput = useCallback(() => {
    const textarea = textareaRef.current
    if (!textarea) return
    textarea.style.height = 'auto'
    const lineHeight = 24
    const minHeight = lineHeight + 16
    const maxHeight = lineHeight * 5 + 16
    const newHeight = Math.min(Math.max(textarea.scrollHeight, minHeight), maxHeight)
    textarea.style.height = `${newHeight}px`
  }, [])

  return (
    <div className="border-t bg-background">
      {/* Context info */}
      {context && context.type !== 'none' && (
        <div className="flex items-center gap-2 px-3 pt-3">
          <span className="text-xs text-muted-foreground">基于: </span>
          <ContextChip
            label={context.label || '当前页面'}
            type={context.type}
            onRemove={clearContext}
          />
        </div>
      )}

      {/* Input area */}
      <div className="flex items-end gap-2 p-3">
        <textarea
          ref={textareaRef}
          value={value}
          onChange={(e) => setValue(e.target.value)}
          onKeyDown={handleKeyDown}
          onInput={handleInput}
          rows={1}
          disabled={isStreaming}
          placeholder={placeholder}
          className={cn(
            'flex-1 resize-none rounded-lg border border-input bg-background px-3 py-2',
            'text-sm ring-offset-background placeholder:text-muted-foreground',
            'focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-ring focus-visible:ring-offset-2',
            'disabled:cursor-not-allowed disabled:opacity-50',
          )}
        />

        <Button
          onClick={handleSend}
          disabled={!canSend}
          size="icon"
          variant="default"
          className="flex-shrink-0 h-10 w-10"
          aria-label="发送消息"
        >
          {isStreaming ? (
            <Loader2 className="h-4 w-4 animate-spin" />
          ) : (
            <Sparkles className="h-4 w-4" />
          )}
        </Button>
      </div>
    </div>
  )
}
