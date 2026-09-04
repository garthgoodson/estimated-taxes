export function confirmDiscard(dirty: boolean, confirm: () => boolean): boolean {
  return !dirty || confirm()
}

export function protectBeforeUnload(dirty: boolean, event: BeforeUnloadEvent): void {
  if (!dirty) return
  event.preventDefault()
  event.returnValue = ''
}
