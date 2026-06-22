import { api } from './api'
import type { ChatRoom, ChatMessage } from '@/types/chat'

export const chatService = {
  getRooms: () => api.get<ChatRoom[]>('/chat/rooms'),

  createRoom: (name: string, isGroup: boolean) =>
    api.post<ChatRoom>('/chat/rooms', { name, is_group: isGroup }),

  getRoom: (id: string) => api.get<ChatRoom>(`/chat/rooms/${id}`),

  getMessages: (roomId: string, before?: string, limit = 50) =>
    api.get<ChatMessage[]>(`/chat/rooms/${roomId}/messages`, {
      ...(before ? { before } : {}),
      limit: String(limit),
    }),

  sendMessage: (roomId: string, content: string) =>
    api.post<ChatMessage>(`/chat/rooms/${roomId}/messages`, { content }),
}
