const part=/^(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?|^[A-Za-z_][A-Za-z0-9_]*|^(?:!=|>=|<=|[+\-*/=><(),])/;
export function normalize(source){
 const output=[];let rest=source.trim();
 if(!rest)throw new Error('empty');
 while(rest){const match=rest.match(part);if(!match)throw new Error('character');output.push(match[0]);rest=rest.slice(match[0].length).trimStart();}
 if(output.length>512)throw new Error('length');
 return output.join(' ');
}
export function validate(source){
 try{
  const text=normalize(source);let depth=0;
  for(const item of text.split(' ')){
   if(['=','!=','>','<','>=','<='].includes(item))return {ok:false,reason:'comparison'};
   if(item==='(')depth++;
   if(item===')' && --depth<0)return {ok:false,reason:'parentheses'};
  }
  return depth===0?{ok:true,text}:{ok:false,reason:'parentheses'};
 }catch(e){return {ok:false,reason:e.message};}
}
export function variablesPayload(variables){
 const seen=new Set();
 return variables.map((v,index)=>{
  const name=v.name.trim(),value=String(v.value).trim();
  const fail=reason=>{throw Object.assign(new Error(reason),{row:index});};
  if(!/^[A-Za-z_][A-Za-z0-9_]*$/.test(name))fail('name');
  if(seen.has(name))fail('duplicate');seen.add(name);
  if(!/^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?$/.test(value)||!Number.isFinite(Number(value)))fail('value');
  return {name,value:Number(value)};
 });
}
