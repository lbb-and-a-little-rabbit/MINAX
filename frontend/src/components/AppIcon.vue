<script setup>
defineProps({name:String});
const paths={calculate:'M4 3h16v18H4z M8 7h8 M8 12h1 M15 12h1 M8 17h1 M15 17h1',history:'M3 11a9 9 0 1 1 2 7 M3 4v7h7 M12 7v6l4 2',help:'M9 9a3 3 0 1 1 5 2c-2 1-2 2-2 3 M12 18h.01 M21 12a9 9 0 1 1-18 0 9 9 0 0 1 18 0',settings:'M4 6h16 M4 12h16 M4 18h16 M8 3v6 M16 9v6 M10 15v6',user:'M16 7a4 4 0 1 1-8 0 4 4 0 0 1 8 0 M4 21v-2a8 8 0 0 1 16 0v2',plus:'M12 5v14 M5 12h14',close:'M6 6l12 12 M18 6L6 18',arrow:'M5 12h14 M13 6l6 6-6 6',upload:'M12 16V3 M7 8l5-5 5 5 M4 15v6h16v-6',copy:'M9 9h12v12H9z M15 9V3H3v12h6',check:'M5 12l4 4L19 6'};
</script>
<template><svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path :d="paths[name] || paths.calculate"/></svg></template>
