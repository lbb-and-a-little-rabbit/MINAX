<script setup>import {t} from '../state';</script>
<template><nav class="mode-tabs" :aria-label="t('计算方式','Calculation mode')"><RouterLink to="/evaluate">{{t('表达式求值','Evaluate')}}</RouterLink><RouterLink to="/optimize">{{t('最值求解','Optimize')}}</RouterLink></nav></template>
