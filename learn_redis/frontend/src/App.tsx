import { Component, lazy, Suspense, useEffect, type ReactNode } from 'react'
import { Routes, Route, Navigate } from 'react-router-dom'
import { useUIStore } from '@/stores/uiStore'
import { useAuthStore } from '@/stores/authStore'

const LoginPage = lazy(() => import('@/pages/LoginPage'))
const RegisterPage = lazy(() => import('@/pages/RegisterPage'))
const ForgotPasswordPage = lazy(() => import('@/pages/ForgotPasswordPage'))
const AppMain = lazy(() => import('@/pages/AppMain'))

class RootErrorBoundary extends Component<{ children: ReactNode }, { err: Error | null }> {
  state = { err: null as Error | null }
  static getDerivedStateFromError(err: Error) { return { err } }
  render() {
    if (this.state.err) return <div style={{padding:40,color:'red',fontFamily:'monospace',fontSize:14}}><h2>App Error</h2><pre>{this.state.err.message}</pre><pre>{this.state.err.stack}</pre></div>
    return this.props.children
  }
}

export default function App() {
  const theme = useUIStore((s) => s.theme)
  const isAuth = useAuthStore((s) => s.isAuthenticated)

  useEffect(() => {
    const root = document.documentElement
    root.classList.remove('light', 'dark')
    root.classList.add(theme === 'system'
      ? (window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light')
      : theme)
  }, [theme])

  return (
    <RootErrorBoundary>
      <Suspense fallback={<div style={{display:'flex',alignItems:'center',justifyContent:'center',height:'100vh',color:'#666',fontSize:14,fontFamily:'Inter,sans-serif'}}>Loading...</div>}>
        <Routes>
          <Route path="/login" element={isAuth ? <Navigate to="/" replace /> : <LoginPage />} />
          <Route path="/register" element={isAuth ? <Navigate to="/" replace /> : <RegisterPage />} />
          <Route path="/forgot-password" element={isAuth ? <Navigate to="/" replace /> : <ForgotPasswordPage />} />
          <Route path="/*" element={isAuth ? <AppMain /> : <Navigate to="/login" replace />} />
        </Routes>
      </Suspense>
    </RootErrorBoundary>
  )
}
