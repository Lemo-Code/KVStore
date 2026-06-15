import { useAuthStore } from '@/stores/authStore'
import { Navigate, useLocation } from 'react-router-dom'

export function ProtectedRoute({ children }: { children: React.ReactNode }) {
  const isAuthenticated = useAuthStore((s) => s.isAuthenticated)
  const location = useLocation()

  if (!isAuthenticated) {
    return <Navigate to="/login" state={{ from: location }} replace />
  }

  return <>{children}</>
}

export function GuestRoute({ children }: { children: React.ReactNode }) {
  const isAuthenticated = useAuthStore((s) => s.isAuthenticated)
  const hasOnboarded = useAuthStore((s) => s.hasOnboarded)

  if (isAuthenticated) {
    return <Navigate to={hasOnboarded ? '/dashboard' : '/onboarding'} replace />
  }

  return <>{children}</>
}

export function OnboardingRoute({ children }: { children: React.ReactNode }) {
  const isAuthenticated = useAuthStore((s) => s.isAuthenticated)
  const hasOnboarded = useAuthStore((s) => s.hasOnboarded)

  if (!isAuthenticated) return <Navigate to="/login" replace />
  if (hasOnboarded) return <Navigate to="/dashboard" replace />

  return <>{children}</>
}
