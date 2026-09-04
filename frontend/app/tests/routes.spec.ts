import { describe, expect, it } from 'vitest'
import { validQuarter } from '~/utils/routes'
import { quarterDateRange } from '~/utils/quarterDates'

describe('quarter routes', () => {
  it('accepts only Q1 through Q4', () => {
    expect(validQuarter('1')).toBe(1)
    expect(validQuarter('4')).toBe(4)
    expect(validQuarter('0')).toBeNull()
    expect(validQuarter('5')).toBeNull()
    expect(validQuarter('Q1')).toBeNull()
    expect(validQuarter('1.0')).toBeNull()
    expect(validQuarter(['1', '2'])).toBeNull()
  })

  it('uses fixed 2026 ranges for every shared quarter route', () => {
    expect([1, 2, 3, 4].map(value => quarterDateRange(value as 1 | 2 | 3 | 4))).toEqual([
      'January 1–March 31, 2026', 'April 1–June 30, 2026', 'July 1–September 30, 2026', 'October 1–December 31, 2026'
    ])
  })
})
