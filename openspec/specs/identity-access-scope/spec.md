# identity-access-scope Specification

## Purpose
TBD - created by archiving change unify-tenant-team-cluster-scope. Update Purpose after archive.
## Requirements
### Requirement: 基础身份作用域
系统 MUST 提供统一的租户、团队、用户和成员关系模型，用于描述用户可访问的业务作用域。

#### Scenario: 显式创建身份作用域
- **WHEN** 系统初始化基础身份表
- **THEN** 系统 MUST 只创建租户、团队、用户和成员关系表结构，不得自动创建默认租户或默认团队

#### Scenario: 用户归属团队
- **WHEN** 管理员创建或更新用户团队关系
- **THEN** 系统 MUST 记录用户所属租户、团队、成员角色和成员状态

#### Scenario: 禁用成员访问
- **WHEN** 用户账号或团队成员关系被禁用
- **THEN** 系统 MUST 阻止该用户在对应作用域内登录或访问受保护资源

### Requirement: 当前访问作用域令牌
系统 MUST 在认证令牌中包含当前用户、角色、租户和团队信息，作为后端服务判断访问作用域的可信来源。

#### Scenario: 登录签发作用域令牌
- **WHEN** 用户登录成功且存在可用团队成员关系
- **THEN** 系统 MUST 签发包含 `user_id`、`username`、`role`、`tenant_id` 和 `team_id` 的访问令牌

#### Scenario: 缺少可用团队
- **WHEN** 用户登录成功但没有任何启用的团队成员关系
- **THEN** 系统 MUST 拒绝签发可访问业务数据的访问令牌

#### Scenario: 切换当前团队
- **WHEN** 用户请求切换到另一个已授权团队
- **THEN** 系统 MUST 重新签发当前租户和团队作用域对应的访问令牌

### Requirement: 作用域访问授权
系统 MUST 使用认证上下文中的租户和团队限制普通用户访问范围，并允许管理员在明确授权范围内管理作用域资源。

#### Scenario: 普通用户访问当前团队
- **WHEN** 普通用户请求访问当前令牌租户和团队内的资源
- **THEN** 系统 MUST 允许请求继续进入对应业务处理流程

#### Scenario: 普通用户越权访问其他团队
- **WHEN** 普通用户请求访问不属于当前令牌租户或团队的资源
- **THEN** 系统 MUST 拒绝该请求

#### Scenario: 管理员管理团队资源
- **WHEN** 管理员请求管理其授权租户或团队内的用户、资产或记忆策略
- **THEN** 系统 MUST 按管理员角色和成员关系执行授权校验

### Requirement: 身份数据库边界
系统 MUST 将用户和身份作用域表维护在基础身份 schema 中，不得把用户表归入性能监控事实表或 AI 记忆表。

#### Scenario: 初始化身份表
- **WHEN** 部署初始化数据库
- **THEN** 系统 MUST 从基础身份 schema 创建用户、租户、团队和成员关系表

#### Scenario: 初始化监控表
- **WHEN** 部署初始化性能监控数据库
- **THEN** 性能监控 schema MUST NOT 负责创建或维护登录用户表

#### Scenario: 初始化记忆表
- **WHEN** 部署初始化 AI 记忆数据库
- **THEN** AI 记忆 schema MUST NOT 负责创建或维护登录用户表

