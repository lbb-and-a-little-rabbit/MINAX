import {test} from 'node:test';
import assert from 'node:assert/strict';
import {normalize,validate,variablesPayload} from '../src/expression.js';
test('compact expressions and scientific notation',()=>{assert.equal(normalize('sum(i,1,3,i*x)+1e-3'),'sum ( i , 1 , 3 , i * x ) + 1e-3');assert.equal(normalize('-3+x*-2'),'- 3 + x * - 2');});
test('unsupported symbols and unbalanced brackets',()=>{assert.equal(validate('x^2').ok,false);assert.equal(validate('( x + 2').reason,'parentheses');assert.equal(validate('x + 2 )').reason,'parentheses');assert.equal(validate('x >= 2').reason,'comparison');assert.equal(validate('').reason,'empty');});
test('function input passes preliminary checks',()=>{assert.equal(validate('pow ( x , 2 )').ok,true);});
test('duplicate names, invalid names and values',()=>{for(const variables of [[{name:'x',value:'2'},{name:'x',value:'3'}],[{name:'1x',value:'1'}],[{name:'x',value:''}],[{name:'x',value:'Infinity'}],[{name:'x',value:'0xFF'}]])assert.throws(()=>variablesPayload(variables));});
test('valid variables retain order',()=>assert.deepEqual(variablesPayload([{name:'x',value:'-1e-3'},{name:'y',value:'.5'}]),[{name:'x',value:-.001},{name:'y',value:.5}]));
