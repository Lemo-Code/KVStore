import { create } from 'zustand'
import { persist } from 'zustand/middleware'

export type ModuleType = 'redis' | 'chat' | 'ai' | 'settings'
type Theme = 'light' | 'dark' | 'system'

interface UIState {
  activeModule: ModuleType
  theme: Theme
  sidebarWidth: number
  setActiveModule: (m: ModuleType) => void
  setTheme: (t: Theme) => void
}

export const useUIStore = create<UIState>()(
  persist(
    (set) => ({
      activeModule: 'redis',
      theme: 'system',
      sidebarWidth: 260,
      setActiveModule: (activeModule) => set({ activeModule }),
      setTheme: (theme) => set({ theme }),
    }),
    { name: 'learn-redis-ui', partialize: (s) => ({ theme: s.theme }) },
  ),
)
