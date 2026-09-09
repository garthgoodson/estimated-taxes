import { describe, expect, it } from 'vitest'
import { navigationItems } from '~/utils/navigation'
import { currentQuarterRoute, quarterNavigationItems } from '~/utils/quarterNavigation'

describe('primary navigation', () => {
  it('links to all primary areas and defaults Quarters to Q1', () => {
    expect(navigationItems(undefined).map(item => item.to)).toEqual(['/', '/quarters/1', '/history', '/settings'])
  })

  it('links Quarters to the current bootstrap quarter', () => {
    expect(navigationItems(3)[1]?.to).toBe('/quarters/3')
    expect(currentQuarterRoute(4)).toBe('/quarters/4')
  })

  it('provides direct links to every quarter with backend-derived status labels', () => {
    expect(quarterNavigationItems(3, 1)).toEqual([
      { quarter: 1, to: '/quarters/1', selected: true, status: 'Historic' },
      { quarter: 2, to: '/quarters/2', selected: false, status: 'Historic' },
      { quarter: 3, to: '/quarters/3', selected: false, status: 'Current' },
      { quarter: 4, to: '/quarters/4', selected: false, status: 'Upcoming' }
    ])
  })
})
