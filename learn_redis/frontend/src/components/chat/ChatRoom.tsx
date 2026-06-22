import React, { useRef, useEffect, useMemo } from 'react'
import { useChatStore } from '@/stores/chatStore'
import { useAuthStore } from '@/stores/authStore'
import { ChatBubble } from '@/components/chat/ChatBubble'
import { ChatInput } from '@/components/chat/ChatInput'
import { TypingIndicator } from '@/components/chat/TypingIndicator'
import { ScrollArea } from '@/components/ui/scroll-area'
import { Users } from 'lucide-react'

export function ChatRoom() {
  const {
    messages,
    activeRoomId,
    rooms,
    typingUsers,
    addMessage,
  } = useChatStore()

  const currentUser = useAuthStore((s) => s.user)

  const scrollBottomRef = useRef<HTMLDivElement>(null)

  const activeRoom = activeRoomId ? rooms.get(activeRoomId) : undefined
  const roomMessages = useMemo(
    () => (activeRoomId ? messages.get(activeRoomId) ?? [] : []),
    [messages, activeRoomId],
  )
  const typingInRoom = useMemo(
    () => (activeRoomId ? typingUsers.get(activeRoomId) ?? [] : []),
    [typingUsers, activeRoomId],
  )

  // Auto-scroll to bottom when messages or typing changes
  useEffect(() => {
    scrollBottomRef.current?.scrollIntoView({ behavior: 'smooth' })
  }, [roomMessages, typingInRoom])

  const handleSend = (content: string) => {
    if (!activeRoomId || !currentUser) return

    const message = {
      id: `temp-${Date.now()}`,
      roomId: activeRoomId,
      userId: currentUser.id,
      userName: currentUser.username,
      content,
      createdAt: new Date().toISOString(),
      status: 'sending' as const,
    }
    addMessage(activeRoomId, message)
    // Note: actual WebSocket send would be handled by a hook/subscription layer
  }

  // Group messages by date for date dividers
  const groupedMessages = useMemo(() => {
    const groups: { date: string; messages: typeof roomMessages }[] = []
    let currentDate = ''

    for (const msg of roomMessages) {
      const msgDate = new Date(msg.createdAt).toLocaleDateString('zh-CN')
      if (msgDate !== currentDate) {
        currentDate = msgDate
        groups.push({ date: msgDate, messages: [msg] })
      } else {
        groups[groups.length - 1].messages.push(msg)
      }
    }

    return groups
  }, [roomMessages])

  if (!activeRoomId || !activeRoom) {
    return (
      <div className="flex flex-col items-center justify-center h-full text-muted-foreground">
        <p>请选择一个聊天室开始交流</p>
      </div>
    )
  }

  return (
    <div className="flex flex-col h-full">
      {/* Chat header */}
      <div className="flex-shrink-0 flex items-center justify-between px-4 py-3 border-b bg-background">
        <div>
          <h2 className="text-lg font-semibold">{activeRoom.name}</h2>
          <div className="flex items-center gap-1.5 text-xs text-muted-foreground">
            <Users className="h-3.5 w-3.5" />
            <span>{activeRoom.members.length} 人在线</span>
          </div>
        </div>
      </div>

      {/* Message list */}
      <ScrollArea className="flex-1">
        <div className="py-2">
          {groupedMessages.map((group) => (
            <div key={group.date}>
              {/* Date divider */}
              <div className="flex justify-center py-3">
                <span className="text-xs text-muted-foreground bg-muted/50 px-3 py-0.5 rounded-full">
                  {group.date}
                </span>
              </div>

              {group.messages.map((msg) => (
                <ChatBubble
                  key={msg.id}
                  variant={
                    currentUser && msg.userId === currentUser.id
                      ? 'self'
                      : 'other'
                  }
                  content={msg.content}
                  timestamp={msg.createdAt}
                  userName={msg.userName}
                  userAvatar={msg.userAvatar}
                  status={msg.status}
                />
              ))}
            </div>
          ))}

          {/* Typing indicator */}
          <TypingIndicator users={typingInRoom} />

          {/* Scroll anchor */}
          <div ref={scrollBottomRef} />
        </div>
      </ScrollArea>

      {/* Chat input */}
      <ChatInput onSend={handleSend} />
    </div>
  )
}
