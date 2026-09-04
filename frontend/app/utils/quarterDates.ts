import type { QuarterNumber } from '~/types/quarter'

const ranges: Record<QuarterNumber, string> = {
  1: 'January 1–March 31, 2026',
  2: 'April 1–June 30, 2026',
  3: 'July 1–September 30, 2026',
  4: 'October 1–December 31, 2026'
}

export function quarterDateRange(quarter: QuarterNumber): string {
  return ranges[quarter]
}
