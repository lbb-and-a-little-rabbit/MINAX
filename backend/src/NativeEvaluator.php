<?php
declare(strict_types=1);
namespace Minax;
final class NativeEvaluator {
    public function __construct(private array $config) {}
    public function calculate(array $data): array {
        $expression = $data['expression'] ?? null; $variables = $data['variables'] ?? null;
        if (!is_string($expression) || trim($expression) === '' || strlen($expression)>8192 || preg_match('/[^\x20-\x7e\t\r\n]/', $expression)) throw new ApiError('INVALID_EXPRESSION');
        $expression = preg_replace('/\s+/', ' ', trim($expression));
        $parts = explode(' ', $expression);
        if (count($parts)>512) throw new ApiError('EXPRESSION_TOO_LONG');
        $depth = 0;
        foreach($parts as $part){ if($part==='(') $depth++; if($part===')') $depth--; if($depth>64) throw new ApiError('EXPRESSION_TOO_DEEP'); }
        if (!is_array($variables) || !array_is_list($variables) || count($variables)>100) throw new ApiError('INVALID_VARIABLES');
        $seen=[]; $lines=[];
        foreach($variables as $variable){
            if(!is_array($variable))throw new ApiError('INVALID_VARIABLES');
            $name=$variable['name'] ?? null; $value=$variable['value'] ?? null;
            if(!is_string($name) || !preg_match('/^[A-Za-z_][A-Za-z0-9_]*$/D', $name) || isset($seen[$name])) throw new ApiError('INVALID_VARIABLES');
            if((!is_int($value) && !is_float($value)) || !is_finite((float)$value)) throw new ApiError('INVALID_VARIABLES');
            $seen[$name]=true;
            // JSON 数字不受服务器区域设置影响。
            $lines[]=$name.' '.json_encode($value, JSON_THROW_ON_ERROR | JSON_PRESERVE_ZERO_FRACTION);
        }
        $exe=realpath($this->config['native']);
        if(!$exe || !is_file($exe)) throw new ApiError('CALCULATOR_UNAVAILABLE',503);
        $input=count($lines)."\n".($lines ? implode("\n",$lines)."\n" : '').$expression."\n";
        // 使用临时文件避免 Windows 非阻塞管道限制；不把表达式拼进命令行。
        $in=tmpfile();$out=tmpfile();$err=tmpfile();
        if(!$in || !$out || !$err) throw new ApiError('SERVICE_UNAVAILABLE',503);
        $process=null; $started=microtime(true); $exit=-1;
        try {
            fwrite($in,$input);rewind($in);
            $process=proc_open([$exe], [0=>$in,1=>$out,2=>$err], $pipes, dirname($exe), null, ['bypass_shell'=>true]);
            if(!is_resource($process)) throw new ApiError('CALCULATOR_UNAVAILABLE',503);
            while(true){
                $state=proc_get_status($process);
                if(!$state['running']){$exit=$state['exitcode'];break;}
                if(microtime(true)-$started>$this->config['timeout_seconds']){proc_terminate($process,9);throw new ApiError('CALCULATION_TIMEOUT',408);}
                if(fstat($out)['size']+fstat($err)['size']>131072){proc_terminate($process,9);throw new ApiError('CALCULATION_FAILED',422);}
                usleep(10000);
            }
            $closed=proc_close($process);$process=null;if($exit<0)$exit=$closed;
            rewind($out);rewind($err);
            $stdout=stream_get_contents($out,131072);$stderr=stream_get_contents($err,8192);
            if($exit!==0) throw $this->friendlyError($stderr);
            if(!preg_match('/^Result = ([^\r\n]+)\r?$/m',$stdout,$matches) || !is_numeric(trim($matches[1])) || !is_finite((float)$matches[1])) throw new ApiError('CALCULATION_FAILED',422);
            return ['expression'=>$expression,'variables'=>$variables,'displayValue'=>trim($matches[1]),'value'=>(float)$matches[1],'durationMs'=>round((microtime(true)-$started)*1000,2)];
        } finally {
            if(is_resource($process)){proc_terminate($process,9);proc_close($process);}
            fclose($in);fclose($out);fclose($err);
        }
    }
    private function friendlyError(string $message): ApiError {
        // 原生诊断只作服务端排查；网页不展示实现名词或内部路径。
        error_log('[MINAX calculation] '.str_replace(["\r","\n"],' ',substr($message,0,3000)));
        $code = match(true) {
            (bool)preg_match('/division by zero|divide by zero/i',$message) => 'DIVISION_BY_ZERO',
            (bool)preg_match('/undefined variable|unknown variable/i',$message) => 'UNKNOWN_VARIABLE',
            (bool)preg_match('/undefined function|unknown function/i',$message) => 'UNKNOWN_FUNCTION',
            (bool)preg_match('/argument count|arguments|arity/i',$message) => 'INVALID_ARGUMENTS',
            (bool)preg_match('/integer|integral/i',$message) => 'INTEGER_BOUNDS_REQUIRED',
            (bool)preg_match('/domain|finite|overflow|requires x/i',$message) => 'INVALID_DOMAIN',
            (bool)preg_match('/budget|limit exceeded/i',$message) => 'CALCULATION_LIMIT',
            default => 'INVALID_EXPRESSION'
        };
        return new ApiError($code,422);
    }
}
