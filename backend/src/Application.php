<?php
declare(strict_types=1);
namespace Minax;
final class Application {
    private \PDO $db;
    public function __construct(private array $config) { $this->db=Database::connect($config['database']); }
    private function query(string $sql, array $params=[]): \PDOStatement { $s=$this->db->prepare($sql);$s->execute($params);return $s; }
    private function limit(string $bucket,int $maximum,int $window): void {
        if(!$this->config['rate_limits'])return;
        $now=time();
        $this->query('DELETE FROM rate_limits WHERE window_start < ?',[$now-3600]);
        $this->query('INSERT INTO rate_limits(bucket,window_start,attempts) VALUES(?,?,1) ON CONFLICT(bucket) DO UPDATE SET attempts=CASE WHEN window_start<=? THEN 1 ELSE attempts+1 END, window_start=CASE WHEN window_start<=? THEN ? ELSE window_start END',[$bucket,$now,$now-$window,$now-$window,$now]);
        if((int)$this->query('SELECT attempts FROM rate_limits WHERE bucket=?',[$bucket])->fetchColumn()>$maximum)throw new ApiError('TOO_MANY_REQUESTS',429);
    }
    private function lookup(?string $cookie): ?array {
        if(!$cookie)return null;
        $s=$this->query('SELECT s.*,u.username FROM sessions s LEFT JOIN users u ON u.id=s.user_id WHERE id_hash=? AND expires_at>?',[hash('sha256',$cookie),time()])->fetch();
        return $s ?: null;
    }
    private function profile(array $session): array { return ['name'=>$session['username'] ?? '', 'guest'=>$session['user_id']===null]; }
    private function rotate(?array $old,?int $userId=null): array {
        $raw=bin2hex(random_bytes(32));$id=hash('sha256',$raw);$csrf=bin2hex(random_bytes(32));$now=time();
        $this->db->beginTransaction();
        try {
            $this->query('DELETE FROM sessions WHERE expires_at<=?',[$now]);
            $this->query('INSERT INTO sessions(id_hash,user_id,csrf,expires_at,created_at) VALUES(?,?,?,?,?)',[$id,$userId,$csrf,$now+($userId ? 604800 : 86400),$now]);
            // 登录后保留本次游客计算，转为当前账号的历史。
            if($old){
                if($userId && $old['user_id']===null)$this->query('UPDATE calculations SET user_id=?,guest_session=NULL WHERE guest_session=?',[$userId,$old['id_hash']]);
                $this->query('DELETE FROM sessions WHERE id_hash=?',[$old['id_hash']]);
            }
            if($userId)$this->query('DELETE FROM calculations WHERE user_id=? AND id NOT IN (SELECT id FROM calculations WHERE user_id=? ORDER BY id DESC LIMIT 30)',[$userId,$userId]);
            $this->db->commit();
        }catch(\Throwable $e){$this->db->rollBack();throw $e;}
        return ['raw'=>$raw,'session'=>$this->lookup($raw)];
    }
    public function handle(string $method,string $path,array $body,array $headers,?string $cookie,string $ip): array {
        $headers=array_change_key_case($headers,CASE_LOWER);
        $session=$this->lookup($cookie);
        // 不依赖代理传入的任意 IP 头；服务器迁移时在受信代理处完成限流。
        $ip=hash('sha256',$ip);
        $origin=$headers['origin'] ?? '';
        $host=$headers['host'] ?? '';
        if($origin && !in_array($origin,['http://'.$host,'https://'.$host,...$this->config['allowed_origins']],true))throw new ApiError('REQUEST_REJECTED',403);
        if($method==='GET' && $path==='/api/session'){
            $raw=null;
            if(!$session){$this->limit('session:'.$ip,60,300);$rotated=$this->rotate(null);$raw=$rotated['raw'];$session=$rotated['session'];}
            return ['data'=>['user'=>$this->profile($session),'csrf'=>$session['csrf']],'cookie'=>$raw];
        }
        if(!$session)throw new ApiError('SESSION_EXPIRED',401);
        if($method!=='GET'){
            if(!hash_equals($session['csrf'],$headers['x-csrf-token'] ?? ''))throw new ApiError('REQUEST_REJECTED',403);
            if(!str_starts_with(strtolower($headers['content-type'] ?? ''),'application/json'))throw new ApiError('INVALID_REQUEST');
        }
        if($method==='POST' && in_array($path,['/api/auth/register','/api/auth/login'],true)){
            $this->limit('auth:'.$ip,20,300);
            $name=$body['username'] ?? null;$password=$body['password'] ?? null;
            if(!is_string($name)||!preg_match('/^[A-Za-z0-9_]{3,32}$/D',$name)||!is_string($password)||!preg_match('/^[\x20-\x7e]{8,72}$/D',$password))throw new ApiError('INVALID_CREDENTIAL_FORMAT');
            if($path==='/api/auth/register'){
                try{$this->query('INSERT INTO users(username,password_hash,created_at) VALUES(?,?,?)',[$name,password_hash($password,PASSWORD_DEFAULT),time()]);}
                catch(\PDOException $e){if($e->getCode()==='23000')throw new ApiError('USERNAME_TAKEN',409);throw $e;}
                $id=(int)$this->db->lastInsertId();
            }else{
                $u=$this->query('SELECT * FROM users WHERE username=?',[$name])->fetch();
                // 固定合法虚拟 hash，避免不存在的用户名跳过密码运算。
                $valid=password_verify($password,$u['password_hash'] ?? '$2y$10$92IXUNpkjO0rOQ5byMi.Ye4oKoEa3Ro9llC/.og/at2uheWG/igi.');
                if(!$u || !$valid)throw new ApiError('INVALID_CREDENTIALS',401);
                $id=(int)$u['id'];
                if(password_needs_rehash($u['password_hash'],PASSWORD_DEFAULT))$this->query('UPDATE users SET password_hash=? WHERE id=?',[password_hash($password,PASSWORD_DEFAULT),$id]);
            }
            $r=$this->rotate($session,$id);
            return ['data'=>['user'=>$this->profile($r['session']),'csrf'=>$r['session']['csrf']],'cookie'=>$r['raw']];
        }
        if($method==='POST' && $path==='/api/auth/logout'){
            $r=$this->rotate($session);return ['data'=>['user'=>$this->profile($r['session']),'csrf'=>$r['session']['csrf']],'cookie'=>$r['raw']];
        }
        $column=$session['user_id']===null?'guest_session':'user_id';
        $owner=$session['user_id'] ?? $session['id_hash'];
        if($method==='GET' && $path==='/api/history'){
            $rows=$this->query("SELECT id,expression,variables_json,result_text,duration_ms,created_at FROM calculations WHERE $column=? ORDER BY id DESC LIMIT 30",[$owner])->fetchAll();
            return ['data'=>['items'=>array_map(static fn($r)=>['id'=>(int)$r['id'],'expression'=>$r['expression'],'variables'=>json_decode($r['variables_json'],true),'displayValue'=>$r['result_text'],'durationMs'=>(float)$r['duration_ms'],'created'=>(int)$r['created_at']],$rows)]];
        }
        if($method==='DELETE' && $path==='/api/history'){$this->query("DELETE FROM calculations WHERE $column=?",[$owner]);return ['data'=>['ok'=>true]];}
        if($method==='POST' && in_array($path,['/api/evaluate','/api/optimize'],true)){
            $optimize=$path==='/api/optimize';
            $this->limit(($optimize?'optimize:':'evaluate:').$ip,$optimize?10:60,60);
            $lock=null;
            for($i=0;$i<2;$i++){
                $candidate=fopen(dirname($this->config['database']).'/worker-'.$i.'.lock','c');
                if($candidate && flock($candidate,LOCK_EX|LOCK_NB)){$lock=$candidate;break;}
                if($candidate)fclose($candidate);
            }
            if(!$lock)throw new ApiError('BUSY',429);
            try{$result=$optimize ? (new NativeOptimizer($this->config))->calculate($body) : (new NativeEvaluator($this->config))->calculate($body);}finally{flock($lock,LOCK_UN);fclose($lock);}
            if($optimize)return ['data'=>$result];
            $this->query('INSERT INTO calculations(user_id,guest_session,expression,variables_json,result_text,duration_ms,created_at) VALUES(?,?,?,?,?,?,?)',[$session['user_id'],$session['user_id']===null?$session['id_hash']:null,$result['expression'],json_encode($result['variables'],JSON_THROW_ON_ERROR),$result['displayValue'],$result['durationMs'],time()]);
            $this->query("DELETE FROM calculations WHERE $column=? AND id NOT IN (SELECT id FROM calculations WHERE $column=? ORDER BY id DESC LIMIT 30)",[$owner,$owner]);
            return ['data'=>$result];
        }
        throw new ApiError('NOT_FOUND',404);
    }
}
