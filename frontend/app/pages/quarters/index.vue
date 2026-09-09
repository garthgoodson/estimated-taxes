<script setup lang="ts">
import { currentQuarterRoute } from '~/utils/quarterNavigation'

const bootstrap = useBootstrap()
if (!bootstrap.state.value) await bootstrap.load()
await navigateTo(bootstrap.state.value ? currentQuarterRoute(bootstrap.state.value.current_quarter) : '/', { replace: true })
</script>

<template><div /></template>
