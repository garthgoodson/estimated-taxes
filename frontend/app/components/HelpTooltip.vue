<script setup lang="ts">
defineProps<{ label: string; text: string }>()
</script>

<template>
  <UTooltip
    :text="text"
    :content="{ side: 'top' }"
    :ui="{ content: 'h-auto w-max max-w-72', text: 'whitespace-normal break-words' }"
  >
    <UButton
      class="help-trigger"
      type="button"
      icon="i-lucide-info"
      size="xs"
      color="neutral"
      variant="ghost"
      square
      :aria-label="`About ${label}`"
    />
  </UTooltip>
</template>

<style scoped>
.help-trigger { color: var(--ui-text-muted); }
</style>
