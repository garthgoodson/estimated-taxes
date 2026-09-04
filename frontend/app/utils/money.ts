import type { Cents } from '~/types/api'

const formatter = new Intl.NumberFormat('en-US', {
  style: 'currency',
  currency: 'USD',
  minimumFractionDigits: 2,
  maximumFractionDigits: 2
})

export function formatCents(value: Cents | null | undefined): string {
  return value == null ? '—' : formatter.format(value / 100)
}

export function parseDollars(value: string, allowNegative = false): Cents | null {
  const text = value.trim()
  if (!text) return null
  if (!/^-?\d+(?:\.\d{0,2})?$/.test(text)) return null
  const negative = text.startsWith('-')
  if (negative && !allowNegative) return null
  const [whole, fraction = ''] = (negative ? text.slice(1) : text).split('.')
  const cents = Number(whole) * 100 + Number((fraction + '00').slice(0, 2))
  return negative ? -cents : cents
}

export function formatCentsInput(value: Cents | null): string {
  if (value == null) return ''
  const sign = value < 0 ? '-' : ''
  const absolute = Math.abs(value)
  return `${sign}${Math.floor(absolute / 100)}.${String(absolute % 100).padStart(2, '0')}`
}
