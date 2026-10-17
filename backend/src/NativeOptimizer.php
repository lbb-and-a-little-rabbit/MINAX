<?php
declare(strict_types=1);
namespace Minax;

final class NativeOptimizer {
    public function __construct(private array $config) {}

    private function expression(mixed $text, bool $constraint=false): string {
        if(!is_string($text) || trim($text)==='' || strlen($text)>8192 || preg_match('/[^\x20-\x7e\t\r\n]/',$text)) throw new ApiError('INVALID_EXPRESSION');
        $text=preg_replace('/\s+/', ' ', trim($text));
        $parts=explode(' ',$text);
        if(count($parts)>512)throw new ApiError('EXPRESSION_TOO_LONG');
        $depth=0;$relations=0;
        foreach($parts as $part){
            if($part==='(' && ++$depth>64)throw new ApiError('EXPRESSION_TOO_DEEP');
            if($part===')' && --$depth<0)throw new ApiError('INVALID_EXPRESSION');
            if(in_array($part,['<','>','!='],true))throw new ApiError('UNSUPPORTED_CONSTRAINT');
            if(in_array($part,['=','<=','>='],true)){if($depth!==0)throw new ApiError('INVALID_CONSTRAINTS');$relations++;}
        }
        if($depth!==0)throw new ApiError('INVALID_EXPRESSION');
        if($relations!==($constraint?1:0))throw new ApiError($constraint?'INVALID_CONSTRAINTS':'INVALID_EXPRESSION');
        return $text;
    }

    public function calculate(array $data): array {
        $goal=$data['goal'] ?? null;
        if(!in_array($goal,['min','max'],true))throw new ApiError('INVALID_REQUEST');
        $expression=$this->expression($data['expression'] ?? null);
        $variables=$data['variables'] ?? null;$constraints=$data['constraints'] ?? [];
        if(!is_array($variables)||!array_is_list($variables)||count($variables)<1||count($variables)>32)throw new ApiError('INVALID_BOUNDS');
        if(!is_array($constraints)||!array_is_list($constraints)||count($constraints)>64)throw new ApiError('INVALID_CONSTRAINTS');
        $seen=[];$lines=[];
        foreach($variables as $v){
            if(!is_array($v))throw new ApiError('INVALID_BOUNDS');
            $name=$v['name'] ?? null;
            if(!is_string($name)||strlen($name)>64||!preg_match('/^[A-Za-z_][A-Za-z0-9_]*$/D',$name)||isset($seen[$name]))throw new ApiError('INVALID_BOUNDS');
            $seen[$name]=true;
            foreach(['lower','upper','initial'] as $key){$value=$v[$key] ?? null;if((!is_int($value)&&!is_float($value))||!is_finite((float)$value))throw new ApiError('INVALID_BOUNDS');}
            if($v['lower'] >= $v['upper'] || !is_finite($v['upper']-$v['lower']) || $v['initial']<$v['lower'] || $v['initial']>$v['upper'])throw new ApiError('INVALID_BOUNDS');
            $lines[]=$name.' '.json_encode($v['lower'],JSON_THROW_ON_ERROR).' '.json_encode($v['upper'],JSON_THROW_ON_ERROR).' '.json_encode($v['initial'],JSON_THROW_ON_ERROR);
        }
        $constraints=array_map(fn($c)=>$this->expression($c,true),$constraints);
        $input=$goal."\n".count($lines)."\n".implode("\n",$lines)."\n".$expression."\n".count($constraints)."\n".implode("\n",$constraints)."\n";
        if(strlen($input)>65536)throw new ApiError('EXPRESSION_TOO_LONG');
        $exe=realpath($this->config['optimizer']);
        if(!$exe || !is_file($exe) || (PHP_OS_FAMILY!=='Windows' && !is_executable($exe)))throw new ApiError('OPTIMIZER_UNAVAILABLE',503);
        $in=tmpfile();$out=tmpfile();$err=tmpfile();$process=null;
        $start=microtime(true);
        try{
            if(!$in||!$out||!$err)throw new ApiError('SERVICE_UNAVAILABLE',503);
            fwrite($in,$input);rewind($in);
            // 数组命令 + 标准输入，不经过 shell，不拼接用户表达式。
            $process=proc_open([$exe],[0=>$in,1=>$out,2=>$err],$pipes,dirname($exe),null,['bypass_shell'=>true]);
            if(!is_resource($process))throw new ApiError('OPTIMIZER_UNAVAILABLE',503);
            while(true){
                $status=proc_get_status($process);
                if(fstat($out)['size']+fstat($err)['size']>262144)throw new ApiError('OPTIMIZATION_FAILED',502);
                if(!$status['running']){$exit=$status['exitcode'];break;}
                if(microtime(true)-$start>$this->config['optimizer_timeout_seconds'])throw new ApiError('OPTIMIZATION_TIMEOUT',408);
                usleep(10000);
            }
            $closed=proc_close($process);$process=null;if($exit<0)$exit=$closed;
            rewind($out);$raw=stream_get_contents($out,262144);
            try{$r=json_decode($raw,true,64,JSON_THROW_ON_ERROR);}catch(\JsonException){throw new ApiError('OPTIMIZATION_FAILED',502);}
            if(!is_array($r))throw new ApiError('OPTIMIZATION_FAILED',502);
            if($exit===1 && is_string($r['error'] ?? null)){
                $message=$r['error'];
                error_log('[MINAX optimizer] '.str_replace(["\r","\n"],' ',substr($message,0,3000)));
                $code=match(true){
                    (bool)preg_match('/undefined variable/i',$message)=>'UNKNOWN_VARIABLE',
                    (bool)preg_match('/undefined function/i',$message)=>'UNKNOWN_FUNCTION',
                    (bool)preg_match('/argument|arity/i',$message)=>'INVALID_ARGUMENTS',
                    default=>'INVALID_EXPRESSION'
                };
                throw new ApiError($code,422);
            }
            // 退出码 2 是正常的“未找到可行点”，不得转换成成功最值。
            if(!in_array($exit,[0,2],true) || ($r['status'] ?? '')!==($exit===0?'feasible_found':'no_feasible_found'))throw new ApiError('OPTIMIZATION_FAILED',502);
            if(!is_array($r['variables'] ?? null)||array_diff_key($seen,$r['variables'])||array_diff_key($r['variables'],$seen)||!is_array($r['constraints'] ?? null)||count($r['constraints'])!==count($constraints))throw new ApiError('OPTIMIZATION_FAILED',502);
            foreach($r['variables'] as $value)if((!is_int($value)&&!is_float($value))||!is_finite((float)$value))throw new ApiError('OPTIMIZATION_FAILED',502);
            if($exit===0){
                if(!is_numeric($r['objective'] ?? null)||!is_finite((float)$r['objective']))throw new ApiError('OPTIMIZATION_FAILED',502);
                foreach($r['constraints'] as $c)if(($c['satisfied'] ?? false)!==true)throw new ApiError('OPTIMIZATION_FAILED',502);
            }
            $r['globally_certified']=false;
            $r['durationMs']=round((microtime(true)-$start)*1000,2);
            $r['goal']=$goal;
            return $r;
        }finally{
            if(is_resource($process)){proc_terminate($process,9);proc_close($process);}
            foreach([$in,$out,$err] as $file)if(is_resource($file))fclose($file);
        }
    }
}
