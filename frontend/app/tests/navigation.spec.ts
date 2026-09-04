import { describe, expect, it } from 'vitest'
import { navigationItems } from '~/utils/navigation'

describe('primary navigation', () => {
  it('links to all primary areas and defaults Quarters to Q1', () => {
    expect(navigationItems(undefined).map(item => item.to)).toEqual(['/', '/quarters/1', '/history', '/settings'])
  })

  it('links Quarters to the current bootstrap quarter', () => {
    expect(navigationItems(3)[1]?.to).toBe('/quarters/3')
  })
})
