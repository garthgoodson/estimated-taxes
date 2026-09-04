<script setup lang="ts">
import { computed } from 'vue'
import { formatCentsInput, parseDollars } from '~/utils/money'
import type { Cents } from '~/types/api'

const props = withDefaults(defineProps<{ modelValue: Cents | null, allowNegative?: boolean, label: string }>(), { allowNegative: false })
const emit = defineEmits<{ 'update:modelValue': [value: Cents | null] }>()
const displayValue = computed(() => formatCentsInput(props.modelValue))

function update(value: string) {
  emit('update:modelValue', parseDollars(value, props.allowNegative))
}
</script>

<template>
  <UFormField :label="label">
    <UInput
      :model-value="displayValue"
      inputmode="decimal"
      autocomplete="off"
      placeholder="0.00"
      @update:model-value="update(String($event))"
    />
  </UFormField>
</template>
