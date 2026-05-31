# monitor_system agent_service 架构概览

## 1. 服务定位

`agent_service` 是 `monitor_system` 中独立运行的 Gin AI 运维服务，负责把监控数据、内部知识库和大模型编排能力连接起来。

- 知识增强对话：`POST /api/agent/chat`
- 流式知识增强对话：`POST /api/agent/chat_stream`
- 运维知识入库：`POST /api/agent/upload`
- AI 运维报告生成：`POST /api/agent/ai_ops`

服务基于 CloudWeGo Eino 构建对话、RAG 检索和 plan-execute-replan 编排流程。

运行时监控事实来自现有 `api_gateway`

## 2. 分层结构

- **服务入口**：`cmd/server/main.go`
- **HTTP 接入层**：`internal/handler/agent/*`、`internal/handler/agent/dto/*`
- **响应与流式输出**：`internal/response/*`、`internal/sse/*`
- **AI 编排层**：`internal/ai/pipeline/chat/*`、`internal/ai/pipeline/knowledge/*`、`internal/ai/pipeline/ops/*`
- **工具与外部集成**：`internal/ai/tools/query_monitor_gateway.go`、`query_internal_docs.go`、`get_current_time.go`
- **知识库存储**：通过 `internal/storage/milvus/client.go` 访问 Milvus，知识库常量位于 `internal/storage/knowledge`
- **记忆系统**：`internal/session/memory` 提供 `MemoryManager`、短期会话窗口、会话摘要、长期记忆治理和内存兼容实现；`internal/storage/memory` 保存记忆表和向量集合常量
- **运行时配置**：`manifest/config/config.yaml`，并支持环境变量覆盖
- **前端入口**：根目录 `web` React 应用中的 `AI 运维` 页面

## 3. 启动流程

1. `cmd/server/main.go` 读取 `docs_dir` / `AGENT_DOCS_DIR`，并写入 `knowledge.FileDir`。
2. 读取 `agent_service_port` / `AGENT_SERVICE_PORT`，默认监听端口为 `6872`。
3. 注册 `/api/agent` 路由组，并挂载 CORS 中间件与统一响应中间件。
4. 绑定 `agent.NewV1()` 处理器后启动 Gin HTTP 服务。

## 4. 核心流程

### 4.1 对话流程

`/api/agent/chat` 和 `/api/agent/chat_stream` 会根据请求中的租户、团队、集群和会话标识加载记忆上下文，构造 `UserMessage`，从 Milvus 检索相关知识片段，渲染对话提示词，并运行带有监控系统工具的 ReAct Agent。

对话上下文按以下顺序组织：

1. 固定系统指令。
2. 当前日期和边界说明。
3. 会话摘要。
4. 最近消息窗口。
5. 长期记忆召回结果。
6. 内部文档 RAG 结果。
7. 当前用户问题。

当前可用的监控工具如下：

- `query_monitor_cluster_overview`：调用 `GET /api/servers/latest` 查询集群概览。
- `query_monitor_anomalies`：调用 `GET /api/servers/:server/anomalies` 查询异常记录；`server_name` 为空时会查询所有服务器。
- `query_monitor_performance`：调用 `GET /api/servers/:server/performance` 查询历史性能数据。
- `query_monitor_trend`：调用 `GET /api/servers/:server/trend` 查询指标趋势。
- `query_monitor_detail`：调用 `GET /api/servers/:server/{net,disk,mem,softirq,mysql}-detail` 查询网络、磁盘、内存、软中断或 MySQL 明细。
- `query_monitor_mysql_detail`：调用 `GET /api/servers/:server/mysql-detail` 查询 MySQL 可用性、连接压力、QPS/TPS、慢查询、锁等待、Buffer Pool 命中率和复制延迟；该工具只通过 `api_gateway` 读取监控事实，不直接访问 MySQL 数据库。
- `query_internal_docs`：查询 Milvus 支撑的内部运维知识库。
- `get_current_time`：提供当前时间戳，用于构造时间窗口查询。

### 4.2 知识上传流程

`/api/agent/upload` 会把上传文件保存到 `knowledge.FileDir`，读取真实落盘路径和文件信息，删除 Milvus 中 `_source` 相同的旧分片，然后重新构建索引。

Milvus 默认配置如下：

- 数据库：`monitor_system_agent`
- Collection：`ops_docs`

### 4.3 AI 运维报告流程

`/api/agent/ai_ops` 会运行固定的中文 plan-execute-replan 任务：

1. 查询集群概览。
2. 查询所有服务器异常。
3. 对异常或低分服务器继续查询性能、趋势或明细指标。
4. 当异常包含 `MYSQL_*`、低分服务器疑似数据库相关、或用户询问数据库问题时，查询 MySQL 明细，并重点查看 `up`、`connection_used_percent`、`qps`、`tps`、`slow_queries_rate`、`innodb_row_lock_waits_rate`、`innodb_buffer_pool_hit_percent`、`replication_lag_seconds`。
5. 检索内部运维文档，查找匹配的处理手册。
6. 输出基于 `monitor_system` 真实数据的中文 AI 运维分析报告。

### 4.4 记忆管理流程

记忆系统使用 `MemoryManager` 作为唯一入口：

- `LoadContext`：按租户、团队、集群和会话加载会话摘要、最近消息和长期记忆。
- `AppendTurn`：在一轮对话完成后追加用户消息和助手回复。
- `SummarizeSession`：维护滚动会话摘要。
- `ExtractDurableMemories`：在长期记忆写入启用后抽取候选长期记忆，并过滤凭据、敏感信息和一次性临时状态。
- `DeleteMemories`：删除长期记忆，清理向量索引，并保留审计事件。

长期记忆默认关闭，可以通过 `/api/agent/memory/policy` 在全局、租户、团队或集群作用域内启用写入和召回。删除中和已删除的长期记忆不会进入召回结果。

## 5. 配置说明

主配置文件：

- `manifest/config/config.yaml`

重要配置项和对应环境变量如下：

- `agent_service_port` / `AGENT_SERVICE_PORT`
- `api_gateway_base_url` / `API_GATEWAY_BASE_URL`
- `milvus_addr` / `MILVUS_ADDR`
- `docs_dir` / `AGENT_DOCS_DIR`
- `think_chat_model.*` / `AGENT_THINK_*`
- `quick_chat_model.*` / `AGENT_QUICK_*`
- `embedding_model.*` / `AGENT_EMBEDDING_*`
- `memory.long_term_enabled` / `AGENT_MEMORY_LONG_TERM_ENABLED`
- `memory.write_enabled` / `AGENT_MEMORY_WRITE_ENABLED`
- `memory.recall_enabled` / `AGENT_MEMORY_RECALL_ENABLED`
- `memory.recent_window` / `AGENT_MEMORY_RECENT_WINDOW`
- `memory.summary_window` / `AGENT_MEMORY_SUMMARY_WINDOW`
- `memory.recall_limit` / `AGENT_MEMORY_RECALL_LIMIT`
- `memory.token_budget` / `AGENT_MEMORY_TOKEN_BUDGET`

仓库提交的配置文件会刻意留空模型密钥，部署或本地运行时应通过环境变量注入。

## 6. 部署关系

根目录 `deploy/docker-compose.yml` 已包含以下服务：

- MySQL
- Redis
- Milvus etcd
- Milvus MinIO
- Milvus standalone
- Attu
- `agent_service`

`agent_service` 通过 `MILVUS_ADDR` 连接 Milvus，通过 `API_GATEWAY_BASE_URL` 获取监控事实。容器部署时，默认把 `API_GATEWAY_BASE_URL` 指向宿主机上的 `api_gateway`。

## 7. 注意事项

- 当前默认运行时仍使用内存兼容记忆实现；长期记忆持久化表结构统一维护在根目录 `sql table/agent_memory_schema.sql`，表说明见根目录 `sql table/README.md`。
- 长期记忆向量集合为 `agent_long_term_memories`，必须和内部文档知识库集合 `ops_docs` 隔离。
- 服务只消费现有 `api_gateway` HTTP API，不改变 C++ `manager` 的 gRPC 契约。
- 依赖可用后，建议在 `agent_service` 目录执行 `go test ./...` 和 `go build ./cmd/server` 做基础验证。
