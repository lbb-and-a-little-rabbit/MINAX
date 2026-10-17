import {normalize,validate} from './expression.js';
const numeric=/^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?$/;
export function optimizationPayload(draft){
 const fail=(code)=>{throw Object.assign(new Error(code),{code});};
 if(!['min','max'].includes(draft.goal))fail('INVALID_REQUEST');
 const objective=validate(draft.expression);
 if(!objective.ok)fail('INVALID_EXPRESSION');
 if(!Array.isArray(draft.variables)||draft.variables.length<1||draft.variables.length>32)fail('INVALID_BOUNDS');
 const seen=new Set();
 const variables=draft.variables.map(v=>{
  const name=String(v.name).trim();
  if(!/^[A-Za-z_][A-Za-z0-9_]{0,63}$/.test(name)||seen.has(name))fail('INVALID_BOUNDS');
  seen.add(name);const result={name};
  for(const field of ['lower','upper','initial']){
   const text=String(v[field]).trim();if(!numeric.test(text)||!Number.isFinite(Number(text)))fail('INVALID_BOUNDS');
   result[field]=Number(text);
  }
  if(result.lower>=result.upper||!Number.isFinite(result.upper-result.lower)||result.initial<result.lower||result.initial>result.upper)fail('INVALID_BOUNDS');
  return result;
 });
 const lines=draft.constraints.split(/\r?\n/).map(s=>s.trim()).filter(Boolean);
 if(lines.length>64)fail('INVALID_CONSTRAINTS');
 const constraints=lines.map(line=>{
  let text;try{text=normalize(line);}catch{fail('INVALID_CONSTRAINTS');}
  const parts=text.split(' ');
  if(parts.some(p=>['<','>','!='].includes(p)))fail('UNSUPPORTED_CONSTRAINT');
  const indices=parts.map((p,i)=>['=','<=','>='].includes(p)?i:-1).filter(i=>i>=0);
  if(indices.length!==1)fail('INVALID_CONSTRAINTS');
  const index=indices[0];
  if(!validate(parts.slice(0,index).join(' ')).ok || !validate(parts.slice(index+1).join(' ')).ok)fail('INVALID_CONSTRAINTS');
  return text;
 });
 return {goal:draft.goal,expression:objective.text,variables,constraints};
}
