<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { formatCentsInput, parseDollars } from '~/utils/money'
import type { Cents } from '~/types/api'

const props = withDefaults(defineProps<{ modelValue: Cents | null; allowNegative?: boolean; label: string }>(), { allowNegative: false })
const emit = defineEmits<{ 'update:modelValue': [value: Cents | null] }>()
const text = ref(formatCentsInput(props.modelValue))
const editing = ref(false)
const error = computed(() => text.value.trim() && parseDollars(text.value, props.allowNegative) === null ? 'Enter a valid dollar amount.' : undefined)

watch(() => props.modelValue, value => {
  if (!editing.value) text.value = formatCentsInput(value)
})

function commit() {
  editing.value = false
  const value = parseDollars(text.value, props.allowNegative)
  if (text.value.trim() && value === null) return
  emit('update:modelValue', value)
  text.value = formatCentsInput(value)
}
</script>

<template>
  <UFormField :label="label" :error="error">
    <UInput
      v-model="text"
      inputmode="decimal"
      autocomplete="off"
      placeholder="0.00"
      @focus="editing = true"
      @blur="commit"
    />
  </UFormField>
</template>
