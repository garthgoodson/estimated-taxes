export function validQuarter(value: string | string[] | undefined): 1 | 2 | 3 | 4 | null {
  const quarter = Number(Array.isArray(value) ? value[0] : value)
  return Number.isInteger(quarter) && quarter >= 1 && quarter <= 4 ? quarter as 1 | 2 | 3 | 4 : null
}
