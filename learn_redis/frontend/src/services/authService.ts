import { api } from './api'
import type { User } from '@/types/chat'

interface LoginResponse {
  user: User
  access_token: string
  refresh_token: string
}

export const authService = {
  login: (username: string, password: string) =>
    api.post<LoginResponse>('/auth/login', { username, password }),

  register: (username: string, email: string, password: string) =>
    api.post<LoginResponse>('/auth/register', { username, email, password }),

  refresh: (refreshToken: string) =>
    api.post<LoginResponse>('/auth/refresh', { refresh_token: refreshToken }),

  getMe: () => api.get<User>('/users/me'),

  updateMe: (data: Partial<User>) => api.put<User>('/users/me', data),
}
