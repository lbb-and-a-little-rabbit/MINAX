<?php
declare(strict_types=1);
require_once dirname(__DIR__).'/bootstrap.php';
$config=require dirname(__DIR__).'/config.php';
header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');
header('X-Content-Type-Options: nosniff');
header('X-Frame-Options: DENY');
$origin=$_SERVER['HTTP_ORIGIN'] ?? '';
if(in_array($origin,$config['allowed_origins'],true)){
    header('Access-Control-Allow-Origin: '.$origin);
    header('Access-Control-Allow-Credentials: true');
    header('Access-Control-Allow-Headers: Content-Type, X-CSRF-Token');
    header('Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS');
    header('Vary: Origin');
}
if($_SERVER['REQUEST_METHOD']==='OPTIONS'){
    http_response_code(in_array($origin,$config['allowed_origins'],true)?204:403);
    exit;
}
try {
    $headers=getallheaders();
    if((int)($_SERVER['CONTENT_LENGTH'] ?? 0)>131072)throw new Minax\ApiError('REQUEST_TOO_LARGE',413);
    $stream=fopen('php://input','rb');$raw=stream_get_contents($stream,131073);fclose($stream);
    if(strlen($raw)>131072)throw new Minax\ApiError('REQUEST_TOO_LARGE',413);
    $body=[];
    if($raw!==''){
        try{$body=json_decode($raw,true,64,JSON_THROW_ON_ERROR);}catch(JsonException){throw new Minax\ApiError('INVALID_REQUEST');}
        if(!is_array($body) || !str_starts_with(ltrim($raw),'{'))throw new Minax\ApiError('INVALID_REQUEST');
    }
    $path=parse_url($_SERVER['REQUEST_URI'],PHP_URL_PATH);
    $result=(new Minax\Application($config))->handle($_SERVER['REQUEST_METHOD'],$path,$body,$headers,$_COOKIE['minax_session'] ?? null,$_SERVER['REMOTE_ADDR'] ?? 'local');
    if(!empty($result['cookie']))setcookie('minax_session',$result['cookie'],['expires'=>time()+($result['data']['user']['guest'] ? 86400 : 604800),'path'=>'/','secure'=>$config['secure_cookie'],'httponly'=>true,'samesite'=>'Lax']);
    echo json_encode($result['data'],JSON_THROW_ON_ERROR|JSON_UNESCAPED_UNICODE);
}catch(Minax\ApiError $e){http_response_code($e->status);echo json_encode(['error'=>['code'=>$e->errorCode,'details'=>$e->details]]);}
catch(Throwable $e){error_log((string)$e);http_response_code(500);echo '{"error":{"code":"SERVICE_UNAVAILABLE"}}';}
