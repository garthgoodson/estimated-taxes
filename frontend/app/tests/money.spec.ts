import { describe, expect, it } from 'vitest'
import { formatCents, formatCentsInput, parseDollars } from '~/utils/money'

describe('money formatting', () => {
  it('preserves cents and nullable values', () => {
    expect(formatCents(123)).toBe('$1.23')
    expect(formatCents(-123)).toBe('-$1.23')
    expect(formatCents(null)).toBe('—')
    expect(formatCentsInput(0)).toBe('0.00')
    expect(formatCentsInput(null)).toBe('')
  })

  it('distinguishes an empty amount from explicit zero', () => {
    expect(parseDollars('')).toBeNull()
    expect(parseDollars('0')).toBe(0)
    expect(parseDollars('12.3')).toBe(1230)
    expect(parseDollars('-1.25')).toBeNull()
    expect(parseDollars('-1.25', true)).toBe(-125)
    expect(parseDollars('1.234')).toBeNull()
  })
})
