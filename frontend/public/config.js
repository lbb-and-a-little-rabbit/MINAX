// 开发由 Vite 转发 /api 到 PHP 8787 端口。部署推荐网页和接口同源。
// 生产可直接修改 dist/config.js，无需重编译。
window.MINAX_CONFIG = Object.freeze({ apiBase: '/api', requestTimeoutMs: 15000, optimizerRequestTimeoutMs: 35000 });
