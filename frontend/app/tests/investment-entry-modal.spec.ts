import { mount } from '@vue/test-utils'
import { describe, expect, it } from 'vitest'
import InvestmentEntryModal from '~/components/InvestmentEntryModal.vue'

const investments = { ordinary_dividends_cents: 100, qualified_dividends_cents: 50, short_term_gain_cents: 0, long_term_gain_cents: 0, federal_withholding_cents: 10, california_withholding_cents: 5, notes: null }
const global = {
  stubs: {
    UModal: { props: ['open', 'title', 'description'], template: '<section v-if="open"><slot name="body" /><slot name="footer" /></section>' },
    UFormField: { template: '<div><slot /></div>' },
    USelect: { name: 'USelect', template: '<button class="type" @click="$emit(\'update:modelValue\', \'qualified_dividends_cents\')"></button>' },
    MoneyInput: { name: 'MoneyInput', template: '<button class="amount" @click="$emit(\'update:modelValue\', -25)"></button>' },
    UAlert: { template: '<p>{{ description }}</p>', props: ['description'] },
    UButton: { template: '<button :disabled="disabled" @click="$emit(\'click\')"><slot /></button>', props: ['disabled'] }
  }
}

function addButton(wrapper: ReturnType<typeof mount>) {
  return wrapper.findAll('button').find(button => button.text() === 'Add')!
}

describe('investment entry modal', () => {
  it('adds a signed reduction to the selected aggregate', async () => {
    const wrapper = mount(InvestmentEntryModal, { props: { open: true, investments }, global })
    await wrapper.find('.amount').trigger('click')
    await addButton(wrapper).trigger('click')
    expect(wrapper.emitted('add')).toEqual([[{ field: 'ordinary_dividends_cents', amount_cents: -25 }]])
  })

  it('rejects qualified dividends above ordinary dividends', async () => {
    const wrapper = mount(InvestmentEntryModal, { props: { open: true, investments: { ...investments, qualified_dividends_cents: 0 } }, global })
    await wrapper.find('.type').trigger('click')
    wrapper.findComponent({ name: 'MoneyInput' }).vm.$emit('update:modelValue', 101)
    await wrapper.vm.$nextTick()
    expect(wrapper.text()).toContain('Qualified dividends cannot exceed ordinary dividends.')
    expect(addButton(wrapper).attributes('disabled')).toBeDefined()
  })

  it('resets on cancel and reopen', async () => {
    const wrapper = mount(InvestmentEntryModal, { props: { open: true, investments }, global })
    await wrapper.find('.amount').trigger('click')
    expect(addButton(wrapper).attributes('disabled')).toBeUndefined()
    await wrapper.findAll('button').find(button => button.text() === 'Cancel')!.trigger('click')
    await wrapper.setProps({ open: false })
    await wrapper.setProps({ open: true })
    expect(addButton(wrapper).attributes('disabled')).toBeDefined()
  })
})
