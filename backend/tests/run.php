<?php
declare(strict_types=1);
require dirname(__DIR__).'/bootstrap.php';
use Minax\Application; use Minax\ApiError; use Minax\NativeEvaluator;
$directory=sys_get_temp_dir().'/minax-test-'.bin2hex(random_bytes(6));mkdir($directory,0700);
$config=require dirname(__DIR__).'/config.php';
$config['database']=$directory.'/test.sqlite3';$config['rate_limits']=false;
$native=getenv('MINAX_TEST_NATIVE');if($native)$config['native']=$native;
$app=new Application($config);$passed=0;$skipped=0;
function check(bool $condition,string $message):void{if(!$condition)throw new RuntimeException($message);}
function fails(callable $fn,string $code):void{try{$fn();}catch(ApiError $e){check($e->errorCode===$code,"Expected $code, got {$e->errorCode}");return;}throw new RuntimeException('Expected failure: '.$code);}
function run(string $name,callable $fn):void{global $passed;$fn();$passed++;echo "PASS $name\n";}
function request(Application $app,string $method,string $path,array $body=[],?array $session=null,array $extra=[]):array{
 $headers=['Host'=>'localhost','Content-Type'=>'application/json'];if($session)$headers['X-CSRF-Token']=$session['data']['csrf'];
 return $app->handle($method,$path,$body,array_replace($headers,$extra),$session['cookie'] ?? null,'127.0.0.1');
}
try{
 $guest=request($app,'GET','/api/session');$other=request($app,'GET','/api/session');
 run('guest session',function()use($guest){check($guest['data']['user']['guest']===true,'Guest expected');check(strlen($guest['cookie'])===64,'Random session value expected');});
 run('session required',fn()=>fails(fn()=>request($app,'GET','/api/history'),'SESSION_EXPIRED'));
 run('request forgery rejected',fn()=>fails(fn()=>request($app,'POST','/api/evaluate',[],$guest,['X-CSRF-Token'=>'wrong']),'REQUEST_REJECTED'));
 run('untrusted origin rejected',fn()=>fails(fn()=>request($app,'GET','/api/history',[],$guest,['Origin'=>'https://evil.example']),'REQUEST_REJECTED'));
 run('invalid variables rejected',function()use($app,$guest){foreach([true,INF,'2'] as $value)fails(fn()=>request($app,'POST','/api/evaluate',['expression'=>'x','variables'=>[['name'=>'x','value'=>$value]]],$guest),'INVALID_VARIABLES');});
 run('optimizer unavailable',fn()=>fails(fn()=>request($app,'POST','/api/optimize',[],$guest),'NOT_AVAILABLE'));
 if(is_file($config['native'])){
  run('real native evaluation: sum, nested product, functions',function()use($app,$guest){
   $cases=[['sum ( i , 1 , 3 , i * x ) + pow ( y , 2 )',[['name'=>'x','value'=>2],['name'=>'y','value'=>3]],21],['sum ( i , 1 , 3 , prod ( j , 1 , i , j ) )',[],9],['sqrt ( pow ( x , 2 ) + pow ( y , 2 ) )',[['name'=>'x','value'=>3],['name'=>'y','value'=>4]],5]];
   foreach($cases as [$expression,$variables,$expected]){$r=request($app,'POST','/api/evaluate',compact('expression','variables'),$guest);check($r['data']['value']===$expected*1.0,'Wrong result');}
  });
  run('native validation errors',function()use($app,$guest){foreach(['1 / 0','1 +','sqrt ( - 1 )','missing + 1'] as $expression){try{request($app,'POST','/api/evaluate',['expression'=>$expression,'variables'=>[]],$guest);}catch(ApiError $e){check($e->status===422,'Expected calculation error');continue;}throw new RuntimeException('Expected native failure');}});
  run('history isolation',function()use($app,$guest,$other){check(count(request($app,'GET','/api/history',[],$guest)['data']['items'])===3,'Three guest calculations expected');check(request($app,'GET','/api/history',[],$other)['data']['items']===[],'Another guest must not see history');});
 }else{$skipped+=3;echo "SKIP native evaluation/error/isolation tests (set MINAX_TEST_NATIVE)\n";}
 $account=request($app,'POST','/api/auth/register',['username'=>'alice','password'=>'safe-password-123'],$guest);
 run('registration rotates session',function()use($app,$guest,$account){check(!$account['data']['user']['guest'],'Account expected');check($guest['cookie']!==$account['cookie'],'Session must rotate');fails(fn()=>request($app,'GET','/api/history',[],$guest),'SESSION_EXPIRED');});
 run('password stored as hash',function()use($config){$db=new PDO('sqlite:'.$config['database']);$hash=$db->query('SELECT password_hash FROM users')->fetchColumn();check($hash!=='safe-password-123'&&password_verify('safe-password-123',$hash),'Password hash expected');});
 run('guest records retained on login',function()use($app,$account,$config){$expected=is_file($config['native'])?3:0;check(count(request($app,'GET','/api/history',[],$account)['data']['items'])===$expected,'Records must migrate');});
 run('duplicate registration',fn()=>fails(fn()=>request($app,'POST','/api/auth/register',['username'=>'alice','password'=>'safe-password-123'],$other),'USERNAME_TAKEN'));
 run('wrong password',fn()=>fails(fn()=>request($app,'POST','/api/auth/login',['username'=>'alice','password'=>'wrong-password'],$other),'INVALID_CREDENTIALS'));
 $loggedOut=request($app,'POST','/api/auth/logout',[],$account);
 run('logout revokes old session',function()use($app,$account,$loggedOut){check($loggedOut['data']['user']['guest'],'Guest expected');fails(fn()=>request($app,'GET','/api/history',[],$account),'SESSION_EXPIRED');});
 $loggedIn=request($app,'POST','/api/auth/login',['username'=>'alice','password'=>'safe-password-123'],$loggedOut);
 run('login restores account',fn()=>check($loggedIn['data']['user']['name']==='alice','Expected alice'));
 run('clear history',function()use($app,$loggedIn){request($app,'DELETE','/api/history',[],$loggedIn);check(request($app,'GET','/api/history',[],$loggedIn)['data']['items']===[],'History must be empty');});
 run('missing evaluator',function()use($config){$config['native']='/missing/minax';fails(fn()=>(new NativeEvaluator($config))->calculate(['expression'=>'1 + 2','variables'=>[]]),'CALCULATOR_UNAVAILABLE');});
 echo "\n$passed tests passed; $skipped skipped.\n";
}catch(Throwable $e){fwrite(STDERR,"FAIL ".$e->getMessage()."\n");exit(1);}
finally{
 unset($app);
 foreach(glob($directory.'/*') as $file)@unlink($file);
 @rmdir($directory);
}
