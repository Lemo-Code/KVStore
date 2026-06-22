import { api } from './api'
import type { AIChatRequest } from '@/types/ai'

export const aiService = {
  streamChat: (
    request: AIChatRequest,
    onChunk: (content: string) => void,
    onDone: () => void,
    onError: (err: Error) => void,
  ) => api.streamFetch('/ai/chat', request, onChunk, onDone, onError),

  getHistory: () => api.get<{ id: string; title: string; updatedAt: string }[]>('/ai/history'),

  deleteHistory: (id: string) => api.delete(`/ai/history/${id}`),
}
