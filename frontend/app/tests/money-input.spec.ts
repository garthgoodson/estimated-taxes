import { mount } from '@vue/test-utils'
import { describe, expect, it } from 'vitest'
import MoneyInput from '~/components/MoneyInput.vue'

const global = {
  stubs: {
    UFormField: { template: '<label><slot /><span class="error">{{ error }}</span></label>', props: ['label', 'error'] },
    UInput: { template: '<input :value="modelValue" @input="$emit(\'update:modelValue\', $event.target.value)">', props: ['modelValue'] }
  }
}

describe('MoneyInput', () => {
  it('preserves intermediate text and commits cents only on blur', async () => {
    const wrapper = mount(MoneyInput, { props: { modelValue: null, label: 'Amount' }, global })
    const input = wrapper.find('input')
    await input.setValue('12.')
    expect(input.element.value).toBe('12.')
    expect(wrapper.emitted('update:modelValue')).toBeUndefined()
    await input.trigger('blur')
    expect(wrapper.emitted('update:modelValue')).toEqual([[1200]])
  })

  it('shows invalid text without replacing it with null', async () => {
    const wrapper = mount(MoneyInput, { props: { modelValue: 0, label: 'Amount' }, global })
    const input = wrapper.find('input')
    await input.setValue('1.234')
    await input.trigger('blur')
    expect(wrapper.find('.error').text()).toContain('valid dollar amount')
    expect(wrapper.emitted('update:modelValue')).toBeUndefined()
    expect(input.element.value).toBe('1.234')
  })
})
