import { mount } from '@vue/test-utils'
import { describe, expect, it } from 'vitest'
import AnnualPositionSummary from '~/components/AnnualPositionSummary.vue'
import HelpTooltip from '~/components/HelpTooltip.vue'
import MoneyDisplay from '~/components/MoneyDisplay.vue'
import QuarterStatus from '~/components/QuarterStatus.vue'
import WarningList from '~/components/WarningList.vue'

const global = {
  stubs: {
    UAlert: { template: '<article>{{ title }} {{ description }}<slot /></article>', props: ['title', 'description', 'icon', 'color'] },
    UBadge: { template: '<span><slot /></span>' },
    NuxtLink: { template: '<a><slot /></a>', props: ['to'] }
  }
}

describe('financial presentation', () => {
  it('provides accessible dashboard help on hover and focus', () => {
    const tooltipStub = { name: 'UTooltip', template: '<span class="tooltip">{{ text }}<slot /></span>', props: ['text', 'ui'] }
    const wrapper = mount(HelpTooltip, {
      props: { label: 'Projected wages', text: 'Estimated full-year taxable wages.' },
      global: {
        stubs: {
          UTooltip: tooltipStub,
          UButton: { template: '<button><slot /></button>' }
        }
      }
    })
    expect(wrapper.find('.tooltip').text()).toContain('Estimated full-year taxable wages.')
    expect(wrapper.findComponent(tooltipStub).props('ui')).toEqual({ content: 'h-auto w-max max-w-72', text: 'whitespace-normal break-words' })
    expect(wrapper.get('button').attributes('aria-label')).toBe('About Projected wages')
    expect(wrapper.get('button').attributes('type')).toBe('button')
  })

  it('labels projected monetary values', () => {
    const wrapper = mount(MoneyDisplay, { props: { value: 1250, meaning: 'projected' } })
    expect(wrapper.text()).toContain('$12.50')
    expect(wrapper.text()).toContain('Projected')
  })

  it('renders a textual quarter status', () => {
    const wrapper = mount(QuarterStatus, { props: { quarter: { quarter: 2, status: 'in_progress' } }, global })
    expect(wrapper.text()).toContain('Q2')
    expect(wrapper.text()).toContain('In progress')
  })

  it('renders warning text in addition to severity color', () => {
    const wrapper = mount(WarningList, { props: { warnings: [{ severity: 'caution', code: 'stale_paystub', path: 'paystubs.spouse_1', message: 'The paystub is stale.' }] }, global })
    expect(wrapper.text()).toContain('stale_paystub')
    expect(wrapper.text()).toContain('The paystub is stale.')
  })

  it('groups the annual position by financial role', () => {
    const wrapper = mount(AnnualPositionSummary, {
      props: {
        resource: {
          result: {
            as_of_date: '2026-09-01',
            annual_summary: {
              federal_wages: { projected_annual_cents: 1 }, california_wages: { projected_annual_cents: 1 },
              investments: { ordinary_dividends_cents: 1, qualified_dividends_cents: 1 },
              federal_withholding: { projected_annual_cents: 1 }, california_withholding: { projected_annual_cents: 1 }, warnings: []
            },
            federal: { details: { annual_liability_cents: 1 }, warnings: [] },
            california: { details: { annual_liability_cents: 1 }, warnings: [] },
            current_recommendations: {
              federal: { payments_credited_cents: 1, remaining_before_recommendation_cents: 1 },
              california: { payments_credited_cents: 1, remaining_before_recommendation_cents: 1 }
            }
          }
        } as never
      },
      global: { stubs: { UCard: { template: '<section><slot name="header" /><slot /></section>' }, MetricSummary: { template: '<span>{{ label }}</span>', props: ['label'] }, WarningList: true } }
    })
    expect(wrapper.text()).toContain('Income & investments')
    expect(wrapper.text()).toContain('Covered taxes')
    expect(wrapper.text()).toContain('Projected tax position')
  })
})
