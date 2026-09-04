<script setup lang="ts">
import { validQuarter } from '~/utils/routes'

const route = useRoute()
const quarter = validQuarter(route.params.quarter)

if (quarter == null) await navigateTo('/')
</script>

<template>
  <div class="placeholder">
    <p class="eyebrow">Quarter workspace</p>
    <h1>Quarter {{ quarter }}</h1>
    <p>The complete quarterly entry workflow begins in F2. This route is ready for all four quarters.</p>
  </div>
</template>

<style scoped>
.placeholder { display: grid; gap: .75rem; } h1, p { margin: 0; } p { color: var(--ui-text-muted); }.eyebrow { color: var(--ui-primary); font-size: .75rem; font-weight: 700; letter-spacing: .08em; text-transform: uppercase; }
</style>
