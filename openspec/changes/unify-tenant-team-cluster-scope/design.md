## Context

当前系统存在三套互相脱节的作用域：

- `api_gateway` 的 `users` 表和 JWT 只表达全局用户、角色和状态，没有租户、团队或成员关系。
- `agent_service` 的记忆表按租户、团队、集群和会话组织，但聊天请求可以直接传入 `TenantId`、`TeamId`，前端默认又不传这些字段，实际容易落入 `default/default`。
- worker 上报给 manager 的 `MonitorInfo` 只包含主机名和 IP，没有租户、团队和集群归属，manager 写入的监控事实无法做多租户隔离。

这次变更横跨数据库 schema、认证、worker 注册、manager 写入、查询过滤、agent 记忆和前端选择器。设计目标是先建立统一作用域契约，再让各模块逐步迁移到同一套可信来源。

## Goals / Non-Goals

**Goals:**

- 建立租户、团队、用户、成员关系和当前访问作用域的基础身份模型。
- 让 `api_gateway` 签发包含当前租户和团队的 JWT，并在监控查询时统一执行授权过滤。
- 建立 worker/server/cluster 到 tenant/team 的可信资产归属模型。
- 让 manager 写入监控数据时带上可信作用域，或写入可关联到可信作用域的 `server_id`。
- 让 `agent_service` 从认证上下文派生租户和团队，并用请求中的集群和会话作为受授权约束的业务参数。
- 保留单租户部署的兼容路径，通过 `default` 租户、团队、集群迁移旧数据。

**Non-Goals:**

- 不在本次变更中实现完整企业级 RBAC、细粒度权限策略或审计报表。
- 不改变 AI 对话 `session_id` 的语义，不把 session 写入 worker 采集链路。
- 不要求 worker 直接知道最终授权关系；worker 只需稳定标识自己，可信归属由 manager 或注册服务解析。
- 不要求立即重写所有历史指标表为强范式结构；可以先用兼容字段迁移，再逐步收敛。

## Decisions

### 1. 以 `api_gateway` 作为用户身份和访问作用域入口

`api_gateway` 负责登录、用户管理、JWT 签发和监控查询授权。新增基础身份表：

- `tenants`
- `teams`
- `users`
- `user_team_memberships`

JWT 中包含 `user_id`、`username`、`role`、`tenant_id`、`team_id`。当用户属于多个团队时，登录可使用默认团队，后续通过切换团队接口重新签发当前作用域 JWT。

替代方案是让前端每次请求都传租户和团队，但这会让普通客户端成为授权来源，无法防止越权写记忆或越权查监控数据。

### 2. 将用户表从性能监控 schema 中拆出

`users` 不属于性能指标表，也不属于 agent 记忆表。新增基础身份脚本，例如 `identity_access_schema.sql`，用于创建身份和成员关系表。`init_server_performance.sql` 聚焦监控事实表，`agent_memory_schema.sql` 聚焦记忆表。

为兼容现有部署，迁移时保留旧 `users` 数据并补齐默认租户、默认团队和默认成员关系；不在首次迁移中删除历史表字段。

### 3. 以 worker 注册表作为监控归属可信来源

新增或扩展资产表：

- `clusters`
- `servers`
- `worker_registrations` 或等价的 worker 资产表

worker 上报时携带稳定 `worker_id` 和注册凭证，推荐通过 gRPC metadata 或 `MonitorInfo` 新增字段传递。manager 根据注册表解析 `tenant_id`、`team_id`、`cluster_id`、`server_id` 后写入数据。

替代方案是让 worker 直接上报租户、团队和集群字段。这个方案实现简单，但 worker 被复制、配置错误或凭证泄露时会污染其他租户数据，因此只能作为 bootstrap 候选信息，不能作为最终授权事实。

### 4. 监控事实查询必须按认证作用域过滤

`api_gateway` 从 JWT 得到当前 `tenant_id` 和 `team_id`，调用 manager 查询时注入作用域。manager 的 QueryService 应支持作用域过滤，并且对于明细查询必须确认目标 `server_name/server_id` 属于当前作用域。

短期可以在所有监控事实表增加 `tenant_id`、`team_id`、`cluster_id` 字段并补索引；长期推荐用 `server_id` 关联 `servers` 表，减少明细表重复维度。考虑当前查询 SQL 大量直接按 `server_name` 过滤，第一阶段可采用兼容字段方案，后续再演进到 `server_id` 主索引。

### 5. agent 记忆作用域从认证上下文派生

`agent_service` 只接受 `cluster_id` 和 `session_id` 作为请求业务参数。`tenant_id` 和 `team_id` 默认来自 JWT。管理员或系统内部任务可显式指定作用域，但必须经过权限校验。

会话记忆唯一键应纳入 `user_id` 或 `actor_id`，避免同一团队同一集群内不同用户碰巧使用相同 `session_id` 时互相污染。长期记忆仍按租户、团队和集群共享，但需要记录 `created_by`。

### 6. 单租户兼容通过默认作用域完成

迁移脚本创建：

- `tenant_id = default`
- `team_id = default`
- `cluster_id = default`

历史用户加入默认团队，历史 worker/server 和历史监控数据归入默认集群。旧前端不传作用域时仍可工作，但新服务内部必须把缺失作用域归一到默认值并记录迁移约束。

## Risks / Trade-offs

- 作用域字段迁移范围大 → 分阶段落地，先补身份和资产表，再补查询过滤，最后收紧请求体信任边界。
- 历史监控表数据量大，补字段和索引可能耗时 → 先支持默认作用域查询，生产迁移使用在线 DDL 或分批回填。
- worker 注册凭证设计不足会影响安全边界 → 初版使用共享注册 token 或预置 worker token，后续再升级到轮换和吊销。
- `server_name` 可能在不同租户重复 → 查询 API 保留名称参数但内部必须结合作用域解析，后续引入 `server_id` 作为稳定主键。
- JWT 当前团队切换会影响前端状态 → 前端需要明确展示当前团队，并在切换后刷新监控和 AI 会话上下文。

## Migration Plan

1. 新增身份和资产 schema，创建默认租户、默认团队、默认集群。
2. 从现有 `users` 数据迁移到基础身份模型，并创建默认成员关系。
3. 为现有 worker/server 建立默认资产归属，历史监控数据先归入默认作用域。
4. 扩展 JWT、认证中间件和 `agent_service` claims 解析，保持旧 token 在开发环境可短期兼容。
5. 扩展 manager 写入和查询过滤，先按兼容字段落地，再评估 `server_id` 收敛。
6. 前端增加当前团队和集群选择能力，并在 AI 对话中只传 `cluster_id/session_id/question`。
7. 完成验证后，收紧普通请求体中的 `tenant_id/team_id` 覆盖能力。

回滚策略：保留默认作用域和旧查询路径；若新作用域过滤出现问题，可临时禁用多租户过滤并回退到默认租户视图，但不得回滚已签发 token 的签名密钥和用户密码数据。

## Open Questions

- 初版是否允许一个用户同时属于多个租户，还是只允许多团队但单租户？
- worker 注册凭证使用 gRPC metadata、`MonitorInfo` 字段，还是独立注册接口更符合现有部署方式？
- 监控事实表第一阶段是否全部增加 `tenant_id/team_id/cluster_id`，还是直接引入 `server_id` 并改写查询 SQL？
- `admin` 是全局管理员、租户管理员，还是两者都需要区分？
