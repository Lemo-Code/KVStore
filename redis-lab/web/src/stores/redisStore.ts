import { create } from 'zustand';
import type { Connection, QuotaStatus, CommandResult, KeyDetail } from '../types';
import { ConnAPI, RedisAPI, QuotaAPI } from '../api';

interface RedisState {
  conns: Connection[];
  activeConnId: number | null;
  quota: QuotaStatus | null;
  keys: string[];
  keyDetail: KeyDetail | null;
  selectedKey: string | null;
  results: Record<number, CommandResult>; // tabIdx -> result
  history: string[];
  executing: boolean;

  loadConns: () => Promise<void>;
  createConn: (d: { name: string; host: string; port: number; password?: string }) => Promise<Connection>;
  updateConn: (id: number, d: Partial<Connection>) => Promise<void>;
  removeConn: (id: number) => Promise<void>;
  testConn: (id: number) => Promise<{ success: boolean; message: string }>;
  setActiveConn: (id: number | null) => void;
  getActiveConn: () => Connection | undefined;
  loadQuota: () => Promise<void>;
  loadKeys: (pattern?: string) => Promise<void>;
  loadKeyDetail: (key: string) => Promise<void>;
  deleteKey: (key: string) => Promise<void>;
  flush: () => Promise<void>;
  exec: (cmd: string, tabIdx?: number) => Promise<CommandResult>;
}

export const useRedis = create<RedisState>((set, get) => ({
  conns: [],
  activeConnId: null,
  quota: null,
  keys: [],
  keyDetail: null,
  selectedKey: null,
  results: {},
  history: [],
  executing: false,

  loadConns: async () => {
    try {
      const { data } = await ConnAPI.list();
      set({ conns: data.connections });
      if (data.connections.length > 0 && !get().activeConnId) {
        set({ activeConnId: data.connections[0].id });
      }
    } catch { /* */ }
  },

  createConn: async (d) => {
    const { data } = await ConnAPI.create(d);
    set((s) => ({ conns: [...s.conns, data.connection] }));
    if (!get().activeConnId) set({ activeConnId: data.connection.id });
    return data.connection;
  },

  updateConn: async (id, d) => {
    await ConnAPI.update(id, d);
    get().loadConns();
  },

  removeConn: async (id) => {
    await ConnAPI.remove(id);
    set((s) => ({ conns: s.conns.filter((c) => c.id !== id), activeConnId: s.activeConnId === id ? null : s.activeConnId }));
  },

  testConn: async (id) => {
    const { data } = await ConnAPI.test(id);
    return data;
  },

  setActiveConn: (id) => set({ activeConnId: id, keys: [], keyDetail: null, selectedKey: null }),

  getActiveConn: () => get().conns.find((c) => c.id === get().activeConnId),

  loadQuota: async () => {
    try { const { data } = await QuotaAPI.get(); set({ quota: data.quota }); } catch { /* */ }
  },

  loadKeys: async (pattern) => {
    const id = get().activeConnId;
    if (!id) return;
    try { const { data } = await RedisAPI.keys(id, pattern || '*'); set({ keys: data.keys || [] }); } catch { /* */ }
  },

  loadKeyDetail: async (key) => {
    const id = get().activeConnId;
    if (!id) return;
    set({ selectedKey: key });
    try { const { data } = await RedisAPI.keyDetail(id, key); set({ keyDetail: data.detail }); } catch { set({ keyDetail: null }); }
  },

  deleteKey: async (key) => {
    const id = get().activeConnId;
    if (!id) return;
    await RedisAPI.deleteKey(id, key);
    set({ keyDetail: null, selectedKey: null });
    get().loadKeys();
  },

  flush: async () => {
    const id = get().activeConnId;
    if (!id) return;
    await RedisAPI.flush(id);
    set({ keys: [], keyDetail: null });
  },

  exec: async (cmd, tabIdx = 0) => {
    const id = get().activeConnId;
    if (!id) throw new Error('No connection');

    set({ executing: true });
    try {
      const { data } = await RedisAPI.exec(id, cmd);
      set((s) => ({
        results: { ...s.results, [tabIdx]: data.result },
        history: [cmd, ...s.history.filter((h) => h !== cmd)].slice(0, 200),
      }));
      // Refresh keys after mutations
      if (/^(SET|DEL|MSET|FLUSH|RENAME)/i.test(cmd.trim())) get().loadKeys();
      return data.result;
    } finally {
      set({ executing: false });
    }
  },
}));
