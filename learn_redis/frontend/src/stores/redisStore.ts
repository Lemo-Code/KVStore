import { create } from 'zustand'
import type { RedisConnection, RedisKey, RedisValue, RedisDataType, ServerInfo, SlowLogEntry, PubSubChannel, PubSubMessage, CliCommand } from '@/types/redis'

interface CliHistoryEntry {
  command: string
  result: string
  timestamp: string
}

interface RedisState {
  // Connection management
  connections: RedisConnection[]
  activeConnectionId: string | null
  activeDb: number
  setConnections: (connections: RedisConnection[]) => void
  addConnection: (conn: RedisConnection) => void
  updateConnection: (id: string, updates: Partial<RedisConnection>) => void
  removeConnection: (id: string) => void
  setActiveConnection: (id: string) => void
  setActiveDb: (db: number) => void

  // Key browsing
  keys: RedisKey[]
  selectedKey: string | null
  keyPattern: string
  setKeys: (keys: RedisKey[]) => void
  selectKey: (key: string | null) => void
  setKeyPattern: (pattern: string) => void
  deleteKeys: (keyNames: string[]) => void
  renameKey: (oldName: string, newName: string) => void

  // Data viewing
  currentValue: RedisValue | null
  editingValue: boolean
  setCurrentValue: (value: RedisValue | null) => void
  setEditingValue: (editing: boolean) => void
  updateValue: (key: string, value: Partial<RedisValue>) => void

  // Server monitoring
  serverInfo: ServerInfo | null
  slowLog: SlowLogEntry[]
  setServerInfo: (info: ServerInfo) => void
  setSlowLog: (log: SlowLogEntry[]) => void

  // CLI
  cliHistory: CliHistoryEntry[]
  cliCommands: CliCommand[]
  addCliHistory: (entry: CliHistoryEntry) => void
  clearCliHistory: () => void

  // PubSub
  subscribedChannels: string[]
  pubSubMessages: PubSubMessage[]
  addPubSubMessage: (msg: PubSubMessage) => void
  subscribeChannel: (channel: string) => void
  unsubscribeChannel: (channel: string) => void
  clearPubSubMessages: () => void
}

export const useRedisStore = create<RedisState>()((set, get) => ({
  connections: [],
  activeConnectionId: null,
  activeDb: 0,
  setConnections: (connections) => set({ connections }),
  addConnection: (conn) => set((s) => ({ connections: [...s.connections, conn] })),
  updateConnection: (id, updates) =>
    set((s) => ({
      connections: s.connections.map((c) => (c.id === id ? { ...c, ...updates } : c)),
    })),
  removeConnection: (id) =>
    set((s) => ({
      connections: s.connections.filter((c) => c.id !== id),
      activeConnectionId: s.activeConnectionId === id ? null : s.activeConnectionId,
    })),
  setActiveConnection: (id) => set({ activeConnectionId: id, selectedKey: null, currentValue: null }),
  setActiveDb: (db) => set({ activeDb: db }),

  keys: [],
  selectedKey: null,
  keyPattern: '',
  setKeys: (keys) => set({ keys }),
  selectKey: (key) => set({ selectedKey: key, editingValue: false }),
  setKeyPattern: (pattern) => set({ keyPattern: pattern }),
  deleteKeys: (keyNames) =>
    set((s) => ({
      keys: s.keys.filter((k) => !keyNames.includes(k.name)),
      selectedKey: keyNames.includes(s.selectedKey || '') ? null : s.selectedKey,
      currentValue: keyNames.includes(s.selectedKey || '') ? null : s.currentValue,
    })),
  renameKey: (oldName, newName) =>
    set((s) => ({
      keys: s.keys.map((k) => (k.name === oldName ? { ...k, name: newName } : k)),
      selectedKey: s.selectedKey === oldName ? newName : s.selectedKey,
    })),

  currentValue: null,
  editingValue: false,
  setCurrentValue: (value) => set({ currentValue: value }),
  setEditingValue: (editing) => set({ editingValue: editing }),
  updateValue: (key, value) =>
    set((s) => ({
      currentValue: s.currentValue && s.currentValue ? { ...s.currentValue, ...value } : null,
    })),

  serverInfo: null,
  slowLog: [],
  setServerInfo: (info) => set({ serverInfo: info }),
  setSlowLog: (log) => set({ slowLog: log }),

  cliHistory: [],
  cliCommands: [],
  addCliHistory: (entry) =>
    set((s) => ({ cliHistory: [...s.cliHistory, entry] })),
  clearCliHistory: () => set({ cliHistory: [] }),

  subscribedChannels: [],
  pubSubMessages: [],
  addPubSubMessage: (msg) =>
    set((s) => ({ pubSubMessages: [...s.pubSubMessages.slice(-99), msg] })),
  subscribeChannel: (channel) =>
    set((s) => ({
      subscribedChannels: s.subscribedChannels.includes(channel)
        ? s.subscribedChannels
        : [...s.subscribedChannels, channel],
    })),
  unsubscribeChannel: (channel) =>
    set((s) => ({
      subscribedChannels: s.subscribedChannels.filter((c) => c !== channel),
    })),
  clearPubSubMessages: () => set({ pubSubMessages: [] }),
}))
