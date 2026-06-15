import { create } from 'zustand';
import type { ChatRoom, ChatMessage, WSMessage, ChatUser } from '../types';
import { ChatAPI, createChatSocket } from '../api';

interface ChatState {
  rooms: ChatRoom[];
  messages: Record<number, ChatMessage[]>;
  activeRoomId: number | null;
  unread: Record<number, number>;
  ws: WebSocket | null;
  connected: boolean;
  loadRooms: () => Promise<void>;
  loadHistory: (roomId: number) => Promise<void>;
  connect: () => void;
  send: (content: string) => void;
  joinRoom: (id: number) => void;
}

export const useChat = create<ChatState>((set, get) => ({
  rooms: [], messages: {}, activeRoomId: null, unread: {}, ws: null, connected: false,

  loadRooms: async () => {
    try { const { data } = await ChatAPI.rooms(); set({ rooms: data.rooms }); } catch { /* */ }
  },

  loadHistory: async (roomId) => {
    try {
      const { data } = await ChatAPI.messages(roomId);
      set((s) => ({ messages: { ...s.messages, [roomId]: data.messages } }));
    } catch { /* */ }
  },

  connect: () => {
    if (get().ws) return;
    const ws = createChatSocket();
    ws.onopen = () => {
      set({ connected: true });
      const rid = get().activeRoomId;
      if (rid) get().joinRoom(rid);
    };
    ws.onmessage = (ev) => {
      const msg: WSMessage = JSON.parse(ev.data);
      if (msg.type === 'message' && msg.room) {
        const room = get().rooms.find((r) => r.name === msg.room);
        if (!room) return;
        const cm: ChatMessage = { id: Date.now(), room_id: room.id, user: msg.user as ChatUser, content: msg.content || '', created_at: msg.timestamp || new Date().toISOString() };
        set((s) => ({
          messages: { ...s.messages, [room.id]: [...(s.messages[room.id] || []), cm] },
          unread: room.id !== s.activeRoomId ? { ...s.unread, [room.id]: (s.unread[room.id] || 0) + 1 } : s.unread,
        }));
      } else if (msg.type === 'history' && msg.messages) {
        const rid = get().activeRoomId;
        if (!rid) return;
        const h: ChatMessage[] = msg.messages.map((m) => ({ id: m.id, room_id: rid, user: m.user, content: m.content, created_at: m.created_at }));
        set((s) => ({ messages: { ...s.messages, [rid]: h } }));
      }
    };
    ws.onclose = () => { set({ connected: false, ws: null }); setTimeout(() => { if (!get().connected) get().connect(); }, 3000); };
    set({ ws });
  },

  send: (content) => {
    const { ws, connected, activeRoomId, rooms } = get();
    if (!ws || !connected || !activeRoomId) return;
    const room = rooms.find((r) => r.id === activeRoomId);
    if (!room) return;
    ws.send(JSON.stringify({ type: 'chat', room: room.name, content }));
  },

  joinRoom: (id) => {
    const { ws, connected, rooms } = get();
    const prev = get().activeRoomId;
    const room = rooms.find((r) => r.id === id);
    if (!room || !ws || !connected) return;
    if (prev && prev !== id) {
      const prevRoom = rooms.find((r) => r.id === prev);
      if (prevRoom) ws.send(JSON.stringify({ type: 'leave', room: prevRoom.name }));
    }
    ws.send(JSON.stringify({ type: 'join', room: room.name }));
    set({ activeRoomId: id, unread: { ...get().unread, [id]: 0 } });
    get().loadHistory(id);
  },
}));
