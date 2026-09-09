import { mount } from '@vue/test-utils'
import { describe, expect, it } from 'vitest'
import GrossPayCalculatorModal from '~/components/GrossPayCalculatorModal.vue'

const global = {
  stubs: {
    UModal: { props: ['open', 'title', 'description'], template: '<section v-if="open"><header><h2>{{ title }}</h2><p>{{ description }}</p></header><slot name="body" /><slot name="footer" /></section>' },
    UAlert: { template: '<p>{{ title }} {{ description }}</p>', props: ['title', 'description', 'color'] },
    MoneyInput: { name: 'MoneyInput', template: '<button class="money" @click="$emit(\'update:modelValue\', 10000)">{{ label }}</button>', props: ['label'] },
    MoneyDisplay: { template: '<span>{{ value }}</span>', props: ['value'] },
    UButton: { template: '<button :disabled="disabled" @click="$emit(\'click\')"><slot /></button>', props: ['disabled'] }
  }
}

function applyButton(wrapper: ReturnType<typeof mount>) {
  return wrapper.findAll('button').find(button => button.text() === 'Apply values')!
}

describe('gross-pay calculator modal', () => {
  it('uses the common modal header structure', () => {
    const wrapper = mount(GrossPayCalculatorModal, { props: { open: true, mode: 'year_to_date' }, global })
    expect(wrapper.find('h2').text()).toBe('Calculate taxable wages from gross pay')
    expect(wrapper.text()).toContain('Derive Federal and California taxable wages from gross pay and payroll deductions.')
  })

  it('explains current-period gross exclusions and emits only the federal result', async () => {
    const wrapper = mount(GrossPayCalculatorModal, { props: { open: true, mode: 'current_period' }, global })
    expect(wrapper.text()).toContain('Exclude bonuses and other nonrecurring pay.')
    await wrapper.findComponent({ name: 'MoneyInput' }).vm.$emit('update:modelValue', 10_001)
    await applyButton(wrapper).trigger('click')
    expect(wrapper.emitted('apply')).toEqual([[{ mode: 'current_period', regular_wages_cents: 10_001 }]])
  })

  it('emits both jurisdiction values for YTD and resets on cancel and reopen', async () => {
    const wrapper = mount(GrossPayCalculatorModal, { props: { open: true, mode: 'year_to_date' }, global })
    const inputs = wrapper.findAllComponents({ name: 'MoneyInput' })
    await inputs[0]!.vm.$emit('update:modelValue', 10_001)
    await inputs[4]!.vm.$emit('update:modelValue', 101)
    await applyButton(wrapper).trigger('click')
    expect(wrapper.emitted('apply')).toEqual([[{
      mode: 'year_to_date', federal_taxable_wages_cents: 9_900, california_taxable_wages_cents: 10_001
    }]])

    await wrapper.setProps({ open: false })
    await wrapper.setProps({ open: true })
    expect(applyButton(wrapper).attributes('disabled')).toBeDefined()
  })

  it('resets when cancelled', async () => {
    const wrapper = mount(GrossPayCalculatorModal, { props: { open: true, mode: 'year_to_date' }, global })
    await wrapper.findComponent({ name: 'MoneyInput' }).vm.$emit('update:modelValue', 10_000)
    expect(applyButton(wrapper).attributes('disabled')).toBeUndefined()
    await wrapper.findAll('button').find(button => button.text() === 'Cancel')!.trigger('click')
    await wrapper.setProps({ open: false })
    await wrapper.setProps({ open: true })
    expect(applyButton(wrapper).attributes('disabled')).toBeDefined()
  })
})
