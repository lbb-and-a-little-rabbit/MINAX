<script setup>
import {ref,watch} from 'vue';import {useRouter} from 'vue-router';import {state,t,notify} from '../state';import {api} from '../api';
const items=ref([]),loading=ref(true),error=ref(''),confirming=ref(false),deleting=ref(false),router=useRouter();
async function load(){loading.value=true;error.value='';try{items.value=(await api('/history')).items;}catch(e){error.value=e.message;}finally{loading.value=false;}}
function restore(item){state.draft={expression:item.expression,variables:item.variables.map(v=>({name:v.name,value:String(v.value)}))};router.push('/evaluate');notify(t('已载入，可修改后重新计算。','Loaded. Edit and calculate again.'));}
async function clear(){deleting.value=true;error.value='';try{await api('/history',{method:'DELETE',data:{}});items.value=[];confirming.value=false;notify(t('计算记录已清空。','History cleared.'));}catch(e){error.value=e.message;}finally{deleting.value=false;}}
function date(value){return new Date(value*1000).toLocaleString(state.lang==='zh'?'zh-CN':'en-US');}
watch(()=>state.ready,ready=>{if(ready)load();else loading.value=false;},{immediate:true});
</script>
<template><header class="page-header"><div><h1>{{t('计算记录','History')}}</h1><p>{{t('最近 30 次成功计算。选择一条记录可重新载入。','Your last 30 successful calculations. Select one to load it again.')}}</p></div><button v-if="items.length" class="button secondary" @click="confirming=true">{{t('清空记录','Clear history')}}</button></header>
 <p v-if="state.user?.guest" class="notice">{{t('游客记录临时保留。登录后，本次记录会保存到账户。','Guest history is temporary. Sign in to save this session’s calculations.')}} <RouterLink to="/account">{{t('登录','Sign in')}}</RouterLink></p>
 <div v-if="confirming" class="panel confirm-panel" role="alert"><h2>{{t('清空全部计算记录？','Clear all calculation history?')}}</h2><p>{{t('清空后无法恢复，当前正在编辑的表达式不会改变。','This cannot be undone. Your current expression will remain.')}}</p><div class="button-row"><button class="button secondary" :disabled="deleting" @click="confirming=false">{{t('取消','Cancel')}}</button><button class="button danger" :disabled="deleting" @click="clear">{{deleting?t('清空中…','Clearing…'):t('确认清空','Clear all')}}</button></div></div>
 <div v-if="error" class="notice error" role="alert">{{error}}<button class="text-button" @click="load">{{t('重试','Retry')}}</button></div>
 <div v-if="loading || !state.ready" class="panel empty-page" role="status">{{t('正在读取记录…','Loading history…')}}</div>
 <section v-else-if="!items.length&&!error" class="panel empty-page"><h2>{{t('还没有计算记录','No calculations yet')}}</h2><p>{{t('完成一次计算后，可在这里查看。','Completed calculations will appear here.')}}</p><RouterLink to="/evaluate" class="button primary">{{t('开始计算','Start calculating')}}</RouterLink></section>
 <div v-else class="history-list"><button v-for="item in items" :key="item.id" class="panel history-item" @click="restore(item)"><div><time>{{date(item.created)}}</time><code>{{item.expression}}</code><span class="history-variables">{{item.variables.map(v=>v.name+' = '+v.value).join('，') || t('无变量','No variables')}}</span></div><div class="history-answer"><span>{{t('结果','Result')}}</span><strong>{{item.displayValue}}</strong><span class="link-label">{{t('载入 →','Load →')}}</span></div></button></div>
</template>
