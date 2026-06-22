import { create } from 'zustand'
import type { ChatMessage, ChatRoom } from '@/types/chat'

interface TypingUser {
  userId: string
  userName: string
}

interface ChatState {
  rooms: Map<string, ChatRoom>
  messages: Map<string, ChatMessage[]>
  activeRoomId: string | null
  typingUsers: Map<string, TypingUser[]>
  connectionStatus: 'connecting' | 'connected' | 'disconnected' | 'reconnecting'

  setRooms: (rooms: ChatRoom[]) => void
  addRoom: (room: ChatRoom) => void
  setActiveRoom: (roomId: string) => void
  setMessages: (roomId: string, messages: ChatMessage[]) => void
  addMessage: (roomId: string, message: ChatMessage) => void
  updateMessageStatus: (roomId: string, messageId: string, status: ChatMessage['status']) => void
  setTyping: (roomId: string, userId: string, userName: string, isTyping: boolean) => void
  setConnectionStatus: (status: ChatState['connectionStatus']) => void
  incrementUnread: (roomId: string) => void
  clearUnread: (roomId: string) => void
}

export const useChatStore = create<ChatState>()((set, get) => ({
  rooms: new Map(),
  messages: new Map(),
  activeRoomId: null,
  typingUsers: new Map(),
  connectionStatus: 'disconnected',

  setRooms: (rooms) => {
    const map = new Map<string, ChatRoom>()
    rooms.forEach((r) => map.set(r.id, r))
    set({ rooms: map })
  },

  addRoom: (room) =>
    set((s) => {
      const rooms = new Map(s.rooms)
      rooms.set(room.id, room)
      return { rooms }
    }),

  setActiveRoom: (roomId) => set({ activeRoomId: roomId }),

  setMessages: (roomId, messages) =>
    set((s) => {
      const map = new Map(s.messages)
      map.set(roomId, messages)
      return { messages: map }
    }),

  addMessage: (roomId, message) =>
    set((s) => {
      const map = new Map(s.messages)
      const existing = map.get(roomId) || []
      const withoutDup = existing.filter((m) => m.id !== message.id)
      map.set(roomId, [...withoutDup, message])
      return { messages: map }
    }),

  updateMessageStatus: (roomId, messageId, status) =>
    set((s) => {
      const map = new Map(s.messages)
      const msgs = map.get(roomId)
      if (!msgs) return { messages: map }
      map.set(
        roomId,
        msgs.map((m) => (m.id === messageId ? { ...m, status } : m)),
      )
      return { messages: map }
    }),

  setTyping: (roomId, userId, userName, isTyping) =>
    set((s) => {
      const typing = new Map(s.typingUsers)
      const current = typing.get(roomId) || []
      if (isTyping) {
        if (!current.some((u) => u.userId === userId)) {
          typing.set(roomId, [...current, { userId, userName }])
        }
      } else {
        typing.set(roomId, current.filter((u) => u.userId !== userId))
      }
      return { typingUsers: typing }
    }),

  setConnectionStatus: (status) => set({ connectionStatus: status }),

  incrementUnread: (roomId) =>
    set((s) => {
      const rooms = new Map(s.rooms)
      const room = rooms.get(roomId)
      if (room) rooms.set(roomId, { ...room, unreadCount: room.unreadCount + 1 })
      return { rooms }
    }),

  clearUnread: (roomId) =>
    set((s) => {
      const rooms = new Map(s.rooms)
      const room = rooms.get(roomId)
      if (room) rooms.set(roomId, { ...room, unreadCount: 0 })
      return { rooms }
    }),
}))
