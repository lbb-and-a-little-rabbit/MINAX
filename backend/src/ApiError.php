<?php
declare(strict_types=1);
namespace Minax;
final class ApiError extends \RuntimeException {
    public function __construct(public string $errorCode, public int $status = 400, public array $details = []) { parent::__construct($errorCode); }
}
