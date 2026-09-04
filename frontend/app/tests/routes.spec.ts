import { describe, expect, it } from 'vitest'
import { validQuarter } from '~/utils/routes'

describe('quarter routes', () => {
  it('accepts only Q1 through Q4', () => {
    expect(validQuarter('1')).toBe(1)
    expect(validQuarter('4')).toBe(4)
    expect(validQuarter('0')).toBeNull()
    expect(validQuarter('5')).toBeNull()
    expect(validQuarter('Q1')).toBeNull()
  })
})
