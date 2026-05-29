# Monitor System Web

Vite + React + TypeScript dashboard for the monitor system `api_gateway`.

## Scripts

```bash
npm install
npm run dev
npm run build
npm run lint
```

## Environment

Copy `.env.example` to `.env.local` when the backend target differs from the defaults.
Docker deployment uses the root `configs/app.env` and the compose file.

```bash
VITE_API_BASE_URL=/api
VITE_API_PROXY_TARGET=http://localhost:8080
VITE_AGENT_API_BASE_URL=/api/agent
VITE_AGENT_API_PROXY_TARGET=http://localhost:6872
```

The development server proxies `/api` and `/health` to `VITE_API_PROXY_TARGET`, and `/api/agent` to `VITE_AGENT_API_PROXY_TARGET`.

## 鉴权

前端启动后会先进入 `/login`。登录成功后，JWT 会保存在浏览器 `localStorage` 中，并自动附加到监控 API 和 AI 运维 API 请求的 `Authorization: Bearer <token>` 请求头。

`admin` 用户会看到“用户管理”入口，可创建 `admin` 或 `user` 账号；普通 `user` 只能访问监控与 AI 运维页面。
