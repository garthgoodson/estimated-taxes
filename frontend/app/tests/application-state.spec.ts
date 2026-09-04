import { mount } from '@vue/test-utils'
import { describe, expect, it } from 'vitest'
import ApplicationState from '~/components/ApplicationState.vue'

const global = {
  stubs: {
    UAlert: { template: '<article>{{ title }} {{ description }}<slot /></article>', props: ['title', 'description'] },
    UEmpty: { template: '<article>{{ title }} {{ description }}</article>', props: ['title', 'description'] },
    UIcon: true
  }
}

describe('application state presentation', () => {
  it('renders loading, success, and failure states', () => {
    const loading = mount(ApplicationState, { props: { pending: true }, global })
    expect(loading.text()).toContain('Loading 2026 tax estimate')

    const success = mount(ApplicationState, { props: {}, slots: { default: '<p>Estimate loaded</p>' }, global })
    expect(success.text()).toContain('Estimate loaded')

    const failure = mount(ApplicationState, { props: { error: { kind: 'transport', code: 'offline', message: 'Offline.', fields: [] } }, global })
    expect(failure.text()).toContain('Unable to load the estimate')
    expect(failure.text()).toContain('Offline.')
  })
})
