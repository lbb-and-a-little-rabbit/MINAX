<?php
// 本机开发：php -S 127.0.0.1:8787 -t backend/public backend/router.php
// 不将项目根目录作为公开目录，数据库与配置始终不可直接访问。
declare(strict_types=1);
$path=rawurldecode(parse_url($_SERVER['REQUEST_URI'],PHP_URL_PATH) ?: '/');
if(str_starts_with($path,'/api/')){require __DIR__.'/public/index.php';return;}
$config=require __DIR__.'/config.php';
$root=realpath($config['web']);
$file=$root ? realpath($root.($path==='/'?'/index.html':$path)) : false;
if(!$file || !is_file($file) || !str_starts_with($file,$root.DIRECTORY_SEPARATOR)){http_response_code(404);echo 'Not found';return;}
$types=['html'=>'text/html; charset=utf-8','js'=>'text/javascript; charset=utf-8','css'=>'text/css; charset=utf-8','png'=>'image/png','svg'=>'image/svg+xml','ico'=>'image/x-icon'];
header('Content-Type: '.($types[pathinfo($file,PATHINFO_EXTENSION)] ?? 'application/octet-stream'));
header('X-Content-Type-Options: nosniff');
if(basename($file)==='config.js')header('Cache-Control: no-store');
readfile($file);
