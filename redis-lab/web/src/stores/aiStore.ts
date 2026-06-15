import { create } from 'zustand';
import type { AIConversation, AIMessage } from '../types';
import { AIAPI, sendAiMessage } from '../api';

interface AIState {
  convs: AIConversation[];
  activeConvId: number | null;
  messages: AIMessage[];
  streaming: boolean;
  streamContent: string;
  loadConvs: () => Promise<void>;
  loadMessages: (id: number) => Promise<void>;
  send: (content: string, connId?: number) => Promise<void>;
  newConv: () => void;
  removeConv: (id: number) => Promise<void>;
}

export const useAI = create<AIState>((set, get) => ({
  convs: [], activeConvId: null, messages: [], streaming: false, streamContent: '',

  loadConvs: async () => { try { const { data } = await AIAPI.conversations(); set({ convs: data.conversations }); } catch { /* */ } },
  loadMessages: async (id) => { try { const { data } = await AIAPI.messages(id); set({ activeConvId: id, messages: data.messages }); } catch { /* */ } },

  send: async (content, connId) => {
    set((s) => ({
      messages: [...s.messages, { id: Date.now(), role: 'user' as const, content, created_at: new Date().toISOString() }],
      streaming: true, streamContent: '',
    }));
    try {
      const stream = await sendAiMessage(content, get().activeConvId || undefined, connId);
      if (!stream) throw new Error('No stream');
      const reader = stream.getReader(); const dec = new TextDecoder(); let full = '';
      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        for (const line of dec.decode(value, { stream: true }).split('\n')) {
          if (line.startsWith('data: ')) {
            try { const d = JSON.parse(line.slice(6)); if (d.token) { full += d.token; set({ streamContent: full }); } } catch { /* */ }
          }
        }
      }
      set((s) => ({
        messages: [...s.messages, { id: Date.now() + 1, role: 'assistant', content: full, created_at: new Date().toISOString() }],
        streaming: false, streamContent: '',
      }));
      get().loadConvs();
    } catch { set({ streaming: false, streamContent: '' }); }
  },

  newConv: () => set({ activeConvId: null, messages: [], streaming: false, streamContent: '' }),
  removeConv: async (id) => {
    try { await AIAPI.removeConversation(id); set((s) => ({ convs: s.convs.filter((c) => c.id !== id), activeConvId: s.activeConvId === id ? null : s.activeConvId, messages: s.activeConvId === id ? [] : s.messages })); } catch { /* */ }
  },
}));
