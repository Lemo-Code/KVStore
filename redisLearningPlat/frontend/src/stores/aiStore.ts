import { create } from 'zustand'
import type { AIMessage, AIContext } from '@/types/ai'
import { getId } from '@/lib/utils'

interface AIState {
  conversations: Map<string, AIMessage[]>
  activeConversationId: string
  isStreaming: boolean
  context: AIContext | null
  selectedProvider: string
  selectedModel: string

  setActiveConversation: (id: string) => void
  newConversation: () => void
  addMessage: (message: AIMessage) => void
  appendToLastMessage: (chunk: string) => void
  finalizeLastMessage: () => void
  setStreaming: (streaming: boolean) => void
  setContext: (context: AIContext | null) => void
  clearContext: () => void
  setProvider: (provider: string) => void
  setModel: (model: string) => void
  getActiveMessages: () => AIMessage[]
}

export const useAIStore = create<AIState>()((set, get) => ({
  conversations: new Map(),
  activeConversationId: 'default',
  isStreaming: false,
  context: null,
  selectedProvider: 'openai',
  selectedModel: 'gpt-4o-mini',

  setActiveConversation: (id) => set({ activeConversationId: id }),

  newConversation: () => {
    const id = getId()
    set((s) => {
      const convos = new Map(s.conversations)
      convos.set(id, [])
      return { conversations: convos, activeConversationId: id, context: null }
    })
  },

  addMessage: (message) =>
    set((s) => {
      const convos = new Map(s.conversations)
      const current = convos.get(s.activeConversationId) || []
      convos.set(s.activeConversationId, [...current, message])
      return { conversations: convos }
    }),

  appendToLastMessage: (chunk) =>
    set((s) => {
      const convos = new Map(s.conversations)
      const current = convos.get(s.activeConversationId) || []
      if (current.length === 0) return { conversations: convos }
      const updated = [...current]
      const last = { ...updated[updated.length - 1] }
      last.content += chunk
      updated[updated.length - 1] = last
      convos.set(s.activeConversationId, updated)
      return { conversations: convos }
    }),

  finalizeLastMessage: () =>
    set((s) => {
      const convos = new Map(s.conversations)
      const current = convos.get(s.activeConversationId) || []
      if (current.length === 0) return { conversations: convos }
      const updated = [...current]
      updated[updated.length - 1] = { ...updated[updated.length - 1], isStreaming: false }
      convos.set(s.activeConversationId, updated)
      return { conversations: convos }
    }),

  setStreaming: (streaming) => set({ isStreaming: streaming }),

  setContext: (context) => set({ context }),
  clearContext: () => set({ context: null }),

  setProvider: (provider) => set({ selectedProvider: provider }),
  setModel: (model) => set({ selectedModel: model }),

  getActiveMessages: () => {
    const state = get()
    return state.conversations.get(state.activeConversationId) || []
  },
}))
