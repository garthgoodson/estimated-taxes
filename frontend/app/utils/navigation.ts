import type { BootstrapState } from '~/types/api'

export function navigationItems(currentQuarter: BootstrapState['current_quarter'] | undefined) {
  return [
    { label: 'Home', icon: 'i-lucide-house', to: '/' },
    { label: 'Quarters', icon: 'i-lucide-calendar-days', to: `/quarters/${currentQuarter ?? 1}` },
    { label: 'History', icon: 'i-lucide-history', to: '/history' },
    { label: 'Settings', icon: 'i-lucide-settings', to: '/settings' }
  ]
}
