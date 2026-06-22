import { Suspense } from 'react'
import { Outlet } from 'react-router-dom'
import { ActivityBar } from './ActivityBar'
import { SidebarPanel } from './SidebarPanel'
import { LoadingSkeleton } from '@/components/shared/LoadingSkeleton'
import { ErrorBoundary } from '@/components/shared/ErrorBoundary'

export function AppLayout() {
  return (
    <div className="flex h-screen overflow-hidden bg-background">
      {/* Activity Bar - far left, fixed 48px */}
      <ActivityBar />

      {/* Context-aware Sidebar - 240px */}
      <SidebarPanel />

      {/* Main content area - flex-1 */}
      <main className="flex-1 overflow-hidden">
        <ErrorBoundary>
          <Suspense fallback={<LoadingSkeleton variant="page" />}>
            <div className="h-full overflow-y-auto">
              <Outlet />
            </div>
          </Suspense>
        </ErrorBoundary>
      </main>
    </div>
  )
}
