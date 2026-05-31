## ADDED Requirements

### Requirement: 监控资产归属
系统 MUST 为集群、服务器和 worker 建立可信资产归属，并将每个可写入监控事实的 worker 绑定到租户、团队和集群。

#### Scenario: 注册 worker 资产
- **WHEN** 管理员或部署流程注册新的 worker
- **THEN** 系统 MUST 保存该 worker 的稳定标识、所属租户、团队、集群、服务器信息和启用状态

#### Scenario: 解析上报归属
- **WHEN** manager 接收到 worker 推送的监控数据
- **THEN** manager MUST 根据可信注册信息解析该数据所属的租户、团队、集群和服务器

#### Scenario: 拒绝未知 worker
- **WHEN** manager 接收到未注册、已禁用或凭证无效的 worker 推送
- **THEN** manager MUST 拒绝写入该监控数据

### Requirement: worker 上报身份
worker MUST 在推送监控数据时携带稳定 worker 身份或注册凭证，但不得把 AI 对话会话作为监控数据归属字段。

#### Scenario: 推送稳定身份
- **WHEN** worker 调用 manager 的监控上报接口
- **THEN** worker MUST 携带可供 manager 识别注册记录的 `worker_id` 或等价凭证

#### Scenario: 不携带 AI 会话
- **WHEN** worker 采集并推送 CPU、内存、磁盘、网络、MySQL 或 Redis 指标
- **THEN** worker MUST NOT 携带 AI 对话 `session_id` 作为监控事实归属字段

#### Scenario: 自报作用域仅作候选
- **WHEN** worker 配置中包含租户、团队或集群名称
- **THEN** manager MUST NOT 直接信任该自报值作为最终授权归属，除非它与可信注册信息匹配

### Requirement: 作用域化监控写入
manager MUST 在写入监控事实时保存或关联可信租户、团队、集群和服务器作用域，保证不同作用域的数据可被隔离查询。

#### Scenario: 写入汇总指标
- **WHEN** manager 写入服务器性能汇总数据
- **THEN** 系统 MUST 保存该记录所属的租户、团队、集群和服务器标识，或保存可关联到这些作用域的 `server_id`

#### Scenario: 写入明细指标
- **WHEN** manager 写入网络、磁盘、内存、软中断、MySQL 或 Redis 明细数据
- **THEN** 系统 MUST 让该记录能够被租户、团队、集群和服务器作用域过滤

#### Scenario: 兼容历史数据
- **WHEN** 历史监控数据缺少显式作用域
- **THEN** 系统 MUST 将其归入默认租户、默认团队和默认集群，保证旧数据仍可查询

### Requirement: 作用域化监控查询
系统 MUST 按认证上下文限制监控查询结果，普通用户只能看到当前租户和团队内的监控资产与指标。

#### Scenario: 查询集群概览
- **WHEN** 普通用户查询服务器最新评分或集群概览
- **THEN** 系统 MUST 只返回当前访问令牌租户和团队内的服务器

#### Scenario: 查询服务器明细
- **WHEN** 普通用户查询指定服务器的性能、趋势、异常或指标明细
- **THEN** 系统 MUST 验证该服务器属于当前访问令牌租户和团队后再返回数据

#### Scenario: 越权服务器查询
- **WHEN** 普通用户查询其他租户或团队的服务器
- **THEN** 系统 MUST 拒绝请求或返回空结果，且 MUST NOT 泄露该服务器的监控事实
