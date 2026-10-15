import { reactive, watch } from 'vue';
function saved(key,fallback){try{return JSON.parse(localStorage.getItem(key)) ?? fallback;}catch{return fallback;}}
export const state=reactive({
 lang:saved('minax-v2-language','zh'),theme:saved('minax-v2-theme','light'),fontSize:saved('minax-v2-font','normal'),
 user:null,csrf:'',ready:false,sessionError:'',
 draft:saved('minax-v2-draft',{expression:'sum ( i , 1 , 3 , i * x ) + pow ( y , 2 )',variables:[{name:'x',value:'2'},{name:'y',value:'3'}]}),
 notice:''
});
export const t=(zh,en)=>state.lang==='zh'?zh:en;
let timer;
export function notify(message){state.notice=message;clearTimeout(timer);timer=setTimeout(()=>state.notice='',5000);}
function persist(key,value){try{localStorage.setItem(key,JSON.stringify(value));}catch{/* 存储不可用不阻止计算 */}}
watch(()=>state.lang,value=>{persist('minax-v2-language',value);document.documentElement.lang=value==='zh'?'zh-CN':'en';},{immediate:true});
watch(()=>state.theme,value=>{persist('minax-v2-theme',value);document.documentElement.dataset.theme=value;},{immediate:true});
watch(()=>state.fontSize,value=>{persist('minax-v2-font',value);document.documentElement.dataset.size=value;},{immediate:true});
watch(()=>state.draft,value=>persist('minax-v2-draft',value),{deep:true});
export const functions=[['sin',1],['cos',1],['tan',1],['exp',1],['log',1],['sqrt',1],['abs',1],['pow',2],['min',2],['max',2]];
export const examples=[
 {zh:'求和',en:'Sum',expression:'sum ( i , 1 , 3 , i * x ) + pow ( y , 2 )',variables:[{name:'x',value:'2'},{name:'y',value:'3'}]},
 {zh:'平方根',en:'Square root',expression:'sqrt ( pow ( x , 2 ) + pow ( y , 2 ) )',variables:[{name:'x',value:'3'},{name:'y',value:'4'}]},
 {zh:'嵌套连乘',en:'Nested product',expression:'sum ( i , 1 , 3 , prod ( j , 1 , i , j ) )',variables:[]}
];
