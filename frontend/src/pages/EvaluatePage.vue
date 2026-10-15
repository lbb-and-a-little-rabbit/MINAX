<script setup>
import {ref,computed,watch,nextTick} from 'vue';
import {state,t,notify,functions,examples} from '../state';
import {api} from '../api';
import {normalize,validate,variablesPayload} from '../expression';
import ModeTabs from '../components/ModeTabs.vue';
import AppIcon from '../components/AppIcon.vue';
const expression= computed({get:()=>state.draft.expression,set:value=>state.draft.expression=value});
const variables=computed(()=>state.draft.variables);
const editor=ref(null),fileInput=ref(null),preview=ref(null),imported=ref(null),error=ref(''),fieldError=ref(null),result=ref(null),busy=ref(false),showKeyboard=ref(true);
const keys=['7','8','9','+','(','4','5','6','-',')','1','2','3','*',',','0','.','e','/','⌫'];
const messages={empty:['请先输入表达式。','Enter an expression.'],character:['包含不支持的符号。幂运算请使用 pow ( x , 2 )。','Unsupported symbol. Use pow ( x , 2 ) for powers.'],length:['表达式过长，请简化后重试。','Simplify the expression and try again.'],parentheses:['左右括号不匹配，请检查。','Check that parentheses are balanced.'],comparison:['求值只接受算术表达式，请移除比较条件。','Remove comparison conditions to evaluate an expression.']};
const variableMessages={name:['变量名须以字母或下划线开头。','Start the name with a letter or underscore.'],duplicate:['变量名重复，请修改。','This name is already used.'],value:['请输入有效数字。','Enter a valid number.']};
watch(()=>state.draft,()=>{result.value=null;error.value='';fieldError.value=null;},{deep:true});
function showError(reason){error.value=t(...(messages[reason] || messages.character));editor.value?.focus();}
async function calculate(){
 if(busy.value)return;
 error.value='';fieldError.value=null;
 const checked=validate(expression.value);if(!checked.ok){showError(checked.reason);return;}
 let values;
 try{values=variablesPayload(variables.value);}catch(e){fieldError.value={row:e.row,message:t(...variableMessages[e.message])};await nextTick();document.querySelector('[data-row="'+e.row+'"] input'+(e.message==='value'?'.value-input':''))?.focus();return;}
 busy.value=true;
 const snapshot=JSON.stringify(state.draft);
 try{
  const response=await api('/evaluate',{method:'POST',data:{expression:checked.text,variables:values}});
  if(snapshot===JSON.stringify(state.draft))result.value=response;
  else notify(t('输入已修改，刚才的结果可在计算记录中查看。','Inputs changed. Find the previous result in History.'));
 }catch(e){error.value=e.message;}
 finally{busy.value=false;}
}
async function addVariable(){variables.value.push({name:'',value:''});await nextTick();document.querySelector('[data-row="'+(variables.value.length-1)+'"] input')?.focus();}
function removeVariable(index){const old=variables.value[index];variables.value.splice(index,1);notify(t('已移除变量 ','Removed variable ')+(old.name||String(index+1)));}
function format(){try{expression.value=normalize(expression.value);notify(t('表达式已整理。','Expression formatted.'));}catch(e){showError(e.message);}}
async function insert(text,kind='symbol'){
 const el=editor.value;if(!el)return;
 const a=el.selectionStart,b=el.selectionEnd,before=expression.value.slice(0,a),after=expression.value.slice(b);
 let content=text,prefix='',suffix='',offset;
 if(kind==='function'){content=text==='sum'||text==='prod'?text+' ( i , 1 , 3 , i )':text+' ( )';}
 if(kind!=='number' && !((text==='+'||text==='-') && /[0-9.]e$/i.test(before))){prefix=before && !/\s$/.test(before)?' ':'';suffix=after&&!/^\s/.test(after)?' ':'';}
 else if(before && /[A-Za-z_)\]]$/.test(before) && text!=='e'){prefix=' ';}
 expression.value=before+prefix+content+suffix+after;offset=before.length+prefix.length+content.length;
 if(kind==='function' && !['sum','prod'].includes(text))offset--;
 await nextTick();el.focus();el.setSelectionRange(offset,offset);
}
async function key(key){
 if(key!=='⌫'){await insert(key,/^[0-9.e]$/.test(key)?'number':'symbol');return;}
 const el=editor.value,a=el.selectionStart,b=el.selectionEnd,start=a===b?Math.max(0,a-1):a;
 expression.value=expression.value.slice(0,start)+expression.value.slice(b);await nextTick();el.focus();el.setSelectionRange(start,start);
}
function example(index){const ex=examples[index];state.draft={expression:ex.expression,variables:ex.variables.map(v=>({...v}))};notify(t('示例已载入。','Example loaded.'));}
async function readFile(event){
 const file=event.target.files?.[0];event.target.value='';if(!file)return;
 if(!file.name.toLowerCase().endsWith('.mina') || file.size>65536){notify(t('请选择不超过 64 KB 的 .mina 文件。','Choose a .mina file smaller than 64 KB.'));return;}
 try{imported.value={name:file.name,text:await file.text()};await nextTick();preview.value?.focus();}catch{notify(t('文件读取失败，请重新选择。','Unable to read the file. Choose it again.'));}
}
function useSelection(){
 const el=preview.value;const text=imported.value.text.slice(el.selectionStart,el.selectionEnd).trim();
 if(!text){notify(t('请先选中一段表达式。','Select an expression first.'));return;}
 try{expression.value=normalize(text);imported.value=null;notify(t('表达式已载入，请确认变量值。','Expression loaded. Check the variable values.'));}catch{notify(t('所选内容不是可识别的表达式。','The selected text is not a recognized expression.'));}
}
async function copy(){try{await navigator.clipboard.writeText(result.value.displayValue);notify(t('结果已复制。','Result copied.'));}catch{notify(t('请选中结果后手动复制。','Select the result and copy it manually.'));}}
</script>
<template>
 <header class="page-header"><div><h1>{{t('计算','Calculate')}}</h1><p>{{t('填写变量值，输入需要计算的表达式。','Enter variable values and an expression.')}}</p></div><button class="button secondary" @click="fileInput.click()"><AppIcon name="upload"/>{{t('导入文件','Import file')}}</button><input ref="fileInput" hidden type="file" accept=".mina" @change="readFile"></header>
 <ModeTabs/>
 <section v-if="imported" class="panel import-panel"><div class="section-heading"><h2>{{imported.name}}</h2><button class="icon-button" @click="imported=null" :aria-label="t('关闭文件预览','Close file preview')"><AppIcon name="close"/></button></div><p id="import-help">{{t('当前支持预览文件。选中一段表达式后点击“载入选中内容”；变量值需另行填写。','Select an expression in the preview, then choose Load selection. Enter variable values separately.')}}</p><textarea ref="preview" :value="imported.text" readonly aria-describedby="import-help" :aria-label="t('文件内容','File contents')"></textarea><button class="button primary" @click="useSelection">{{t('载入选中内容','Load selection')}}</button></section>
 <div class="calculation-layout">
  <div class="input-column">
   <section class="panel"><div class="section-heading"><h2>{{t('变量','Variables')}}</h2><button class="button quiet" @click="addVariable" :disabled="busy||variables.length>=100"><AppIcon name="plus"/>{{t('添加变量','Add variable')}}</button></div>
    <p id="variable-help" class="supporting">{{t('变量名以字母或下划线开头；没有变量时可留空。','Start names with a letter or underscore. No variables are required for constants.')}}</p>
    <div v-for="(v,i) in variables" :key="i" :data-row="i" class="variable-item">
     <div class="variable-row"><label>{{t('变量名','Name')}}<input v-model="v.name" :disabled="busy" spellcheck="false" autocomplete="off" placeholder="x" aria-describedby="variable-help" :aria-invalid="fieldError?.row===i" :aria-errormessage="fieldError?.row===i?'variable-error':undefined"></label><span class="equals">=</span><label>{{t('数值','Value')}}<input class="value-input" v-model="v.value" :disabled="busy" inputmode="decimal" placeholder="0" :aria-invalid="fieldError?.row===i"></label><button class="icon-button delete" :disabled="busy" @click="removeVariable(i)" :aria-label="t('移除变量 ','Remove variable ')+(v.name||i+1)"><AppIcon name="close"/></button></div>
     <p v-if="fieldError?.row===i" id="variable-error" class="field-error" role="alert">{{fieldError.message}}</p>
    </div>
    <p v-if="!variables.length" class="empty-inline">{{t('未添加变量，可直接计算常量表达式。','No variables. You can evaluate a constant expression.')}}</p>
   </section>
   <section class="panel expression-panel"><div class="section-heading"><label for="expression" class="heading-label">{{t('表达式','Expression')}}</label><button class="button quiet" :disabled="busy" @click="format">{{t('整理格式','Format')}}</button></div><textarea id="expression" ref="editor" v-model="expression" :disabled="busy" spellcheck="false" placeholder="x + y * 2" aria-describedby="expression-help" :aria-invalid="!!error" @keydown.ctrl.enter.prevent="state.ready&&calculate()" @keydown.meta.enter.prevent="state.ready&&calculate()"></textarea><div class="expression-help" id="expression-help"><span>{{t('幂运算示例：pow ( x , 2 )','Power example: pow ( x , 2 )')}}</span><RouterLink to="/guide">{{t('格式说明','Syntax help')}}</RouterLink></div>
    <div v-if="error" class="notice error" role="alert">{{error}}</div>
    <div class="calculate-actions"><span class="shortcut">Ctrl + Enter</span><button class="button primary" :disabled="busy||!state.ready" @click="calculate"><span v-if="busy" class="spinner" aria-hidden="true"></span>{{busy?t('计算中…','Calculating…'):t('计算','Calculate')}}<AppIcon v-if="!busy" name="arrow"/></button></div>
   </section>
   <section class="panel keyboard-panel"><button class="disclosure" :aria-expanded="showKeyboard" aria-controls="keyboard" @click="showKeyboard=!showKeyboard"><h2>{{t('公式键盘','Expression keyboard')}}</h2><span>{{showKeyboard?'−':'+'}}</span></button><div v-show="showKeyboard" id="keyboard" class="keyboard-content"><div class="number-keys"><button v-for="k in keys" :key="k" :disabled="busy" :class="{operator:!/[0-9.e]/.test(k)}" @click="key(k)" :aria-label="k==='⌫'?t('退格','Backspace'):k">{{k}}</button></div><div><p class="small-label">{{t('函数','Functions')}}</p><div class="function-keys"><button v-for="[name] in functions" :key="name" :disabled="busy" @click="insert(name,'function')">{{name}}</button><button :disabled="busy" @click="insert('sum','function')">Σ sum</button><button :disabled="busy" @click="insert('prod','function')">Π prod</button></div><div v-if="variables.some(v=>v.name)" class="variable-chips"><button v-for="(v,i) in variables.filter(v=>v.name)" :key="i" :disabled="busy" @click="insert(v.name)">{{v.name}}</button></div></div></div></section>
  </div>
  <aside class="output-column">
   <section class="panel result-panel" :aria-busy="busy"><div class="section-heading"><h2>{{t('结果','Result')}}</h2><button v-if="result" class="icon-button" @click="copy" :aria-label="t('复制结果','Copy result')"><AppIcon name="copy"/></button></div><div aria-live="polite" aria-atomic="true"><template v-if="result"><div class="result-value">{{result.displayValue}}</div><p class="result-state"><AppIcon name="check"/>{{t('计算完成','Completed')}}</p></template><template v-else><div class="result-placeholder">—</div><p>{{busy?t('正在计算，请稍候。','Calculating, please wait.'):t('计算结果将显示在这里。','Your result will appear here.')}}</p></template></div><RouterLink to="/history" class="result-history">{{t('查看计算记录','View history')}}<AppIcon name="arrow"/></RouterLink></section>
   <section class="panel examples-panel"><h2>{{t('示例','Examples')}}</h2><p>{{t('载入后可修改变量和表达式。','Load an example, then adjust it.')}}</p><button v-for="(ex,i) in examples" :key="i" :disabled="busy" @click="example(i)"><span>{{t(ex.zh,ex.en)}}</span><AppIcon name="arrow"/></button></section>
  </aside>
 </div>
</template>
