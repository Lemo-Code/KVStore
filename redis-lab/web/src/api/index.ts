import api from './client';
import type { User, Connection, QuotaStatus, CommandResult, ChatRoom, ChatMessage, AIConversation, AIMessage } from '../types';

// ---- Auth ----
export const AuthAPI = {
  register: (u: string, e: string, p: string) => api.post('/auth/register', { username: u, email: e, password: p }),
  login: (u: string, p: string) => api.post<{ token: string; user: User }>('/auth/login', { username: u, password: p }),
  me: () => api.get<{ user: User }>('/auth/me'),
};

// ---- Quota ----
export const QuotaAPI = {
  get: () => api.get<{ quota: QuotaStatus }>('/quota'),
};

// ---- Connections ----
export const ConnAPI = {
  list: () => api.get<{ connections: Connection[] }>('/connections'),
  create: (d: { name: string; host: string; port: number; password?: string; db_index?: number }) =>
    api.post<{ connection: Connection }>('/connections', d),
  update: (id: number, d: Partial<Connection>) => api.put<{ connection: Connection }>(`/connections/${id}`, d),
  remove: (id: number) => api.delete(`/connections/${id}`),
  test: (id: number) => api.post<{ success: boolean; message: string }>(`/connections/${id}/test`),
};

// ---- Redis ----
export const RedisAPI = {
  exec: (connId: number, cmd: string) => api.post<{ result: CommandResult }>(`/connections/${connId}/exec`, { command: cmd }),
  keys: (connId: number, pattern?: string) => api.get<{ keys: string[] }>(`/connections/${connId}/keys`, { params: { pattern } }),
  keyDetail: (connId: number, key: string) =>
    api.get<{ detail: { key: string; type: string; ttl: number; value: unknown } }>(`/connections/${connId}/keys/${key}`),
  deleteKey: (connId: number, key: string) => api.delete(`/connections/${connId}/keys/${key}`),
  info: (connId: number) => api.get<{ info: string }>(`/connections/${connId}/info`),
  flush: (connId: number) => api.post(`/connections/${connId}/flush`),
};

// ---- Chat ----
export const ChatAPI = {
  rooms: () => api.get<{ rooms: ChatRoom[] }>('/rooms'),
  messages: (roomId: number, before?: string) =>
    api.get<{ messages: ChatMessage[] }>(`/rooms/${roomId}/messages`, { params: { before, limit: 50 } }),
};

export function createChatSocket(): WebSocket {
  const t = localStorage.getItem('token');
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
  return new WebSocket(`${proto}//${location.host}/api/ws/chat?token=${t}`);
}

// ---- AI ----
export const AIAPI = {
  conversations: () => api.get<{ conversations: AIConversation[] }>('/ai/conversations'),
  messages: (id: number) => api.get<{ messages: AIMessage[] }>(`/ai/conversations/${id}`),
  removeConversation: (id: number) => api.delete(`/ai/conversations/${id}`),
};

export async function sendAiMessage(msg: string, conversationId?: number, connectionId?: number) {
  const t = localStorage.getItem('token');
  const r = await fetch('/api/ai/chat', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json', Authorization: `Bearer ${t}` },
    body: JSON.stringify({ message: msg, conversation_id: conversationId, connection_id: connectionId }),
  });
  if (!r.ok) throw new Error('AI request failed');
  return r.body;
}
