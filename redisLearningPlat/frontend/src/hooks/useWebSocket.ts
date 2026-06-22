import { useEffect, useRef, useCallback } from 'react'
import { WS_BASE_URL } from '@/lib/constants'
import { useChatStore } from '@/stores/chatStore'
import { useAuthStore } from '@/stores/authStore'
import { getId } from '@/lib/utils'
import type { WSMessage, ChatMessage } from '@/types/chat'

export function useWebSocket(roomId: string | null) {
  const wsRef = useRef<WebSocket | null>(null)
  const reconnectTimerRef = useRef<ReturnType<typeof setTimeout>>()
  const reconnectAttemptRef = useRef(0)
  const maxReconnectDelay = 30000

  const { addMessage, updateMessageStatus, setTyping, setConnectionStatus } = useChatStore()
  const token = useAuthStore((s) => s.token)

  const connect = useCallback(() => {
    if (!roomId) return

    try {
      const url = `${WS_BASE_URL}/ws/chat/${roomId}${token ? `?token=${token}` : ''}`
      const ws = new WebSocket(url)
      wsRef.current = ws
      setConnectionStatus('connecting')

      ws.onopen = () => {
        setConnectionStatus('connected')
        reconnectAttemptRef.current = 0
      }

      ws.onmessage = (event) => {
        try {
          const data: WSMessage = JSON.parse(event.data)

          switch (data.type) {
            case 'message': {
              const message: ChatMessage = {
                id: data.messageId || getId(),
                roomId: data.roomId || roomId,
                userId: data.userId || '',
                userName: data.userName || 'Unknown',
                userAvatar: undefined,
                content: data.content || '',
                createdAt: data.timestamp || new Date().toISOString(),
                status: 'delivered',
              }
              addMessage(roomId, message)
              break
            }
            case 'ack': {
              if (data.clientId && data.messageId) {
                updateMessageStatus(roomId, data.clientId, 'delivered')
                if (data.messageId) {
                  updateMessageStatus(roomId, data.clientId, 'read')
                }
              }
              break
            }
            case 'typing': {
              if (data.userId && data.userName !== undefined) {
                const isTyping = data.content === 'true' || data.content === 'start'
                const rId = data.roomId || roomId || ''
                const uid = data.userId
                const uname = data.userName
                setTyping(rId, uid, uname, isTyping)
                if (isTyping) {
                  setTimeout(() => {
                    setTyping(rId, uid, uname, false)
                  }, 3000)
                }
              }
              break
            }
          }
        } catch {
          // ignore parse errors
        }
      }

      ws.onclose = () => {
        setConnectionStatus('disconnected')
        scheduleReconnect()
      }

      ws.onerror = () => {
        ws.close()
      }
    } catch {
      scheduleReconnect()
    }
  }, [roomId, token, addMessage, updateMessageStatus, setTyping, setConnectionStatus])

  const scheduleReconnect = useCallback(() => {
    const delay = Math.min(1000 * 2 ** reconnectAttemptRef.current, maxReconnectDelay)
    reconnectAttemptRef.current += 1
    setConnectionStatus('reconnecting')
    reconnectTimerRef.current = setTimeout(connect, delay)
  }, [connect, setConnectionStatus])

  useEffect(() => {
    connect()

    const heartbeat = setInterval(() => {
      if (wsRef.current?.readyState === WebSocket.OPEN) {
        wsRef.current.send(JSON.stringify({ type: 'ping' }))
      }
    }, 30000)

    return () => {
      clearInterval(heartbeat)
      clearTimeout(reconnectTimerRef.current)
      wsRef.current?.close()
      setConnectionStatus('disconnected')
    }
  }, [connect, setConnectionStatus])

  const sendMessage = useCallback(
    (content: string) => {
      const clientId = getId()
      const ws = wsRef.current
      if (ws?.readyState === WebSocket.OPEN) {
        const optimisticMessage: ChatMessage = {
          id: clientId,
          roomId: roomId || '',
          userId: useAuthStore.getState().user?.id || '',
          userName: useAuthStore.getState().user?.username || 'Me',
          content,
          createdAt: new Date().toISOString(),
          status: 'sending',
        }
        addMessage(roomId || '', optimisticMessage)

        ws.send(
          JSON.stringify({
            type: 'message',
            roomId,
            content,
            clientId,
          }),
        )
      }
    },
    [roomId, addMessage],
  )

  const sendTyping = useCallback(
    (isTyping: boolean) => {
      if (wsRef.current?.readyState === WebSocket.OPEN) {
        wsRef.current.send(JSON.stringify({ type: 'typing', roomId, isTyping }))
      }
    },
    [roomId],
  )

  return { sendMessage, sendTyping, status: useChatStore((s) => s.connectionStatus) }
}
