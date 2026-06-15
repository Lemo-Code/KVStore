import { create } from 'zustand';
import type { User } from '../types';
import { AuthAPI } from '../api';

interface AuthState {
  user: User | null;
  token: string | null;
  loading: boolean;
  login: (u: string, p: string) => Promise<void>;
  register: (u: string, e: string, p: string) => Promise<void>;
  logout: () => void;
  hydrate: () => void;
}

export const useAuth = create<AuthState>((set) => ({
  user: null,
  token: null,
  loading: false,

  login: async (u, p) => {
    set({ loading: true });
    try {
      const { data } = await AuthAPI.login(u, p);
      localStorage.setItem('token', data.token);
      localStorage.setItem('user', JSON.stringify(data.user));
      set({ user: data.user, token: data.token });
    } finally {
      set({ loading: false });
    }
  },

  register: async (u, e, p) => {
    set({ loading: true });
    await AuthAPI.register(u, e, p);
    set({ loading: false });
  },

  logout: () => {
    localStorage.removeItem('token');
    localStorage.removeItem('user');
    set({ user: null, token: null });
  },

  hydrate: () => {
    const t = localStorage.getItem('token');
    const u = localStorage.getItem('user');
    if (t && u) {
      try { set({ token: t, user: JSON.parse(u) }); } catch { logout(); }
    }
  },
}));
