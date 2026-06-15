import { RouterProvider } from 'react-router-dom'
import { router } from '@/router'
import { ToastProvider } from '@/components/ui/Toast'
import { useSettingsStore } from '@/stores/settingsStore'
import { MotionConfig } from 'framer-motion'
import { useEffect } from 'react'

function applyTheme(theme: 'dark' | 'light' | 'system') {
  const resolved =
    theme === 'system'
      ? window.matchMedia('(prefers-color-scheme: light)').matches
        ? 'light'
        : 'dark'
      : theme
  document.documentElement.classList.toggle('light', resolved === 'light')
  document.documentElement.classList.toggle('dark', resolved === 'dark')
}

export default function App() {
  const theme = useSettingsStore((s) => s.theme)
  const reduceMotion = useSettingsStore((s) => s.reduceMotion)

  useEffect(() => {
    applyTheme(theme)
    if (theme !== 'system') return
    const media = window.matchMedia('(prefers-color-scheme: light)')
    const handler = () => applyTheme('system')
    media.addEventListener('change', handler)
    return () => media.removeEventListener('change', handler)
  }, [theme])

  return (
    <MotionConfig reducedMotion={reduceMotion ? 'always' : 'never'}>
      <ToastProvider>
        <RouterProvider router={router} />
      </ToastProvider>
    </MotionConfig>
  )
}
