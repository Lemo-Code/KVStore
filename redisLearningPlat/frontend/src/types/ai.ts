export interface AIMessage {
  id: string
  role: 'user' | 'assistant' | 'system'
  content: string
  timestamp: string
  isStreaming?: boolean
}

export interface AIContext {
  type: 'article' | 'page' | 'none'
  label?: string
  articleId?: string
  pagePath?: string
}

export interface AIProvider {
  id: string
  name: string
  baseURL: string
  apiKey: string
  models: string[]
}

export interface AIChatRequest {
  messages: { role: string; content: string }[]
  context?: AIContext
  provider?: string
  model?: string
}

export interface AIChatStreamChunk {
  type: 'chunk' | 'done' | 'error'
  content?: string
  error?: string
}
