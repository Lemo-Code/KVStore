import { cn } from '@/lib/utils'
import { AnimatePresence, motion } from 'framer-motion'
import { ChevronDown } from 'lucide-react'
import { createContext, useContext, useEffect, useRef, useState, type ReactNode } from 'react'

interface DropdownContextType {
  isOpen: boolean
  setIsOpen: (open: boolean) => void
  toggle: () => void
}

const DropdownContext = createContext<DropdownContextType | null>(null)

function useDropdown() {
  const context = useContext(DropdownContext)
  if (!context) throw new Error('Dropdown components must be used within DropdownMenu')
  return context
}

interface DropdownMenuProps {
  children: ReactNode
  className?: string
  defaultOpen?: boolean
}

export function DropdownMenu({ children, className, defaultOpen = false }: DropdownMenuProps) {
  const [isOpen, setIsOpen] = useState(defaultOpen)
  const containerRef = useRef<HTMLDivElement>(null)

  useEffect(() => {
    function handleClickOutside(event: MouseEvent) {
      if (containerRef.current && !containerRef.current.contains(event.target as Node)) {
        setIsOpen(false)
      }
    }
    document.addEventListener('mousedown', handleClickOutside)
    return () => document.removeEventListener('mousedown', handleClickOutside)
  }, [])

  return (
    <DropdownContext.Provider value={{ isOpen, setIsOpen, toggle: () => setIsOpen(!isOpen) }}>
      <div ref={containerRef} className={cn('relative inline-block', className)}>
        {children}
      </div>
    </DropdownContext.Provider>
  )
}

interface DropdownTriggerProps {
  children: ReactNode
  className?: string
  asChild?: boolean
}

export function DropdownTrigger({ children, className, asChild }: DropdownTriggerProps) {
  const { toggle, isOpen } = useDropdown()

  if (asChild) {
    return (
      <div onClick={toggle} className={className}>
        {children}
      </div>
    )
  }

  return (
    <button
      onClick={toggle}
      className={cn(
        'flex items-center gap-1.5 rounded-lg px-3 py-2 text-sm font-medium',
        'bg-surface-2 hover:bg-surface-3 border border-border-subtle transition-all',
        isOpen && 'bg-surface-3 border-border',
        className
      )}
    >
      {children}
      <ChevronDown
        size={14}
        className={cn('text-text-muted transition-transform', isOpen && 'rotate-180')}
      />
    </button>
  )
}

interface DropdownContentProps {
  children: ReactNode
  className?: string
  align?: 'start' | 'center' | 'end'
  width?: 'auto' | 'trigger' | 'sm' | 'md' | 'lg'
}

const widthClasses = {
  auto: 'w-auto',
  trigger: 'w-full min-w-[140px]',
  sm: 'w-40',
  md: 'w-56',
  lg: 'w-72',
}

const alignClasses = {
  start: 'left-0',
  center: 'left-1/2 -translate-x-1/2',
  end: 'right-0',
}

export function DropdownContent({
  children,
  className,
  align = 'start',
  width = 'trigger',
}: DropdownContentProps) {
  const { isOpen } = useDropdown()

  return (
    <AnimatePresence>
      {isOpen && (
        <motion.div
          initial={{ opacity: 0, y: -8, scale: 0.98 }}
          animate={{ opacity: 1, y: 0, scale: 1 }}
          exit={{ opacity: 0, y: -8, scale: 0.98 }}
          transition={{ duration: 0.15, ease: 'easeOut' }}
          className={cn(
            'absolute z-50 top-full mt-1',
            'rounded-xl border border-border bg-surface-1 shadow-xl',
            'py-1.5',
            widthClasses[width],
            alignClasses[align],
            className
          )}
        >
          {children}
        </motion.div>
      )}
    </AnimatePresence>
  )
}

interface DropdownItemProps {
  children: ReactNode
  className?: string
  onClick?: () => void
  disabled?: boolean
  active?: boolean
  destructive?: boolean
  icon?: React.ReactNode
}

export function DropdownItem({
  children,
  className,
  onClick,
  disabled,
  active,
  destructive,
  icon,
}: DropdownItemProps) {
  const { setIsOpen } = useDropdown()

  const handleClick = () => {
    if (disabled) return
    onClick?.()
    setIsOpen(false)
  }

  return (
    <button
      onClick={handleClick}
      disabled={disabled}
      className={cn(
        'w-full flex items-center gap-2.5 px-3 py-2 text-xs text-text-secondary',
        'hover:bg-surface-2 hover:text-text-primary transition-colors',
        'disabled:opacity-50 disabled:pointer-events-none',
        active && 'bg-accent-red/10 text-accent-red',
        destructive && 'text-danger hover:bg-danger/10',
        className
      )}
    >
      {icon && <span className="text-text-muted">{icon}</span>}
      {children}
    </button>
  )
}

interface DropdownGroupProps {
  children: ReactNode
  label?: string
  className?: string
}

export function DropdownGroup({ children, label, className }: DropdownGroupProps) {
  return (
    <div className={cn('py-1', className)}>
      {label && (
        <div className="px-3 py-1.5 text-[10px] font-semibold text-text-muted uppercase tracking-wider">
          {label}
        </div>
      )}
      {children}
    </div>
  )
}

export function DropdownSeparator() {
  return <div className="my-1.5 h-px bg-border-subtle" />
}

interface DropdownCheckboxItemProps {
  children: ReactNode
  checked: boolean
  onCheckedChange: (checked: boolean) => void
  className?: string
  disabled?: boolean
}

export function DropdownCheckboxItem({
  children,
  checked,
  onCheckedChange,
  className,
  disabled,
}: DropdownCheckboxItemProps) {
  const { setIsOpen } = useDropdown()

  const handleClick = () => {
    if (disabled) return
    onCheckedChange(!checked)
    setIsOpen(false)
  }

  return (
    <button
      onClick={handleClick}
      disabled={disabled}
      className={cn(
        'w-full flex items-center justify-between px-3 py-2 text-xs text-text-secondary',
        'hover:bg-surface-2 hover:text-text-primary transition-colors',
        'disabled:opacity-50 disabled:pointer-events-none',
        className
      )}
    >
      <span>{children}</span>
      <div
        className={cn(
          'w-4 h-4 rounded border flex items-center justify-center transition-colors',
          checked
            ? 'bg-accent-red border-accent-red'
            : 'border-border-subtle bg-surface-0'
        )}
      >
        {checked && (
          <svg className="w-3 h-3 text-white" fill="none" viewBox="0 0 24 24" stroke="currentColor">
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={3} d="M5 13l4 4L19 7" />
          </svg>
        )}
      </div>
    </button>
  )
}
