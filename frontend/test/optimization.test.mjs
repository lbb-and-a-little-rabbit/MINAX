import {test} from 'node:test';
import assert from 'node:assert/strict';
import {optimizationPayload} from '../src/optimization.js';
const draft=()=>({goal:'min',expression:'pow(x,2)',constraints:'x>=0\nx<=2',variables:[{name:'x',lower:'-2',upper:'2',initial:'0'}]});
test('optimization normalizes compact formula and all constraints',()=>{
 const p=optimizationPayload(draft());assert.equal(p.expression,'pow ( x , 2 )');assert.deepEqual(p.constraints,['x >= 0','x <= 2']);assert.equal(p.variables[0].lower,-2);
});
test('bounds reject missing/nonfinite values, invalid order and starting point',()=>{
 for(const change of [{lower:''},{lower:'Infinity'},{lower:'2'},{upper:'-3'},{initial:'3'},{name:'x;bad'}]){
  const d=draft();Object.assign(d.variables[0],change);assert.throws(()=>optimizationPayload(d),{code:'INVALID_BOUNDS'});
 }
 const d=draft();d.variables.push({...d.variables[0]});assert.throws(()=>optimizationPayload(d),{code:'INVALID_BOUNDS'});
});
test('strict and malformed constraints are rejected',()=>{
 for(const text of ['x<2','x>0','x!=1']){const d=draft();d.constraints=text;assert.throws(()=>optimizationPayload(d),{code:'UNSUPPORTED_CONSTRAINT'});}
 for(const text of ['x','x<=','x=1=2','(x=2)']){const d=draft();d.constraints=text;assert.throws(()=>optimizationPayload(d),{code:'INVALID_CONSTRAINTS'});}
});
test('no constraints and maximization are supported',()=>{const d=draft();d.constraints='\n ';d.goal='max';assert.deepEqual(optimizationPayload(d).constraints,[]);});
