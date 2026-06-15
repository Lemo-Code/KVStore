import { create } from 'zustand'
import { persist } from 'zustand/middleware'

export type Theme = 'dark' | 'light' | 'system'

interface SettingsState {
  theme: Theme
  editorFontSize: number
  reduceMotion: boolean
  autoConnect: boolean
  autoSaveHistory: boolean
  setTheme: (theme: Theme) => void
  setEditorFontSize: (size: number) => void
  setReduceMotion: (v: boolean) => void
  setAutoConnect: (v: boolean) => void
  setAutoSaveHistory: (v: boolean) => void
}

export const useSettingsStore = create<SettingsState>()(
  persist(
    (set) => ({
      theme: 'dark',
      editorFontSize: 13,
      reduceMotion: false,
      autoConnect: true,
      autoSaveHistory: true,
      setTheme: (theme) => set({ theme }),
      setEditorFontSize: (editorFontSize) => set({ editorFontSize }),
      setReduceMotion: (reduceMotion) => set({ reduceMotion }),
      setAutoConnect: (autoConnect) => set({ autoConnect }),
      setAutoSaveHistory: (autoSaveHistory) => set({ autoSaveHistory }),
    }),
    {
      name: 'redis-lab-settings',
    },
  ),
)
