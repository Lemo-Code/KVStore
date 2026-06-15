import { create } from 'zustand'
import { persist } from 'zustand/middleware'

export interface User {
  id: string
  email: string
  username: string
  avatar: string
  role: 'student' | 'mentor' | 'admin'
  level: string
  joinDate: string
  bio?: string
}

interface AuthState {
  isAuthenticated: boolean
  hasOnboarded: boolean
  user: User | null
  login: (email: string, password: string) => Promise<boolean>
  register: (data: { email: string; username: string; password: string }) => Promise<boolean>
  logout: () => void
  completeOnboarding: () => void
  updateUser: (partial: Partial<User>) => void
}

const mockUser: User = {
  id: 'u-001',
  email: 'learner@redis.lab',
  username: 'redis_learner',
  avatar: 'RL',
  role: 'student',
  level: '进阶学员',
  joinDate: '2025-03-15',
  bio: '正在学习 Redis 数据结构与应用实践',
}

export const useAuthStore = create<AuthState>()(
  persist(
    (set) => ({
      isAuthenticated: false,
      hasOnboarded: false,
      user: null,

      login: async (email, _password) => {
        await new Promise((r) => setTimeout(r, 800))
        set({
          isAuthenticated: true,
          user: { ...mockUser, email, username: email.split('@')[0] },
        })
        return true
      },

      register: async ({ email, username, password: _password }) => {
        await new Promise((r) => setTimeout(r, 1000))
        set({
          isAuthenticated: true,
          hasOnboarded: false,
          user: { ...mockUser, email, username },
        })
        return true
      },

      logout: () =>
        set({ isAuthenticated: false, user: null, hasOnboarded: false }),

      completeOnboarding: () => set({ hasOnboarded: true }),

      updateUser: (partial) =>
        set((s) => ({
          user: s.user ? { ...s.user, ...partial } : null,
        })),
    }),
    { name: 'redis-lab-auth' },
  ),
)
