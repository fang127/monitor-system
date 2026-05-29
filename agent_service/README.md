# agent_service

`agent_service` 是 `monitor_system` 的 AI 运维服务，提供知识增强对话、流式对话、运维文档入库和自动 AI 运维报告能力。它通过 `api_gateway` 获取监控事实，通过 Milvus 存储和检索内部运维知识，并使用 CloudWeGo Eino 编排 RAG、工具调用和 plan-execute-replan 流程。

## 功能

- `POST /api/agent/chat`：普通对话接口，支持基于会话 ID 的进程内上下文记忆。
- `POST /api/agent/chat_stream`：SSE 流式对话接口。
- `POST /api/agent/upload`：上传运维文档并写入 Milvus 知识库。
- `POST /api/agent/ai_ops`：查询监控事实和内部文档，生成中文 AI 运维分析报告。

所有 `/api/agent/*` 接口都要求携带与 `api_gateway` 相同密钥签发的 JWT。服务校验通过后，会在调用 `api_gateway` 查询监控事实时透传同一个 `Authorization: Bearer <token>`。

## 目录结构

```text
agent_service/
├── cmd/server/                   # HTTP 服务启动入口
├── cmd/tools/                    # 本地调试和知识入库工具
├── internal/config/              # 配置文件与环境变量读取
├── internal/handler/agent/       # Agent HTTP 处理器和 DTO
├── internal/response/            # 统一 JSON 响应与 CORS 中间件
├── internal/sse/                 # SSE 连接与事件写入
├── internal/session/memory/      # 进程内会话记忆
├── internal/storage/             # Milvus 客户端和知识库常量
├── internal/ai/pipeline/         # 对话、知识索引、AI Ops 编排流程
├── internal/ai/tools/            # 监控查询、知识库查询、时间工具
├── manifest/config/config.yaml   # 默认运行配置
├── manifest/docker/              # Docker 构建与单独 compose 文件
├── docs/                         # 默认知识文档目录
└── architecture_overview.md      # 架构说明
```

## 运行前准备

需要先准备以下依赖：

- Go `1.24+`
- 可访问的 Milvus，默认地址为 `127.0.0.1:19530`
- 可访问的 `api_gateway`，默认地址为 `http://127.0.0.1:8080`
- 可用的大模型与 Embedding 模型配置

模型密钥不应写入仓库配置文件，建议通过环境变量注入。

## 配置

默认配置位于 `manifest/config/config.yaml`。常用配置和环境变量覆盖关系如下：

| 配置项 | 环境变量 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `agent_service_port` | `AGENT_SERVICE_PORT` | `6872` | 服务监听端口 |
| `jwt_secret` | `JWT_SECRET` | `monitor-system-dev-secret` | JWT HS256 校验密钥，需要与 `api_gateway` 一致 |
| `api_gateway_base_url` | `API_GATEWAY_BASE_URL` | `http://127.0.0.1:8080` | 监控 API 网关地址 |
| `milvus_addr` | `MILVUS_ADDR` | `127.0.0.1:19530` | Milvus 地址 |
| `docs_dir` | `AGENT_DOCS_DIR` | `./docs` | 上传文档保存目录 |
| `ds_think_chat_model.*` | `AGENT_THINK_*` | 空 | 规划、推理类模型配置 |
| `ds_quick_chat_model.*` | `AGENT_QUICK_*` | 空 | 快速对话类模型配置 |
| `doubao_embedding_model.*` | `AGENT_EMBEDDING_*` | `text-embedding-v4` | Embedding 模型配置 |

示例：

```bash
export AGENT_SERVICE_PORT=6872
export JWT_SECRET=please-change-this-jwt-secret
export API_GATEWAY_BASE_URL=http://127.0.0.1:8080
export MILVUS_ADDR=127.0.0.1:19530
export AGENT_DOCS_DIR=./docs
export AGENT_THINK_API_KEY=your-api-key
export AGENT_THINK_BASE_URL=https://example.com/v1
export AGENT_THINK_MODEL=your-think-model
export AGENT_QUICK_API_KEY=your-api-key
export AGENT_QUICK_BASE_URL=https://example.com/v1
export AGENT_QUICK_MODEL=your-quick-model
export AGENT_EMBEDDING_API_KEY=your-api-key
export AGENT_EMBEDDING_MODEL=text-embedding-v4
```

## 本地启动

在 `agent_service` 目录执行：

```bash
go mod download
go run ./cmd/server
```

服务启动后默认监听：

```text
http://127.0.0.1:6872/api/agent
```

## Docker 部署

从项目根目录通过统一配置启动容器化模块：

```bash
docker compose --env-file configs/app.env -f deploy/docker-compose.yml up -d
```

完整 compose 会包含 MySQL、Redis、Milvus、Attu、`agent_service`、`api_gateway` 和 `web`。容器内 `agent_service` 默认通过 `MILVUS_ADDR=milvus:19530` 访问 Milvus，通过 `API_GATEWAY_BASE_URL=http://api_gateway:8080` 访问容器内的 `api_gateway`。

也可以只在 `agent_service/manifest/docker` 下使用单独的 Milvus compose 文件准备向量库环境。

## 接口示例

普通对话：

```bash
curl -X POST http://127.0.0.1:6872/api/agent/chat \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer <access_token>" \
  -d '{"Id":"demo-user","Question":"当前集群健康情况怎么样？"}'
```

上传文档：

```bash
curl -X POST http://127.0.0.1:6872/api/agent/upload \
  -H "Authorization: Bearer <access_token>" \
  -F "file=@./docs/告警处理手册.md"
```

生成 AI 运维报告：

```bash
curl -X POST http://127.0.0.1:6872/api/agent/ai_ops \
  -H "Authorization: Bearer <access_token>"
```

## Agent 工具

`agent_service` 的对话 Agent 和 AI Ops Agent 只通过 `api_gateway` 获取监控事实，不直接访问 MySQL 数据库。当前监控查询工具包括：

- `query_monitor_cluster_overview`：查询集群概览。
- `query_monitor_anomalies`：查询服务器异常记录。
- `query_monitor_performance`：查询单台服务器历史性能数据。
- `query_monitor_trend`：查询单台服务器指标趋势。
- `query_monitor_detail`：查询网络、磁盘、内存、软中断或 MySQL 明细；MySQL 可通过 `kind=mysql` 使用。
- `query_monitor_mysql_detail`：查询 `GET /api/servers/:server/mysql-detail`，返回 MySQL 可用性、连接压力、QPS/TPS、慢查询、锁等待、Buffer Pool 命中率和复制延迟等明细。

## 前端入口

AI 运维界面已集成到根目录 `web` React 应用中，侧边栏点击 `AI 运维` 即可进入。

默认请求地址为 `/api/agent`，如前端与 `agent_service` 分开部署，可通过 `VITE_AGENT_API_BASE_URL` 覆盖。

## 开发验证

在依赖和环境变量准备好后，可在 `agent_service` 目录执行：

```bash
PATH=/usr/local/go/bin:$HOME/go/bin:$PATH GOCACHE=/tmp/monitor-system-go-cache-agent go test ./...
PATH=/usr/local/go/bin:$HOME/go/bin:$PATH go build ./cmd/server
```

如果测试或本地运行需要调用模型、Milvus 或 `api_gateway`，请先确认对应服务和环境变量可用。
