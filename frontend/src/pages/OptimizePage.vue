<script setup>
import {ref,watch,nextTick,computed} from 'vue';
import {state,t,functions} from '../state';
import {api,errorText} from '../api';
import {optimizationPayload} from '../optimization';
import ModeTabs from '../components/ModeTabs.vue';
import AppIcon from '../components/AppIcon.vue';
const draft=computed(()=>state.optimizeDraft);
const busy=ref(false),error=ref(''),result=ref(null),editor=ref(null),constraintEditor=ref(null),target=ref('expression');
const keys=['7','8','9','+','(','4','5','6','-',')','1','2','3','*',',','0','.','e','/','⌫'];
const example={goal:'min',variables:[{name:'x',lower:'-5',upper:'5',initial:'0'},{name:'y',lower:'-5',upper:'5',initial:'0'}],expression:'( x - 2 ) * ( x - 2 ) + ( y + 1 ) * ( y + 1 )',constraints:'x + y = 2'};
watch(draft,()=>{result.value=null;error.value='';},{deep:true});
function loadExample(){state.optimizeDraft=structuredClone(example);}
function addVariable(){draft.value.variables.push({name:'',lower:'-10',upper:'10',initial:'0'});}
async function solve(){
 if(busy.value||!state.ready)return;
 error.value='';result.value=null;
 let payload;try{payload=optimizationPayload(draft.value);}catch(e){error.value=errorText(e.code);return;}
 const snapshot=JSON.stringify(draft.value);busy.value=true;
 try{
  const response=await api('/optimize',{method:'POST',data:payload,timeoutMs:window.MINAX_CONFIG?.optimizerRequestTimeoutMs ?? 35000});
  if(snapshot===JSON.stringify(draft.value))result.value=response;
 }catch(e){error.value=e.message;}finally{busy.value=false;}
}
async function insert(text){
 const field=target.value,el=field==='expression'?editor.value:constraintEditor.value;
 if(!el)return;
 const source=draft.value[field],a=el.selectionStart,b=el.selectionEnd;
 let start=a,content=text;
 if(text==='⌫'){start=a===b?Math.max(0,a-1):a;content='';}
 else if(!/^[0-9.e]$/.test(text))content=(a&&!/\s$/.test(source.slice(0,a))?' ':'')+text+' ';
 draft.value[field]=source.slice(0,start)+content+source.slice(b);
 await nextTick();el.focus();el.setSelectionRange(start+content.length,start+content.length);
}
function format(){try{const p=optimizationPayload(draft.value);draft.value.expression=p.expression;draft.value.constraints=p.constraints.join('\n');}catch(e){error.value=errorText(e.code);}}
const display=value=>typeof value==='number'&&Number.isFinite(value)?value.toPrecision(12).replace(/\.?0+(?=e|$)/,''):'—';
const stopText=computed(()=>{
 if(result.value?.termination==='time_budget')return t('已达到本次搜索时间限制。','The search time limit was reached.');
 if(result.value?.termination==='evaluation_budget')return t('已达到本次计算次数限制。','The evaluation limit was reached.');
 return t('本次搜索已完成。','This search has finished.');
});
</script>
<template>
 <header class="page-header"><div><h1>{{t('计算','Calculate')}}</h1><p>{{t('设定变量范围、目标与约束，寻找可行的最优候选解。','Set bounds, an objective and constraints to search for a feasible best candidate.')}}</p></div><button class="button secondary" :disabled="busy" @click="loadExample">{{t('载入示例','Load example')}}</button></header>
 <ModeTabs/>
 <form class="calculation-layout" @submit.prevent="solve">
  <div class="input-column">
   <section class="panel">
    <div class="section-heading"><h2>{{t('变量范围','Variable bounds')}}</h2><button type="button" class="button quiet" :disabled="busy||draft.variables.length>=32" @click="addVariable">{{t('添加变量','Add variable')}}</button></div>
    <p class="supporting">{{t('每个变量都需要有限的上下界，初始值须在范围内。','Every variable needs finite bounds and an initial value within them.')}}</p>
    <div v-for="(v,i) in draft.variables" :key="i" class="bounds-row">
     <label>{{t('变量名','Name')}}<input v-model="v.name" :disabled="busy" spellcheck="false" autocomplete="off" maxlength="64" placeholder="x"></label>
     <label>{{t('下界','Lower bound')}}<input v-model="v.lower" :disabled="busy" inputmode="decimal" placeholder="-5"></label>
     <label>{{t('上界','Upper bound')}}<input v-model="v.upper" :disabled="busy" inputmode="decimal" placeholder="5"></label>
     <label>{{t('初始值','Initial value')}}<input v-model="v.initial" :disabled="busy" inputmode="decimal" placeholder="0"></label>
     <button type="button" class="icon-button delete" :disabled="busy||draft.variables.length<=1" @click="draft.variables.splice(i,1)" :aria-label="t('移除变量 ','Remove variable ')+(v.name||i+1)"><AppIcon name="close"/></button>
    </div>
   </section>
   <section class="panel expression-panel">
    <div class="section-heading"><h2>{{t('目标与约束','Objective and constraints')}}</h2><button type="button" class="button quiet" :disabled="busy" @click="format">{{t('整理格式','Format')}}</button></div>
    <fieldset class="goal-choice" :disabled="busy"><legend>{{t('求解方向','Direction')}}</legend><label><input v-model="draft.goal" type="radio" value="min">{{t('最小值','Minimize')}}</label><label><input v-model="draft.goal" type="radio" value="max">{{t('最大值','Maximize')}}</label></fieldset>
    <label for="objective">{{t('目标表达式','Objective expression')}}</label><textarea id="objective" ref="editor" v-model="draft.expression" :disabled="busy" spellcheck="false" aria-describedby="optimization-help" @focus="target='expression'" @keydown.ctrl.enter.prevent="solve" placeholder="pow ( x , 2 ) + pow ( y , 2 )"></textarea>
    <label for="constraints">{{t('约束条件（可选，每行一个）','Constraints (optional, one per line)')}}</label><textarea id="constraints" ref="constraintEditor" v-model="draft.constraints" :disabled="busy" spellcheck="false" aria-describedby="optimization-help" @focus="target='constraints'" placeholder="x + y = 2"></textarea>
    <p id="optimization-help" class="supporting">{{t('支持 =、<=、>=。幂运算使用 pow ( x , 2 )；支持 sum、prod。提交时自动整理符号间距。','Use =, <= or >=. Write powers as pow ( x , 2 ). sum and prod are supported. Spacing is formatted automatically.')}}</p>
    <div v-if="error" class="notice error" role="alert">{{error}}</div>
    <div class="calculate-actions"><span class="supporting">{{t('通常几秒，复杂问题可能需要约 30 秒。','Usually a few seconds; complex problems may take about 30 seconds.')}}</span><button type="submit" class="button primary" :disabled="busy||!state.ready"><span v-if="busy" class="spinner" aria-hidden="true"></span>{{busy?t('求解中…','Searching…'):t('开始求解','Start search')}}</button></div>
   </section>
   <section class="panel keyboard-panel"><h2>{{t('公式键盘','Expression keyboard')}}</h2><p class="supporting">{{target==='expression'?t('输入位置：目标表达式','Editing: objective'):t('输入位置：约束条件','Editing: constraints')}}</p><div class="keyboard-content"><div class="number-keys"><button v-for="k in keys" :key="k" type="button" :disabled="busy" @click="insert(k)" :aria-label="k==='⌫'?t('退格','Backspace'):k">{{k}}</button></div><div class="function-keys"><button v-for="[f] in functions" :key="f" type="button" :disabled="busy" @click="insert(f+' ( )')">{{f}}</button><button v-for="f in ['sum','prod']" :key="f" type="button" :disabled="busy" @click="insert(f+' ( i , 1 , 3 , i )')">{{f}}</button><button v-for="r in ['=','<=','>=']" :key="r" type="button" :disabled="busy||target!=='constraints'" @click="insert(r)">{{r}}</button></div></div></section>
  </div>
  <aside class="output-column"><section class="panel result-panel" :aria-busy="busy"><h2>{{t('求解结果','Search result')}}</h2><div aria-live="polite" aria-atomic="true">
   <template v-if="result?.status==='feasible_found'"><p class="result-state"><AppIcon name="check"/>{{t('找到可行候选解','Feasible candidate found')}}</p><p>{{result.goal==='min'?t('本次找到的最小目标值','Lowest objective found'):t('本次找到的最大目标值','Highest objective found')}}</p><div class="result-value">{{display(result.objective)}}</div><dl class="solution-values"><template v-for="(value,name) in result.variables" :key="name"><dt>{{name}}</dt><dd>{{display(value)}}</dd></template></dl><p class="supporting">{{t('已按容差复核约束；此结果不构成全局最优证明。','Constraints were checked within tolerance. This is not a global optimality certificate.')}}</p></template>
   <template v-else-if="result"><h3>{{t('本次未找到可行解','No feasible point found')}}</h3><p>{{t('请检查约束是否冲突，或调整变量范围与初始值后重试。这不代表问题一定无解。','Check for conflicting constraints or adjust bounds and initial values. This does not prove the problem has no solution.')}}</p></template>
   <template v-else><div class="result-placeholder">—</div><p>{{busy?t('正在搜索并检查约束，请稍候。','Searching and checking constraints. Please wait.'):t('求解结果将显示在这里。','Search results will appear here.')}}</p></template>
   <template v-if="result"><p class="supporting">{{stopText}}</p><p>{{t('计算次数','Evaluations')}}：{{result.evaluations}}</p><p>{{t('用时','Elapsed')}}：{{(result.durationMs/1000).toFixed(2)}} s</p><details v-if="result.constraints?.length"><summary>{{t('约束检查','Constraint checks')}}</summary><div v-for="(c,i) in result.constraints" :key="i" class="constraint-result"><code>{{c.name}}</code><p>{{c.satisfied?t('满足','Satisfied'):t('未满足','Not satisfied')}} · {{t('违反量','Violation')}} {{display(c.violation)}} · {{t('容差','Tolerance')}} {{display(c.tolerance)}}</p></div></details></template>
  </div></section></aside>
 </form>
</template>
<style scoped>
.bounds-row{display:grid;grid-template-columns:repeat(4,minmax(0,1fr)) 44px;gap:12px;align-items:end;margin:18px 0}.bounds-row label{min-width:0}.bounds-row input{width:100%;min-width:0}.goal-choice{border:0;padding:0;margin:0 0 24px;display:flex;gap:24px}.goal-choice legend{margin-bottom:12px}.goal-choice label{display:flex;gap:10px;align-items:center}.goal-choice input{width:20px;height:20px}.solution-values{display:grid;grid-template-columns:1fr 2fr;gap:12px;font-size:20px}.solution-values dd{margin:0;text-align:right;overflow-wrap:anywhere}.constraint-result{padding:12px 0;border-bottom:1px solid var(--border)}.constraint-result code{white-space:normal;overflow-wrap:anywhere}summary{cursor:pointer;padding:12px 0}.expression-panel>label{display:block;margin:16px 0 8px}@media(max-width:650px){.bounds-row{grid-template-columns:repeat(2,minmax(0,1fr))}.bounds-row .delete{grid-column:2;justify-self:end}.goal-choice{gap:16px}}
</style>
