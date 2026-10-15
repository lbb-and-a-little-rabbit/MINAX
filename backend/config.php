<?php
declare(strict_types=1);
$root = dirname(__DIR__);
$config = [
    'database' => getenv('MINAX_DB_PATH') ?: __DIR__ . '/storage/minax.sqlite3',
    'native' => getenv('MINAX_NATIVE_EXE') ?: $root . '/bin/minax_calculator' . (PHP_OS_FAMILY === 'Windows' ? '.exe' : ''),
    'web' => $root . '/frontend/dist',
    'secure_cookie' => getenv('MINAX_SECURE_COOKIE') === '1',
    // 生产填写用户实际访问的 HTTPS 源。本地默认无额外来源；请求仍需防伪验证。
    'allowed_origins' => array_filter(explode(',', getenv('MINAX_ALLOWED_ORIGINS') ?: '')),
    'timeout_seconds' => 8,
    'rate_limits' => true,
];
$local = __DIR__ . '/config.local.php';
if (is_file($local)) $config = array_replace($config, require $local);
return $config;
