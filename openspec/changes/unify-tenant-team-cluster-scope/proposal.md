## Why

当前系统已经在 `agent_service` 记忆系统中引入租户、团队、集群和会话作用域，但登录用户、JWT、worker 上报数据和监控查询链路没有共享同一套作用域模型。这样会导致 AI 记忆默认落入 `default/default`、用户可通过请求体伪造作用域、监控事实无法按租户和团队隔离，也会让后续多租户部署缺少清晰边界。

## What Changes

- 引入统一身份与访问作用域模型，明确用户、租户、团队、成员关系、角色和 JWT 中的当前访问作用域。
- 将登录用户表从性能监控初始化脚本中解耦，归入基础身份/访问控制 schema，并为后续租户和团队管理预留迁移路径。
- 引入监控资产归属模型，明确 worker、server、cluster、tenant、team 的绑定关系。
- 调整 worker 到 manager 的上报归属：worker 负责携带稳定 worker 身份或注册凭证，manager 根据可信注册表解析租户、团队和集群。
- 调整监控查询链路：`api_gateway` 根据登录用户作用域向 manager 查询并过滤授权资产。
- 修改 `agent-memory` 能力：聊天和记忆治理必须从认证上下文派生租户/团队，不能信任普通请求体中的租户/团队字段；会话记忆需要避免同团队不同用户的会话冲突。
- 保持 `session_id` 作为 AI 对话概念，不将它混入 worker 监控采集数据。

## Capabilities

### New Capabilities

- `identity-access-scope`: 定义租户、团队、用户、成员关系、角色、JWT 当前作用域和访问授权规则。
- `monitor-asset-scope`: 定义 worker、server、cluster 与租户/团队的归属关系，以及监控写入和查询的作用域隔离。

### Modified Capabilities

- `agent-memory`: 调整记忆作用域来源、权限校验和会话隔离要求，使其与统一身份作用域对齐。

## Impact

- 数据库脚本：需要新增或调整基础身份表、资产归属表，并从 `init_server_performance.sql` 中拆分 `users` 的职责边界。
- `api_gateway`：用户模型、登录签发 JWT、用户管理接口、查询接口授权过滤会受影响。
- `agent_service`：认证上下文、聊天请求作用域派生、记忆策略管理、长期记忆查询/删除权限会受影响。
- `worker` / `manager` / protobuf：worker 注册身份、`MonitorInfo` 或 gRPC metadata、manager 写库维度、QueryService 查询参数和 SQL 过滤会受影响。
- `web`：用户管理、AI 运维页面、团队/集群选择器和请求参数会受影响。
- 兼容性：现有单租户部署需要提供默认租户、默认团队和默认集群迁移策略，保证历史数据仍可查询。
