## 1. 数据库与迁移

- [x] 1.1 新增基础身份 schema，创建 `tenants`、`teams`、`users`、`user_team_memberships` 表，不自动创建默认租户/团队
- [x] 1.2 从性能监控初始化脚本中移除 `users` 建表职责，并更新 `sql table/README.md` 的表归属说明
- [x] 1.3 新增监控资产 schema，创建 `clusters`、`servers`、`worker_registrations` 或等价资产归属表
- [x] 1.4 移除默认作用域兼容设计，文档中明确租户、团队、集群和成员关系必须显式创建
- [x] 1.5 更新 `agent_memory_schema.sql`，为会话记忆和长期记忆补充用户/创建者隔离字段及索引

## 2. api_gateway 身份与授权

- [x] 2.1 扩展用户模型和存储层，支持租户、团队、成员关系、成员状态和成员角色
- [x] 2.2 扩展登录流程，登录成功后选择一个已授权团队并签发包含 `tenant_id`、`team_id` 的 JWT
- [x] 2.3 新增或调整当前用户信息接口，返回当前租户、团队和可切换团队列表
- [x] 2.4 增加团队切换接口，校验成员关系后重新签发当前作用域 JWT
- [x] 2.5 为监控 HTTP 查询接口注入认证作用域，并拒绝普通用户越权指定其他租户或团队
- [x] 2.6 补充 api_gateway 身份、JWT、团队切换和越权访问测试

## 3. worker 与 manager 监控归属

- [x] 3.1 扩展 worker 配置，支持稳定 `worker_id` 和注册凭证
- [x] 3.2 扩展 worker 上报链路，通过 gRPC metadata 或 protobuf 字段传递 worker 身份
- [x] 3.3 扩展 manager 接收逻辑，根据 worker 注册表解析租户、团队、集群和服务器归属
- [x] 3.4 manager 对未知、禁用或凭证无效的 worker 拒绝写入并记录错误指标
- [x] 3.5 扩展 manager 写库逻辑，使汇总表和明细表可按租户、团队、集群、服务器过滤
- [x] 3.6 补充模拟 worker 推送和 manager 写库测试，覆盖有效 worker、未知 worker 和缺少显式作用域场景

## 4. manager QueryService 与监控查询

- [x] 4.1 扩展 QueryService 请求结构，支持租户、团队和集群过滤参数
- [x] 4.2 更新 QueryManager SQL，所有概览、评分、趋势、异常和明细查询都应用作用域过滤
- [x] 4.3 对按服务器名称查询的接口增加作用域内服务器校验，避免跨租户同名服务器泄露
- [x] 4.4 更新 api_gateway gRPC 客户端，转发当前认证作用域到 manager
- [x] 4.5 补充 manager 查询测试，覆盖当前团队数据、其他团队数据和缺少显式作用域的拒绝路径

## 5. agent_service 记忆作用域

- [x] 5.1 扩展 agent_service claims 和认证上下文，统一解析 `tenant_id`、`team_id` 和 `user_id`
- [x] 5.2 调整普通聊天和流式聊天作用域派生逻辑，从 JWT 获取租户/团队/用户，只接受请求中的集群和会话参数
- [x] 5.3 为记忆策略、列表、删除和清空接口增加作用域授权校验，普通用户不得越权治理其他团队记忆
- [x] 5.4 调整会话记忆键和持久化结构，纳入用户身份以隔离不同用户的相同 `session_id`
- [x] 5.5 长期记忆写入时记录创建者，并保持同租户、团队、集群范围内的长期记忆共享
- [x] 5.6 补充 agent_service 测试，覆盖请求体伪造租户/团队、同 session 不同用户隔离、管理员显式作用域

## 6. web 前端体验

- [x] 6.1 扩展登录态类型，保存当前租户、团队和可访问团队列表
- [x] 6.2 增加当前团队展示和团队切换入口，切换后刷新认证会话
- [x] 6.3 增加集群选择状态，并让监控页面和 AI 运维页面按当前团队/集群刷新
- [x] 6.4 调整 AI 聊天请求，只发送 `cluster_id`、`session_id` 和问题内容，不发送普通用户不可控的租户/团队
- [x] 6.5 补充前端构建和关键页面交互验证

## 7. 文档与验证

- [x] 7.1 更新 api_gateway、agent_service、worker、manager 和 web README 中的作用域说明
- [x] 7.2 更新配置示例，增加显式租户/团队/集群初始化、worker_id、worker 注册凭证和迁移说明
- [x] 7.3 执行 api_gateway Go 测试并修复失败
- [x] 7.4 执行 agent_service Go 测试并修复失败
- [x] 7.5 构建 C++ protobuf、manager 和 worker 目标并修复失败
- [x] 7.6 构建 web 前端并修复失败
- [x] 7.7 使用 OpenSpec 校验变更状态，确认提案、设计、spec 和任务均可用于实现
