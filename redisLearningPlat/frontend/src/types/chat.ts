export interface User {
  id: string
  username: string
  email: string
  avatarUrl?: string
}

export interface ChatRoom {
  id: string
  name: string
  isGroup: boolean
  lastMessage?: ChatMessage
  unreadCount: number
  members: User[]
  createdAt: string
}

export interface ChatMessage {
  id: string
  roomId: string
  userId: string
  userName: string
  userAvatar?: string
  content: string
  createdAt: string
  status: MessageStatus
}

export type MessageStatus = 'sending' | 'delivered' | 'read' | 'failed'

export type MessageType = 'text' | 'system'

export interface WSMessage {
  type: 'message' | 'typing' | 'ack' | 'join' | 'leave' | 'read_receipt'
  roomId?: string
  userId?: string
  userName?: string
  content?: string
  messageId?: string
  clientId?: string
  status?: MessageStatus
  timestamp?: string
}
