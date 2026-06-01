## Context

`agent_service` 当前已有 `MemoryManager`、`MemoryStore`、`VectorIndex` 等抽象，但运行实现仍偏“内存兼容版”：短期会话、长期记忆、策略和向量索引都可以落在 `InMemoryStore`，长期记忆抽取依赖显式关键词，召回使用简单文本匹配，并且无向量命中时会 fallback 到最近长期记忆。

这些实现适合单元测试和本地演示，却不适合生产排障助手。生产环境需要把热状态、权威治理和语义索引拆开：短期会话要快，长期记忆要可治理，语义召回要相关且不能污染 prompt。

本设计明确 V2 不兼容旧简单内存系统。聊天主流程不得再依赖 `SimpleMemory` 或内存长期记忆；长期记忆默认仍可通过配置关闭，但关闭时系统应退化为“固定指令 + 短期会话 + 摘要”的安全模式。

## Goals / Non-Goals

**Goals:**

- 删除旧的 `SimpleMemory` 聊天路径和内存长期记忆主路径。
- 使用 Redis/本地 LRU 保存短期最近消息和滚动摘要，避免每轮聊天打 MySQL。
- 使用 MySQL 保存长期记忆正文、状态、策略、审计事件和可选摘要 checkpoint。
- 使用独立 Milvus collection `agent_long_term_memories` 保存长期记忆向量，不复用 `ops_docs`。
- 将召回改成“Milvus 候选 ID + MySQL 回表过滤”，无相似命中返回空。
- 保持长期记忆精确作用域召回，不自动把 team/tenant 级记忆混入 cluster 会话。
- 使用 LLM 输出结构化 JSON 候选记忆，并通过规则护栏过滤敏感、临时、未确认和实时指标内容。
- 支持去重、合并、冲突检测和高风险候选 pending 治理。
- 给抽取、召回、更新、删除、失败路径补审计和日志。
- 给管理接口补充 query、limit、status、越权、删除中和过期过滤测试。

**Non-Goals:**

- 不实现跨租户、跨团队或跨作用域的自动记忆继承。
- 不把 MySQL 作为短期会话每轮读写主路径。
- 不把 Milvus 作为长期记忆权威正文存储。
- 不保存密码、token、一次性排障状态、未确认猜测或实时指标值。
- 不在本变更中建设完整人工审核 UI；先通过状态字段和管理 API 支持 pending 治理。

## Decisions

### 1. 短期会话采用 Redis/LRU 热存储，而不是 MySQL 主路径

短期会话最近消息是高频热状态。每轮聊天都会读最近消息、模型完成后追加用户和助手消息，并可能更新摘要。如果直接读写 MySQL，会增加延迟和 JSON 更新写放大，也会让 MySQL 承担不适合它的热状态职责。

V2 引入 `SessionMemoryStore`：

```go
type SessionMemoryStore interface {
    LoadSession(ctx context.Context, scope MemoryScope) (*SessionMemory, error)
    SaveSession(ctx context.Context, session *SessionMemory) error
    DeleteSession(ctx context.Context, scope MemoryScope) error
}
```

实现优先级：

```text
进程内 LRU
  ↓ 未命中
agent_service 自己的 Redis
  ↓ 可选 checkpoint
agent_service 自己的 MySQL 摘要快照
```

Redis key 使用 `tenant/team/cluster/user/session` 维度，设置 TTL。LRU 用于同一会话连续对话的一级缓存。Redis 使用 `AGENT_REDIS_*` / `agent_redis` 配置和 agent_service 自己的 Go 客户端，不复用 manager 的 Redis 连接或 `MANAGER_*` 配置。MySQL 只保存摘要 checkpoint 或治理需要的低频快照，不保存每轮 recent messages 主数据。

替代方案是沿用 `agent_session_memories` 表保存 recent messages。该方案实现直观，但高频写入和 JSON 更新压力过大，因此不作为主路径。

### 2. 长期记忆由 MySQL 做权威存储，Milvus 只做语义索引

长期记忆需要状态、来源、置信度、敏感度、原因、过期时间、创建者、更新历史和删除审计。MySQL 适合做权威存储，Milvus 适合做语义候选召回。MySQL 使用 `AGENT_MYSQL_*` / `agent_mysql` 配置和 agent_service 自己的 Go 依赖与连接池，不复用 manager 的 MySQL 连接或 `MANAGER_*` 配置。

召回流程：

```text
用户问题
  │
  ▼
Embed(query)
  │
  ▼
Milvus 搜索 agent_long_term_memories
  │ 返回候选 memory_id + score
  ▼
按相似度阈值过滤
  │
  ▼
MySQL 回表
  │ 过滤 tenant/team/cluster/status/expires_at/sensitivity
  ▼
按 score + updated_at + confidence 排序
  │
  ▼
按 token 预算注入 prompt
```

如果 Milvus 没有返回满足阈值的候选，系统必须返回空长期记忆，不得 fallback 到最近记忆。

### 3. 长期记忆保持精确作用域召回

本变更不做 `cluster + team + tenant` 混合召回。作用域规则为：

```text
cluster 会话：tenant_id + team_id + cluster_id 精确匹配
team 会话：tenant_id + team_id + cluster_id='' 精确匹配
```

这样可以避免团队级偏好或其他集群经验污染当前集群排障。后续如果需要“显式继承”能力，应另行设计为可配置策略，而不是默认行为。

### 4. 结构化 LLM 抽取替代关键词抽取

新增 `MemoryExtractor` 抽象，默认使用 quick model 输出 JSON：

```json
{
  "candidates": [
    {
      "type": "cluster_knowledge",
      "content": "prod-a 的 Redis 使用哨兵模式，故障切换后先检查 master_last_io_seconds_ago。",
      "scope_level": "cluster",
      "confidence": 0.86,
      "reason": "用户描述了稳定集群拓扑和固定排查流程",
      "expires_at": null,
      "sensitivity": "low",
      "should_store": true
    }
  ]
}
```

Prompt 明确只允许保存稳定信息：偏好、环境约定、集群拓扑、已确认事故结论和团队流程。禁止保存密码、token、一次性排障中间状态、未确认猜测和实时指标值。

规则护栏在 LLM 之后执行，不能只相信模型输出。高风险、低置信度或存在冲突的候选进入 `pending` 或直接丢弃。

### 5. 去重、合并和冲突检测进入写入链路

写入新候选前先生成 embedding，并在同作用域、同类型内搜索相似记忆：

- 高相似且不冲突：更新旧记忆内容、置信度、原因和更新时间。
- 高相似但语义冲突：新候选标记为 `pending`，记录冲突详情，等待治理。
- 低相似：创建新记忆。

第一版冲突检测可采用规则 + LLM 判定混合方式。规则负责明显互斥表达，LLM 负责解释冲突原因。

### 6. 会话摘要升级为真实摘要或分层摘要

当前摘要只是拼接窗口消息。V2 使用摘要器生成稳定的会话摘要，至少包含：当前问题、已确认事实、已执行查询、助手结论、待处理事项。摘要不得声称历史指标仍然实时有效，prompt 中必须区分摘要与实时监控查询结果。

摘要更新可以异步或在聊天完成后执行。失败时记录日志，不阻塞用户拿到模型回答。

### 7. 管理接口按治理语义补全查询和状态行为

长期记忆管理接口需要支持：

- `query`：按文本或语义检索长期记忆。
- `limit`：限制返回数量，并设置默认上限。
- `status`：查询 active、pending、deleting、deleted 等状态。
- 越权保护：普通用户不得访问其他租户或团队。
- 删除状态：deleting/deleted 不得被聊天召回。
- 过期过滤：过期记忆不得被聊天召回。

### 8. 错误和审计必须可观察

`AppendTurn()`、`SummarizeSession()`、`ExtractDurableMemories()`、向量写入、向量删除和 MySQL 回表失败都必须记录日志。抽取、召回、创建、更新、冲突、删除和重试删除必须写审计事件；审计失败不能静默吞掉，应至少打日志。

## Risks / Trade-offs

- Redis 依赖增加 → 本地 LRU 可作为短期降级，但需明确重启丢失短期上下文。
- LLM 抽取误判 → 使用结构化输出、规则护栏、置信度、pending 状态和审计降低污染。
- Milvus 与 MySQL 不一致 → MySQL 作为权威，召回必须回表过滤；删除先标记 deleting，再清理向量。
- 不做宽作用域召回可能漏掉团队偏好 → 通过后续显式继承策略解决，不在默认路径自动混入。
- 会话摘要生成失败 → 聊天可继续，只丢失摘要更新，并记录日志方便排查。
- 去重/冲突检测成本增加 → 只在写入长期记忆时执行，不影响普通未启用长期记忆的聊天路径。

## Migration Plan

1. 新增 V2 OpenSpec 和测试用例，明确不兼容旧内存系统。
2. 拆分 `SessionMemoryStore`、`LongTermMemoryStore`、`MemoryExtractor`、`VectorIndex` 等接口。
3. 实现 Redis/LRU 短期会话存储，并把聊天流程从 `SimpleMemory` 完全迁移出去。
4. 实现 MySQL 长期记忆存储和审计事件存储。
5. 实现 Milvus 长期记忆索引，使用 `agent_long_term_memories` collection。
6. 接入结构化 LLM 抽取、规则护栏、去重合并和冲突 pending。
7. 更新管理接口 query/limit/status 行为和测试。
8. 删除旧兼容入口或使其不再被构建时依赖。
9. 运行 `go test ./...`，并补充迁移说明。

回滚策略：关闭长期记忆写入和召回后，聊天保留短期 Redis/LRU 会话能力；如果 Redis 不可用，可临时使用本地 LRU 降级。MySQL 和 Milvus 的长期记忆数据不参与 prompt 时不会影响聊天结果。

## Open Questions

无。以下决策已确认：Redis 使用 agent_service 自己的 Redis 客户端和配置；MySQL 使用 agent_service 自己的 MySQL 依赖、配置和连接池；结构化记忆抽取默认使用 quick model。
