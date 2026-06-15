import { cn } from '@/lib/utils'
import { useState, useRef, useEffect } from 'react'

interface TooltipProps {
  children: React.ReactNode
  content: React.ReactNode
  placement?: 'top' | 'bottom' | 'left' | 'right'
  delay?: number
  className?: string
}

export function Tooltip({
  children,
  content,
  placement = 'top',
  delay = 200,
  className,
}: TooltipProps) {
  const [isVisible, setIsVisible] = useState(false)
  const [isMounted, setIsMounted] = useState(false)
  const timeoutRef = useRef<ReturnType<typeof setTimeout> | null>(null)

  useEffect(() => {
    return () => {
      if (timeoutRef.current) clearTimeout(timeoutRef.current)
    }
  }, [])

  const handleMouseEnter = () => {
    timeoutRef.current = setTimeout(() => {
      setIsMounted(true)
      requestAnimationFrame(() => setIsVisible(true))
    }, delay)
  }

  const handleMouseLeave = () => {
    if (timeoutRef.current) clearTimeout(timeoutRef.current)
    setIsVisible(false)
    setTimeout(() => setIsMounted(false), 150)
  }

  const placementClasses = {
    top: 'bottom-full left-1/2 -translate-x-1/2 mb-2',
    bottom: 'top-full left-1/2 -translate-x-1/2 mt-2',
    left: 'right-full top-1/2 -translate-y-1/2 mr-2',
    right: 'left-full top-1/2 -translate-y-1/2 ml-2',
  }

  const arrowClasses = {
    top: 'top-full left-1/2 -translate-x-1/2 -mt-1 border-l-transparent border-r-transparent border-b-transparent border-t-surface-3',
    bottom: 'bottom-full left-1/2 -translate-x-1/2 -mb-1 border-l-transparent border-r-transparent border-t-transparent border-b-surface-3',
    left: 'left-full top-1/2 -translate-y-1/2 -ml-1 border-t-transparent border-b-transparent border-r-transparent border-l-surface-3',
    right: 'right-full top-1/2 -translate-y-1/2 -mr-1 border-t-transparent border-b-transparent border-l-transparent border-r-surface-3',
  }

  return (
    <div
      className="relative inline-flex"
      onMouseEnter={handleMouseEnter}
      onMouseLeave={handleMouseLeave}
    >
      {children}
      {isMounted && (
        <div
          className={cn(
            'absolute z-50 px-2.5 py-1.5 rounded-md bg-surface-3 border border-border text-xs text-text-primary whitespace-nowrap shadow-lg shadow-black/20',
            'transition-all duration-150 pointer-events-none',
            placementClasses[placement],
            isVisible ? 'opacity-100 scale-100' : 'opacity-0 scale-95',
            className,
          )}
        >
          {content}
          <div
            className={cn(
              'absolute w-0 h-0 border-4',
              arrowClasses[placement],
            )}
          />
        </div>
      )}
    </div>
  )
}

// TooltipProvider for simpler usage
interface SimpleTooltipProps {
  text: string
  children: React.ReactNode
  placement?: 'top' | 'bottom' | 'left' | 'right'
}

export function SimpleTooltip({ text, children, placement = 'top' }: SimpleTooltipProps) {
  return (
    <Tooltip content={text} placement={placement}>
      {children}
    </Tooltip>
  )
}

// Icon button wrapped in a tooltip — handy for compact action affordances.
interface TooltipIconProps {
  icon: React.ElementType
  content: React.ReactNode
  placement?: 'top' | 'bottom' | 'left' | 'right'
  size?: number
  onClick?: () => void
  className?: string
}

export function TooltipIcon({
  icon: Icon,
  content,
  placement = 'top',
  size = 14,
  onClick,
  className,
}: TooltipIconProps) {
  return (
    <Tooltip content={content} placement={placement}>
      <button
        type="button"
        onClick={onClick}
        className={cn(
          'flex h-7 w-7 items-center justify-center rounded-md text-text-muted',
          'hover:text-text-secondary hover:bg-surface-3 transition-colors',
          className,
        )}
      >
        <Icon size={size} />
      </button>
    </Tooltip>
  )
}
