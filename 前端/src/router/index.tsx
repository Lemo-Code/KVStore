import { GuestRoute, OnboardingRoute, ProtectedRoute } from '@/components/auth/ProtectedRoute'
import { MainLayout } from '@/components/layout/MainLayout'
import ForgotPasswordPage from '@/pages/auth/ForgotPasswordPage'
import LoginPage from '@/pages/auth/LoginPage'
import RegisterPage from '@/pages/auth/RegisterPage'
import ResetPasswordPage from '@/pages/auth/ResetPasswordPage'
import VerifyEmailPage from '@/pages/auth/VerifyEmailPage'
import LandingPage from '@/pages/landing/LandingPage'
import NotFoundPage from '@/pages/NotFoundPage'
import OnboardingPage from '@/pages/onboarding/OnboardingPage'
import { lazy } from 'react'
import { createBrowserRouter } from 'react-router-dom'

// Authenticated app pages are code-split so the heavy Monaco/workspace bundle
// is only loaded once the user reaches it.
const DashboardPage = lazy(() => import('@/pages/dashboard/DashboardPage'))
const WorkspacePage = lazy(() => import('@/pages/workspace/WorkspacePage'))
const ConnectionsPage = lazy(() => import('@/pages/connections/ConnectionsPage'))
const ChatPage = lazy(() => import('@/pages/chat/ChatPage'))
const AIPage = lazy(() => import('@/pages/ai/AIPage'))
const LearningPage = lazy(() => import('@/pages/learning/LearningPage'))
const CourseDetailPage = lazy(() => import('@/pages/learning/CourseDetailPage'))
const ExercisesPage = lazy(() => import('@/pages/exercises/ExercisesPage'))
const ExerciseDetailPage = lazy(() => import('@/pages/exercises/ExerciseDetailPage'))
const ProfilePage = lazy(() => import('@/pages/profile/ProfilePage'))
const SettingsPage = lazy(() => import('@/pages/settings/SettingsPage'))
const NotificationsPage = lazy(() => import('@/pages/notifications/NotificationsPage'))
const HelpPage = lazy(() => import('@/pages/help/HelpPage'))
const AdminPage = lazy(() => import('@/pages/admin/AdminPage'))

export const router = createBrowserRouter([
  // Public
  { path: '/', element: <LandingPage /> },
  { path: '/login', element: <GuestRoute><LoginPage /></GuestRoute> },
  { path: '/register', element: <GuestRoute><RegisterPage /></GuestRoute> },
  { path: '/forgot-password', element: <GuestRoute><ForgotPasswordPage /></GuestRoute> },
  { path: '/reset-password', element: <GuestRoute><ResetPasswordPage /></GuestRoute> },

  // Auth flow
  {
    path: '/verify-email',
    element: (
      <ProtectedRoute>
        <VerifyEmailPage />
      </ProtectedRoute>
    ),
  },
  {
    path: '/onboarding',
    element: (
      <OnboardingRoute>
        <OnboardingPage />
      </OnboardingRoute>
    ),
  },

  // App (authenticated)
  {
    element: (
      <ProtectedRoute>
        <MainLayout />
      </ProtectedRoute>
    ),
    children: [
      { path: '/dashboard', element: <DashboardPage /> },
      { path: '/workspace', element: <WorkspacePage /> },
      { path: '/connections', element: <ConnectionsPage /> },
      { path: '/chat', element: <ChatPage /> },
      { path: '/ai', element: <AIPage /> },
      { path: '/learning', element: <LearningPage /> },
      { path: '/learning/:courseId', element: <CourseDetailPage /> },
      { path: '/exercises', element: <ExercisesPage /> },
      { path: '/exercises/:exerciseId', element: <ExerciseDetailPage /> },
      { path: '/profile', element: <ProfilePage /> },
      { path: '/settings', element: <SettingsPage /> },
      { path: '/notifications', element: <NotificationsPage /> },
      { path: '/help', element: <HelpPage /> },
      { path: '/admin', element: <AdminPage /> },
    ],
  },

  // Fallback
  { path: '*', element: <NotFoundPage /> },
])
