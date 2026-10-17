# 网页最值求解更新

基于仓库提交 `637557cc9c9fd6ff2ed208770c3c444a8882ade9` 修改。更新包按项目根目录布局，包含修改源码和已构建的 `frontend/dist`，不包含数据库、密码、本地配置、node_modules 或本机编译的二进制。没有替你推送 GitHub 或修改线上服务器。

## 调用流程

网页 `/#/optimize` → `POST /api/optimize` → PHP 输入检查/会话及 CSRF 检查/共享计算槽 → `bin/minax_optimize` → JSON → 页面展示候选值、变量、约束违反量和停止原因。

选择最小值或最大值；每个变量输入有限上下界和初始值；目标表达式一条；约束每行一条。网页自动整理空格，支持已有求值器函数及 sum/prod。支持 `=`、`<=`、`>=`。不支持整数优化、严格比较或 `!=`。数值可行不等于全局最优。求解结果暂在当前页面展示，不写入原有求值历史，避免破坏原有记录结构。

## 服务器更新

将更新包上传到服务器 `/tmp/MINAX-Web-Optimizer-Update.zip`。已有项目应包含上一版 optimizer 源码；此包只更新其 CMake 安装规则，其他算法文件保持仓库版本。

先备份代码（以下备份明确不含运行中的数据库）：

```bash
cd /srv/minax
tar --exclude='backend/storage' --exclude='backend/data' --exclude='frontend/node_modules' \
  -czf /srv/minax-code-before-optimize.tar.gz \
  backend frontend optimizer/CMakeLists.txt .gitignore
```

更新并编译：

```bash
sudo apt update
sudo apt install -y unzip cmake g++ libnlopt-cxx-dev
cd /srv/minax
unzip -o /tmp/MINAX-Web-Optimizer-Update.zip -d /srv/minax
cmake -S optimizer -B build-optimizer \
  -DCMAKE_BUILD_TYPE=Release -DMINAX_WITH_EXPRESSIONS=ON
cmake --build build-optimizer -j1
ctest --test-dir build-optimizer --output-on-failure
cmake --install build-optimizer --prefix /srv/minax
./bin/minax_optimize < optimizer/examples/problem.txt
```

安装步骤只安装 `minax_optimize` 到 `bin`，不会覆盖 `minax_calculator`。

默认 PHP 配置已指向 `/srv/minax/bin/minax_optimize`。自定义路径可在现有 `backend/config.local.php` 返回数组中添加以下两项（保留其余配置，不要整文件替换）：

```php
'optimizer' => '/srv/minax/bin/minax_optimize',
'optimizer_timeout_seconds' => 25,
```

`frontend/dist` 已在包里，可以直接上线。若你在 `frontend/dist/config.js` 改过请求地址，请从备份恢复对应的 `apiBase`；求解请求额外支持 `optimizerRequestTimeoutMs: 35000`，未设置时页面默认 35000 毫秒。后续从源码重建：

```bash
cd /srv/minax/frontend
npm ci
npm run build
```

同源 Nginx `/api/` 转发规则可以继续使用。若另设代理超时，需要大于 PHP 求解硬超时（建议至少 35 秒）。不用新增公开端口。已有 PHP-FPM 若关闭了文件变更检测，需要重载以读取新代码；结合你当前配置：

```bash
sudo systemctl reload php8.4-fpm
```

然后访问原来网站地址加 `/#/optimize`，如 `http://服务器IP:8088/#/optimize`。点击“载入示例”，再点击“开始求解”。预期 `x≈2.5, y≈-0.5, 目标值≈0.5`。

若网页仍显示“未开放”，强制刷新并确认 Nginx 指向本项目 `frontend/dist`。若求解服务不可用，检查 `bin/minax_optimize` 是否存在，PHP-FPM 用户是否能执行，以及 `ldd bin/minax_optimize` 是否缺少 NLopt 动态库。数据库及 storage 目录继续使用原来的权限，不要递归放开整个项目。

## 本机与测试

PHP 启动方法保持不变：

```bash
php -S 127.0.0.1:8787 -t backend/public backend/router.php
```

另开终端在 frontend 下运行 `npm run dev:h5`。Windows 将编译出的程序路径配置为 `optimizer => 'D:/.../minax_optimize.exe'`。

自动测试：

```bash
cd /srv/minax
MINAX_TEST_NATIVE=/srv/minax/bin/minax_calculator php backend/tests/run.php
MINAX_TEST_OPTIMIZER=/srv/minax/bin/minax_optimize php backend/tests/optimizer.php
MINAX_TEST_OPTIMIZER=/srv/minax/bin/minax_optimize node backend/tests/http-optimizer.mjs
cd frontend
npm test
```

本次验证结果：

- 前端 9 项输入处理测试通过；生产构建通过。
- 原有 PHP 18 项回归测试通过，包括原生求值、账号、历史隔离。
- 新增 PHP 求解集成测试通过：等式最小值、最大值、sum、>=、无可行点、非法输入、鉴权、并发计算槽、超时。
- 实际 HTTP 测试通过：游客会话 → CSRF → 求解 → 原生 JSON，以及 422/403 错误和网页入口。
- 没有完成浏览器自动化点击和视觉检查；本环境浏览器安装失败。上线后按上述示例检查页面操作。

## 协议

请求（使用已获取的会话 Cookie 与 X-CSRF-Token）：

```json
{
  "goal": "min",
  "expression": "( x - 2 ) * ( x - 2 ) + ( y + 1 ) * ( y + 1 )",
  "variables": [
    {"name":"x","lower":-5,"upper":5,"initial":0},
    {"name":"y","lower":-5,"upper":5,"initial":0}
  ],
  "constraints": ["x + y = 2"]
}
```

`feasible_found` 只代表找到满足数值容差的候选解；`no_feasible_found` 是正常搜索结果，HTTP 200，页面不把候选点当最值显示。函数/变量错误返回 422，缺少会话或防伪校验失败为 401/403，进程缺失为 503，硬超时为 408。求解器软预算 20 秒，PHP 硬超时 25 秒，浏览器默认等待 35 秒。进程用参数数组启动，表达式只写入标准输入。

原有两个计算槽由求值和求解共享，繁忙返回 429。求解请求默认每 IP 每分钟最多 10 次；现有 rate_limits 配置继续有效。生产部署应限制整体并发，不要为每次页面请求启动无限制后台任务。
