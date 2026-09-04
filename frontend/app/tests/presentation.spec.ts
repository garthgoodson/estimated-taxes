import { mount } from '@vue/test-utils'
import { describe, expect, it } from 'vitest'
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
})
