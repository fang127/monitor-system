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
