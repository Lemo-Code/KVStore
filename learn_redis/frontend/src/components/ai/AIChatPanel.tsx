import React, { useRef, useEffect, useMemo, useCallback } from 'react'
import { X } from 'lucide-react'
import { useAIStore } from '@/stores/aiStore'
import { useAuthStore } from '@/stores/authStore'
import { ChatBubble } from '@/components/chat/ChatBubble'
import { AIInput } from '@/components/ai/AIInput'
import { AIResponse } from '@/components/ai/AIResponse'
import { ContextChip } from '@/components/ai/ContextChip'
import { ScrollArea } from '@/components/ui/scroll-area'
import { Button } from '@/components/ui/button'
import { cn } from '@/lib/utils'

interface AIChatPanelProps {
  isOpen: boolean
  onClose: () => void
}

export function AIChatPanel({ isOpen, onClose }: AIChatPanelProps) {
  const {
    conversations,
    activeConversationId,
    isStreaming,
    context,
    addMessage,
    clearContext,
    getActiveMessages,
  } = useAIStore()

  const currentUser = useAuthStore((s) => s.user)

  const scrollBottomRef = useRef<HTMLDivElement>(null)

  const messages = useMemo(
    () => (activeConversationId ? conversations.get(activeConversationId) ?? [] : []),
    [conversations, activeConversationId],
  )

  // Auto-scroll to bottom on new messages or streaming
  useEffect(() => {
    scrollBottomRef.current?.scrollIntoView({ behavior: 'smooth' })
  }, [messages, isStreaming])

  const handleSend = useCallback(
    (content: string) => {
      // Add user message
      const userMessage = {
        id: `user-${Date.now()}`,
        role: 'user' as const,
        content,
        timestamp: new Date().toISOString(),
      }
      addMessage(userMessage)

      // Add placeholder assistant message that will be updated via streaming
      const assistantMessage = {
        id: `assistant-${Date.now()}`,
        role: 'assistant' as const,
        content: '',
        timestamp: new Date().toISOString(),
        isStreaming: true,
      }
      addMessage(assistantMessage)

      // Note: actual AI API call would be triggered here via a service layer
    },
    [addMessage],
  )

  if (!isOpen) return null

  return (
    <div
      className={cn(
        'fixed right-0 top-0 h-full w-full sm:w-96 lg:w-[420px]',
        'border-l bg-background shadow-lg z-40',
        'flex flex-col',
      )}
    >
      {/* Panel header */}
      <div className="flex-shrink-0 flex items-center justify-between px-4 py-3 border-b">
        <div>
          <h2 className="text-lg font-semibold">AI 助手</h2>
          <p className="text-xs text-muted-foreground">
            基于知识库回答 Redis 相关问题
          </p>
        </div>
        <Button variant="ghost" size="icon-sm" onClick={onClose} aria-label="关闭 AI 面板">
          <X className="h-4 w-4" />
        </Button>
      </div>

      {/* Context chips */}
      {context && context.type !== 'none' && (
        <div className="flex-shrink-0 flex items-center gap-2 px-4 py-2 border-b bg-muted/30">
          <span className="text-xs text-muted-foreground">当前上下文: </span>
          <ContextChip
            label={context.label || '当前页面'}
            type={context.type}
            onRemove={clearContext}
          />
        </div>
      )}

      {/* Messages */}
      <ScrollArea className="flex-1">
        <div className="py-4">
          {messages.length === 0 ? (
            <div className="flex flex-col items-center justify-center h-64 text-muted-foreground px-8">
              <p className="text-lg font-medium mb-2">你好！</p>
              <p className="text-sm text-center">
                我是 Redis 学习助手，可以回答关于 Redis 的任何问题。试试问我一些关于数据类型、集群配置或性能优化的问题。
              </p>
            </div>
          ) : (
            messages.map((msg) => {
              if (msg.role === 'user') {
                return (
                  <ChatBubble
                    key={msg.id}
                    variant="self"
                    content={msg.content}
                    timestamp={msg.timestamp}
                    userName={currentUser?.username}
                  />
                )
              }

              // Assistant message
              return (
                <div key={msg.id} className="px-4 mb-4">
                  <div className="flex gap-2">
                    <div className="flex-shrink-0 h-8 w-8 rounded-full bg-primary/10 flex items-center justify-center">
                      <span className="text-xs font-semibold text-primary">AI</span>
                    </div>
                    <div className="flex-1 min-w-0">
                      <span className="text-xs text-muted-foreground mb-1 block">
                        AI 助手
                      </span>
                      <div className="bg-muted rounded-2xl rounded-bl-md px-4 py-2.5">
                        <AIResponse
                          content={msg.content}
                          isStreaming={msg.isStreaming || isStreaming}
                        />
                      </div>
                      <span className="text-xs text-muted-foreground mt-1">
                        {msg.timestamp
                          ? new Date(msg.timestamp).toLocaleTimeString('zh-CN', {
                              hour: '2-digit',
                              minute: '2-digit',
                            })
                          : ''}
                      </span>
                    </div>
                  </div>
                </div>
              )
            })
          )}

          {/* Scroll anchor */}
          <div ref={scrollBottomRef} />
        </div>
      </ScrollArea>

      {/* Input */}
      <AIInput onSend={handleSend} isStreaming={isStreaming} />
    </div>
  )
}
