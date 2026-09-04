<script setup lang="ts">
import { formatCents } from '~/utils/money'
import type { Cents } from '~/types/api'

const props = withDefaults(defineProps<{
  value: Cents | null | undefined
  meaning?: 'actual' | 'projected' | 'paid' | 'recommended'
}>(), { meaning: undefined })

const labels = {
  actual: 'Actual', projected: 'Projected', paid: 'Paid', recommended: 'Recommended'
}
</script>

<template>
  <span class="money-display">
    <span>{{ formatCents(props.value) }}</span>
    <span v-if="props.meaning" class="money-display__meaning">{{ labels[props.meaning] }}</span>
  </span>
</template>

<style scoped>
.money-display { display: inline-flex; align-items: baseline; gap: .5rem; font-variant-numeric: tabular-nums; }
.money-display__meaning { color: var(--ui-text-muted); font-size: .75rem; }
</style>
