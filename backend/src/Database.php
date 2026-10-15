<?php
declare(strict_types=1);
namespace Minax;
final class Database {
    public static function connect(string $path): \PDO {
        if (!is_dir(dirname($path)) && !mkdir(dirname($path), 0700, true) && !is_dir(dirname($path))) throw new \RuntimeException('Cannot create data directory');
        $db = new \PDO('sqlite:' . $path, null, null, [\PDO::ATTR_ERRMODE => \PDO::ERRMODE_EXCEPTION, \PDO::ATTR_DEFAULT_FETCH_MODE => \PDO::FETCH_ASSOC]);
        $db->exec('PRAGMA foreign_keys = ON; PRAGMA busy_timeout = 5000;');
        if ((int)$db->query('PRAGMA user_version')->fetchColumn() === 0) $db->exec(file_get_contents(__DIR__ . '/../database/schema.sql'));
        return $db;
    }
}
