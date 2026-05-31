# web

`web` 是 `monitor-system` 的 React 前端控制台，使用 Vite、TypeScript 和 ECharts 构建。它通过 `api_gateway` 读取监控数据，通过 `agent_service` 调用 AI 运维能力。

## 功能页面

- `/login`：登录页面，调用 `POST /api/auth/login` 获取 JWT。
- `/`：集群总览，展示服务器健康状态和核心统计。
- `/servers`：服务器列表和最新评分。
- `/servers/:server`：单台服务器概览。
- `/servers/:server/performance`：历史性能数据。
- `/servers/:server/trend`：指标趋势。
- `/servers/:server/anomalies`：异常数据。
- `/servers/:server/details/:kind`：网络、磁盘、内存、软中断、MySQL、Redis 明细。
- `/ai-ops`：AI 运维对话、报告和文档上传入口。
- `/system`：系统信息页面。
- `/users`：用户管理，仅 `admin` 角色可访问。

## 目录结构

```text
web/
├── src/
│   ├── api/              # axios client、监控 API、鉴权 API、Agent API
│   ├── auth/             # 登录状态、JWT 保存和恢复
│   ├── components/       # 通用布局、图表、表格、分页和状态组件
│   ├── pages/            # 页面级组件
│   ├── types/            # API 与页面数据类型
│   ├── utils/            # 格式化工具
│   ├── App.tsx           # 路由定义
│   └── main.tsx          # 应用入口
├── .env.example          # 本地开发环境变量示例
├── package.json
└── README.md
```

## 环境变量

复制 `.env.example` 为 `.env.local` 后按需调整：

```bash
cp .env.example .env.local
```

| 变量 | 默认值 | 说明 |
|---|---|---|
| `VITE_API_BASE_URL` | `/api` | 浏览器访问监控 API 的基础路径。 |
| `VITE_API_PROXY_TARGET` | `http://localhost:8080` | Vite 开发服务器把 `/api` 和 `/health` 代理到的 api_gateway 地址。 |
| `VITE_AGENT_API_BASE_URL` | `/api/agent` | 浏览器访问 AI 运维 API 的基础路径。 |
| `VITE_AGENT_API_PROXY_TARGET` | `http://localhost:6872` | Vite 开发服务器把 `/api/agent` 代理到的 agent_service 地址。 |

Docker 部署时，前端镜像由根目录 `deploy/docker-compose.yml` 编排，运行配置主要来自根目录 `configs/app.env`。

## 本地开发

```bash
npm install
npm run dev
```

默认开发地址：

```text
http://127.0.0.1:5173
```

开发服务器代理规则：

- `/api` 和 `/health` 转发到 `VITE_API_PROXY_TARGET`。
- `/api/agent` 转发到 `VITE_AGENT_API_PROXY_TARGET`。

启动前需要确认 `api_gateway` 和 `agent_service` 已可访问；如果只调试监控页面，`agent_service` 可以暂时不可用，但 AI 运维页面会报错。

## 构建与检查

```bash
npm run build
npm run lint
```

构建产物输出到：

```text
web/dist/
```

## 鉴权与角色

前端启动后会先进入 `/login`。登录成功后，JWT 会保存在浏览器 `localStorage` 中，并自动附加到监控 API 和 AI 运维 API 请求的 `Authorization: Bearer <token>` 请求头。登录表单支持填写 `tenant_id` 和 `team_id`；如果账号只有一个有效团队，后端会自动选择该团队，如果账号有多个团队则需要显式选择。

登录态会保存当前租户、团队和可切换团队列表。侧栏会展示当前租户/团队，并在用户拥有多个团队时提供团队切换；切换成功后会刷新访问令牌。侧栏的“集群 ID”会作为当前集群过滤条件附加到监控查询和 AI 运维聊天请求中。

`admin` 用户会看到“用户管理”入口，可创建 `admin` 或 `user` 账号；普通 `user` 只能访问监控与 AI 运维页面。

## 部署

根目录统一启动：

```bash
docker compose --env-file configs/app.env -f deploy/docker-compose.yml up -d web
```

容器化部署时，前端通常由 Nginx 提供静态资源，并把 API 请求转发到 Compose 网络内的 `api_gateway` 和 `agent_service`。具体服务地址和端口见根目录 `deploy/docker-compose.yml`。
