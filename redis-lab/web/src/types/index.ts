// ---- Auth ----
export interface User {
  id: number; username: string; email: string; avatar: string; created_at: string;
}

// ---- Connection ----
export interface Connection {
  id: number; user_id: number; name: string; host: string; port: number;
  password?: string; db_index: number; color?: string; group?: string;
  created_at: string; updated_at: string;
}

// ---- Quota ----
export interface QuotaStatus {
  max_keys: number; used_keys: number; keys_percent: number;
  max_value_size: number; max_memory: number; used_memory: number;
  memory_percent: number; rate_limit: number; total_cmds: number;
}

// ---- Redis Result ----
export type ResultType = 'string' | 'integer' | 'array' | 'error' | 'null';
export interface CommandResult { type: ResultType; value: unknown; }
export interface KeyDetail { key: string; type: string; ttl: number; value: unknown; }

// ---- Chat ----
export interface ChatRoom { id: number; name: string; title: string; description: string; }
export interface ChatUser { id: number; username: string; avatar: string; }
export interface ChatMessage {
  id: number; room_id: number; user: ChatUser; content: string; created_at: string;
}
export interface WSMessage {
  type: string; room?: string; content?: string; user?: ChatUser;
  timestamp?: string; message?: string; messages?: WSHistoryMsg[];
}
export interface WSHistoryMsg { id: number; content: string; user: ChatUser; created_at: string; }

// ---- AI ----
export interface AIConversation { id: number; title: string; created_at: string; updated_at: string; }
export interface AIMessage { id: number; role: 'user' | 'assistant'; content: string; created_at: string; }

// ---- UI State ----
export type RightPanelType = 'chat' | 'ai' | null;
export type LeftPanelTab = 'connections' | 'keys' | 'monitor';
