<script setup>
import {onMounted,watch,nextTick,ref} from 'vue';
import {useRoute} from 'vue-router';
import {state,t} from './state';
import {initialize} from './api';
import AppIcon from './components/AppIcon.vue';
const route=useRoute(),main=ref(null);
const links=[['/evaluate','calculate','计算','Calculate'],['/history','history','计算记录','History'],['/guide','help','使用说明','Help'],['/settings','settings','设置','Settings']];
watch(()=>route.path,async()=>{await nextTick();main.value?.focus();window.scrollTo({top:0,behavior:'instant'});});
onMounted(initialize);
</script>
<template>
 <a class="skip-link" href="#main">{{t('跳转到内容','Skip to content')}}</a>
 <div class="app-shell">
  <aside class="sidebar">
   <RouterLink to="/evaluate" class="brand"><img src="/assets/MINAX.png" alt=""><span>MINAX</span></RouterLink>
   <nav :aria-label="t('主导航','Main navigation')"><RouterLink v-for="[path,icon,zh,en] in links" :key="path" :to="path" :class="{active:route.path===path || (path==='/evaluate' && route.path==='/optimize')}"><AppIcon :name="icon"/><span>{{t(zh,en)}}</span></RouterLink></nav>
   <RouterLink to="/account" class="account-link" :class="{active:route.path==='/account'}"><span class="avatar"><AppIcon name="user"/></span><span><strong>{{state.user && !state.user.guest ? state.user.name : t('游客','Guest')}}</strong><small>{{state.user && !state.user.guest?t('账户管理','Account'):t('登录或注册','Sign in or register')}}</small></span></RouterLink>
  </aside>
  <main id="main" ref="main" tabindex="-1">
   <div v-if="state.sessionError || (!state.ready && state.user)" class="notice warning" role="alert"><span>{{state.sessionError || t('登录已过期，请重新连接后继续。','Your session expired. Reconnect to continue.')}}</span><button class="text-button" @click="initialize">{{t('重试','Retry')}}</button></div>
   <RouterView/>
  </main>
 </div>
 <div v-if="state.notice" class="toast" role="status">{{state.notice}}</div>
</template>
