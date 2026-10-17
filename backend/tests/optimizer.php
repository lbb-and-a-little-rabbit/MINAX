<?php
declare(strict_types=1);
require dirname(__DIR__).'/bootstrap.php';
use Minax\Application;use Minax\ApiError;
function check(bool $ok,string $message):void{if(!$ok)throw new RuntimeException($message);}
function fails(callable $f,string $code):void{try{$f();}catch(ApiError $e){check($e->errorCode===$code,"Expected $code, got {$e->errorCode}");return;}throw new RuntimeException('Missing error '.$code);}
$dir=sys_get_temp_dir().'/minax-opt-test-'.bin2hex(random_bytes(5));mkdir($dir,0700);
$config=require dirname(__DIR__).'/config.php';$config['database']=$dir.'/test.sqlite3';$config['rate_limits']=false;
$config['optimizer']=getenv('MINAX_TEST_OPTIMIZER') ?: $config['optimizer'];
$app=new Application($config);$guest=$app->handle('GET','/api/session',[],[],null,'local');
$headers=['Host'=>'localhost','Content-Type'=>'application/json','X-CSRF-Token'=>$guest['data']['csrf']];
$body=['goal'=>'min','expression'=>'( x - 2 ) * ( x - 2 ) + ( y + 1 ) * ( y + 1 )','variables'=>[['name'=>'x','lower'=>-5,'upper'=>5,'initial'=>0],['name'=>'y','lower'=>-5,'upper'=>5,'initial'=>0]],'constraints'=>['x + y = 2']];
$post=fn($b)=>$app->handle('POST','/api/optimize',$b,$headers,$guest['cookie'],'local')['data'];
try{
 check(is_file($config['optimizer']),'Set MINAX_TEST_OPTIMIZER to a compiled executable');
 fails(fn()=>$app->handle('POST','/api/optimize',$body,$headers,null,'local'),'SESSION_EXPIRED');
 fails(fn()=>$app->handle('POST','/api/optimize',$body,array_replace($headers,['X-CSRF-Token'=>'bad']),$guest['cookie'],'local'),'REQUEST_REJECTED');
 $r=$post($body);check($r['status']==='feasible_found'&&!$r['globally_certified']&&abs($r['objective']-.5)<2e-6,'Equality minimum failed');echo "PASS equality minimum through Application\n";
 $max=$body;$max['goal']='max';$max['expression']='7 - pow ( x - 1 , 2 ) - pow ( y + 2 , 2 )';$max['constraints']=[];
 check(abs($post($max)['objective']-7)<1e-7,'Maximum failed');echo "PASS maximum\n";
 $bad=$body;$bad['constraints']=['x <= 0','x >= 1'];check($post($bad)['status']==='no_feasible_found','Impossible constraints reported success');echo "PASS no feasible point\n";
 $sum=$body;$sum['expression']='sum ( i , 1 , 3 , pow ( x - i , 2 ) )';$sum['constraints']=['x >= 0.5'];check(abs($post($sum)['objective']-2)<1e-6,'Sum failed');echo "PASS sum and >=\n";
 $bad=$body;$bad['expression']='missing + 1';fails(fn()=>$post($bad),'UNKNOWN_VARIABLE');
 $bad=$body;$bad['constraints']=['x < 2'];fails(fn()=>$post($bad),'UNSUPPORTED_CONSTRAINT');
 $bad=$body;$bad['variables'][0]['initial']=10;fails(fn()=>$post($bad),'INVALID_BOUNDS');
 $bad=$body;$bad['variables'][0]['lower']=true;fails(fn()=>$post($bad),'INVALID_BOUNDS');
 $bad=$body;$bad['variables'][0]['name']='x;touch';fails(fn()=>$post($bad),'INVALID_BOUNDS');
 $bad=$body;$bad['constraints']=['x = 1 = 2'];fails(fn()=>$post($bad),'INVALID_CONSTRAINTS');
 $missing=$config;$missing['optimizer']=$dir.'/absent';fails(fn()=>(new Minax\NativeOptimizer($missing))->calculate($body),'OPTIMIZER_UNAVAILABLE');
 // 限时必须终止真实原生进程，而不是让浏览器独自超时。
 $short=$config;$short['optimizer_timeout_seconds']=0;fails(fn()=>(new Minax\NativeOptimizer($short))->calculate($body),'OPTIMIZATION_TIMEOUT');
 // 占满两个计算槽时返回 BUSY，释放后继续可用。
 $locks=[];for($i=0;$i<2;$i++){$locks[$i]=fopen($dir.'/worker-'.$i.'.lock','c');flock($locks[$i],LOCK_EX);}
 try{fails(fn()=>$post($body),'BUSY');}finally{foreach($locks as $l){flock($l,LOCK_UN);fclose($l);}}
 check($post($body)['status']==='feasible_found','Worker lock was not released');
 echo "PASS authentication, validation, unavailable process, timeout and shared worker locks\nALL OPTIMIZER API TESTS PASSED\n";
}catch(Throwable $e){fwrite(STDERR,'FAIL '.$e->getMessage()."\n");exit(1);}
finally{unset($app);foreach(glob($dir.'/*') as $file)@unlink($file);@rmdir($dir);}
