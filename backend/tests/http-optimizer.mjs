// 从项目根目录运行：MINAX_TEST_OPTIMIZER=/srv/minax/bin/minax_optimize node backend/tests/http-optimizer.mjs
import {spawn} from 'node:child_process';
import {mkdtemp,rm} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import {join,resolve,dirname} from 'node:path';
import {fileURLToPath} from 'node:url';
import assert from 'node:assert/strict';
const root=resolve(dirname(fileURLToPath(import.meta.url)),'../..');
const temp=await mkdtemp(join(tmpdir(),'minax-http-'));
const port=Number(process.env.MINAX_TEST_PORT||18788),base=`http://127.0.0.1:${port}`;
const args=[];
if(process.env.MINAX_TEST_PHP_EXTENSION_DIR)args.push('-n','-d',`extension_dir=${process.env.MINAX_TEST_PHP_EXTENSION_DIR}`,'-d','extension=pdo','-d','extension=pdo_sqlite');
args.push('-S',`127.0.0.1:${port}`,'-t','backend/public','backend/router.php');
const child=spawn(process.env.PHP_BINARY||'php',args,{cwd:root,env:{...process.env,MINAX_DB_PATH:join(temp,'test.sqlite3'),MINAX_OPTIMIZER_EXE:process.env.MINAX_TEST_OPTIMIZER||join(root,'bin','minax_optimize')},stdio:['ignore','pipe','pipe']});
let log='',spawnError=null;child.stderr.on('data',d=>log+=d);child.on('error',e=>spawnError=e);
try{
 let session;
 for(let i=0;i<100;i++){
  if(spawnError)throw spawnError;
  if(child.exitCode!==null)throw new Error(log);
  try{session=await fetch(base+'/api/session');break;}catch{await new Promise(r=>setTimeout(r,50));}
 }
 assert.ok(session,'PHP server failed to start');assert.equal(session.status,200);
 const cookie=session.headers.get('set-cookie').split(';')[0],profile=await session.json();
 const headers={'Content-Type':'application/json','X-CSRF-Token':profile.csrf,Cookie:cookie};
 const body={goal:'min',expression:'( x - 2 ) * ( x - 2 ) + ( y + 1 ) * ( y + 1 )',variables:[{name:'x',lower:-5,upper:5,initial:0},{name:'y',lower:-5,upper:5,initial:0}],constraints:['x + y = 2']};
 const post=b=>fetch(base+'/api/optimize',{method:'POST',headers,body:JSON.stringify(b)});
 let response=await post(body);assert.equal(response.status,200);let result=await response.json();
 assert.equal(result.status,'feasible_found');assert.ok(Math.abs(result.objective-.5)<2e-6);assert.equal(result.globally_certified,false);
 response=await post({...body,constraints:['x <= 0','x >= 1']});assert.equal(response.status,200);assert.equal((await response.json()).status,'no_feasible_found');
 response=await post({...body,expression:'unknown + 1'});assert.equal(response.status,422);assert.equal((await response.json()).error.code,'UNKNOWN_VARIABLE');
 response=await fetch(base+'/api/optimize',{method:'POST',headers:{...headers,'X-CSRF-Token':'wrong'},body:JSON.stringify(body)});assert.equal(response.status,403);
 const page=await fetch(base+'/');assert.equal(page.status,200);assert.match(await page.text(),/id="app"/);
 console.log('PASS HTTP guest session -> CSRF -> optimize -> native JSON; infeasible and error responses; frontend entry');
}finally{
 if(child.exitCode===null && !spawnError){const closed=new Promise(r=>child.once('exit',r));child.kill();await closed;}
 await rm(temp,{recursive:true,force:true});
}
