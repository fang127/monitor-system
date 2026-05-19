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

- **HTTP 接入层**：`main.go`、`api/chat/v1/chat.go`、`internal/controller/chat/*`
- **AI 编排层**：`internal/ai/agent/chat_pipeline/*`、`knowledge_index_pipeline/*`、`plan_execute_replan/*`
- **工具与外部集成**：`internal/ai/tools/query_monitor_gateway.go`、`query_internal_docs.go`、`get_current_time.go`
- **知识库存储**：通过 `utility/client/client.go` 访问 Milvus
- **运行时配置**：`manifest/config/config.yaml`，并支持环境变量覆盖
- **前端入口**：根目录 `web` React 应用中的 `AI 运维` 页面

## 3. 启动流程

1. `main.go` 读取 `docs_dir` / `AGENT_DOCS_DIR`，并写入 `common.FileDir`。
2. 读取 `agent_service_port` / `AGENT_SERVICE_PORT`，默认监听端口为 `6872`。
3. 注册 `/api/agent` 路由组，并挂载 CORS 中间件与统一响应中间件。
4. 绑定 `chat.NewV1()` 控制器后启动 Gin HTTP 服务。

## 4. 核心流程

### 4.1 对话流程

`/api/agent/chat` 和 `/api/agent/chat_stream` 会根据请求构造 `UserMessage`，从 Milvus 检索相关知识片段，渲染对话提示词，并运行带有监控系统工具的 ReAct Agent。

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

`/api/agent/upload` 会把上传文件保存到 `common.FileDir`，读取真实落盘路径和文件信息，删除 Milvus 中 `_source` 相同的旧分片，然后重新构建索引。

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

## 5. 配置说明

主配置文件：

- `manifest/config/config.yaml`

重要配置项和对应环境变量如下：

- `agent_service_port` / `AGENT_SERVICE_PORT`
- `api_gateway_base_url` / `API_GATEWAY_BASE_URL`
- `milvus_addr` / `MILVUS_ADDR`
- `docs_dir` / `AGENT_DOCS_DIR`
- `ds_think_chat_model.*` / `AGENT_THINK_*`
- `ds_quick_chat_model.*` / `AGENT_QUICK_*`
- `doubao_embedding_model.*` / `AGENT_EMBEDDING_*`

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

- 会话记忆仍是进程内存级别，具体实现位于 `utility/mem`。
- 服务只消费现有 `api_gateway` HTTP API，不改变 C++ `manager` 的 gRPC 契约。
- 依赖可用后，建议在 `agent_service` 目录执行 `go test ./...` 做基础验证。
