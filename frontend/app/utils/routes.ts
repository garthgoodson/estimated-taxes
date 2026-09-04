export function validQuarter(value: string | string[] | undefined): 1 | 2 | 3 | 4 | null {
  if (typeof value !== 'string' || !/^[1-4]$/.test(value)) return null
  return Number(value) as 1 | 2 | 3 | 4
}
