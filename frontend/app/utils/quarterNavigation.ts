import type { QuarterNumber } from '~/types/quarter'

const quarters: QuarterNumber[] = [1, 2, 3, 4]

export function currentQuarterRoute(currentQuarter: QuarterNumber | undefined): string {
  return `/quarters/${currentQuarter ?? 1}`
}

export function quarterNavigationItems(currentQuarter: QuarterNumber | undefined, selectedQuarter: QuarterNumber) {
  return quarters.map(quarter => ({
    quarter,
    to: `/quarters/${quarter}`,
    selected: quarter === selectedQuarter,
    status: quarter === currentQuarter ? 'Current' : quarter < (currentQuarter ?? 1) ? 'Historic' : 'Upcoming'
  }))
}
