## ADDED Requirements

### Requirement: 旧内存记忆路径移除
系统 MUST 移除聊天主流程对旧 `SimpleMemory` 全局 map、内存长期记忆和文本匹配向量模拟的依赖，并以新的记忆管理入口处理短期会话、长期记忆和召回。

#### Scenario: 聊天流程不使用 SimpleMemory
- **WHEN** 用户发起普通聊天或流式聊天请求
- **THEN** 系统 MUST NOT 通过 `GetSimpleMemory` 或全局 `SimpleMemoryMap` 读取或写入会话历史

#### Scenario: 内存长期记忆不作为生产主路径
- **WHEN** 长期记忆写入或召回启用
- **THEN** 系统 MUST 使用长期记忆权威存储和长期记忆向量索引，而不是进程内 map 作为主路径

### Requirement: 长期记忆去重合并和冲突治理
系统 MUST 在写入长期记忆前执行去重、合并和冲突检测，避免同一事实重复保存或互相矛盾的事实直接污染可召回记忆。

#### Scenario: 相似候选更新旧记忆
- **WHEN** 新候选长期记忆与同作用域、同类型的现有记忆高度相似且不冲突
- **THEN** 系统 MUST 更新现有记忆或合并来源信息，而不是创建重复记忆

#### Scenario: 冲突候选进入待确认
- **WHEN** 新候选长期记忆与现有 active 记忆语义冲突
- **THEN** 系统 MUST 将新候选标记为 pending 或拒绝写入，并记录冲突原因和审计事件

### Requirement: 记忆可观察性
系统 MUST 对记忆追加、摘要、抽取、召回、更新、删除和索引操作中的错误记录日志，并为长期记忆治理动作记录审计事件。

#### Scenario: 追加或抽取失败有日志
- **WHEN** `AppendTurn`、`SummarizeSession` 或 `ExtractDurableMemories` 返回错误
- **THEN** 聊天处理器 MUST 记录包含作用域和错误原因的日志

#### Scenario: 召回写入审计事件
- **WHEN** 系统创建、更新、召回、删除或重试删除长期记忆
- **THEN** 系统 MUST 记录包含记忆 ID、作用域、动作和操作者的审计事件

## MODIFIED Requirements

### Requirement: 租户团队集群作用域
系统 MUST 按租户、团队、集群、用户和会话维度组织记忆，并保证不同租户和团队的记忆互相隔离。租户和团队 MUST 来自认证上下文或受信任系统任务，集群和会话可以来自请求参数但 MUST 接受授权约束。长期记忆召回 MUST 使用精确作用域匹配，不得默认把团队级或租户级记忆混入集群级会话。

#### Scenario: 同团队同集群共享长期记忆
- **WHEN** 两个会话属于相同租户、团队和集群
- **THEN** 系统 MUST 允许后发起的会话召回该精确集群作用域下已启用且相关的长期记忆

#### Scenario: 不同团队隔离长期记忆
- **WHEN** 两个会话属于不同团队
- **THEN** 系统 MUST NOT 在任一会话中召回另一个团队的长期记忆

#### Scenario: 集群会话只召回精确集群记忆
- **WHEN** 用户请求包含集群标识
- **THEN** 系统 MUST 只召回相同 `tenant_id`、`team_id` 和 `cluster_id` 的长期记忆，不得自动补充团队级或租户级长期记忆

#### Scenario: 团队会话只召回团队级记忆
- **WHEN** 用户请求未包含集群标识
- **THEN** 系统 MUST 只召回相同 `tenant_id`、`team_id` 且空 `cluster_id` 的长期记忆

#### Scenario: 请求集群不属于当前团队
- **WHEN** 普通用户请求使用不属于当前租户和团队的集群标识
- **THEN** 系统 MUST 拒绝访问该集群作用域下的会话记忆和长期记忆

### Requirement: 短期会话记忆
系统 MUST 保存当前会话最近若干轮消息，并在超出配置窗口后移除旧消息或依赖会话摘要保留关键信息。短期会话最近消息 MUST 使用热状态存储作为主路径，优先支持 Redis 和进程内 LRU，MUST NOT 要求每轮聊天都读写 MySQL。

#### Scenario: 读取最近消息
- **WHEN** 同一个会话继续发起下一轮聊天请求
- **THEN** 系统 MUST 将该会话最近消息作为短期上下文提供给聊天流程

#### Scenario: 消息窗口超限
- **WHEN** 会话消息数量超过配置的最近消息窗口
- **THEN** 系统 MUST 控制注入模型的最近消息数量不超过配置上限

#### Scenario: 短期会话不依赖 MySQL 热路径
- **WHEN** 一轮聊天完成并追加最近消息
- **THEN** 系统 MUST 将最近消息写入短期热状态存储，并且 MUST NOT 因 MySQL 不可用而阻断最近消息热存储的主路径

### Requirement: 会话摘要
系统 MUST 为长会话维护滚动会话摘要，用于保存当前问题、已确认事实、已执行查询、助手结论和待处理事项。摘要 MUST 由摘要逻辑生成或更新，不得只是简单拼接最近窗口消息。

#### Scenario: 更新会话摘要
- **WHEN** 一轮聊天完成且会话内容达到摘要更新条件
- **THEN** 系统 MUST 更新该会话的摘要，并在后续聊天中提供给模型上下文

#### Scenario: 摘要不得替代实时事实
- **WHEN** 会话摘要包含历史监控观察
- **THEN** 系统 MUST 在 prompt 边界中区分摘要信息与通过 `api_gateway` 查询到的实时监控事实

#### Scenario: 摘要失败不阻断回答
- **WHEN** 模型回答已经生成但会话摘要更新失败
- **THEN** 系统 MUST 记录错误日志，并允许当前回答返回给用户

### Requirement: 长期记忆抽取与治理
系统 MUST 只把稳定、可复用、非敏感且符合策略的内容保存为长期记忆，并记录来源、类型、作用域、置信度、创建方式、确认状态、生命周期、敏感度和可解释原因字段。长期记忆抽取 MUST 使用结构化 LLM 输出和规则护栏，而不是仅依赖关键词判断。

#### Scenario: 生成候选长期记忆
- **WHEN** 一轮聊天完成且长期记忆写入已启用
- **THEN** 系统 MUST 基于对话内容生成包含 `type`、`content`、`scope_level`、`confidence`、`reason`、`expires_at`、`sensitivity` 和 `should_store` 的候选长期记忆

#### Scenario: 拒绝敏感内容
- **WHEN** 候选长期记忆包含凭据、个人敏感信息、一次性临时状态、未确认猜测或实时指标值
- **THEN** 系统 MUST NOT 将该候选内容保存为可召回的长期记忆

#### Scenario: 标记待确认记忆
- **WHEN** 候选长期记忆置信度不足、敏感度较高、存在冲突或需要人工确认
- **THEN** 系统 MUST 将其保存为不可召回的 pending 状态，或直接丢弃该候选内容

### Requirement: 长期记忆语义召回
系统 MUST 根据当前问题、租户、团队和集群精确作用域召回相关长期记忆，并对召回数量、相似度阈值和上下文预算进行限制。长期记忆召回 MUST 使用长期记忆专用向量索引获取候选 ID，再从权威存储回表过滤正文和状态。

#### Scenario: 召回相关记忆
- **WHEN** 用户问题与已启用的长期记忆语义相关
- **THEN** 系统 MUST 在配置预算内召回相关长期记忆，并在上下文中标明记忆类型、作用域和置信度

#### Scenario: 过滤过期记忆
- **WHEN** 长期记忆已过期、已删除、删除中或未确认
- **THEN** 系统 MUST NOT 将该记忆作为长期记忆召回结果注入聊天上下文

#### Scenario: 无相似命中返回空
- **WHEN** 长期记忆向量索引没有返回满足相似度阈值的候选记忆
- **THEN** 系统 MUST 返回空长期记忆结果，并且 MUST NOT fallback 到最近创建或最近更新的长期记忆

#### Scenario: 使用独立长期记忆 collection
- **WHEN** 系统写入或搜索长期记忆向量
- **THEN** 系统 MUST 使用 `agent_long_term_memories` collection，并且 MUST NOT 复用内部文档知识库 `ops_docs`

### Requirement: 记忆查看与删除
系统 MUST 提供长期记忆查看、删除和按作用域清空能力，并确保被删除、删除中、过期或未确认的记忆不再参与聊天召回。查看接口 MUST 支持 query、limit 和 status 筛选。

#### Scenario: 查询长期记忆列表
- **WHEN** 管理方按作用域、查询文本、状态或限制数量查询长期记忆
- **THEN** 系统 MUST 返回符合授权和筛选条件的长期记忆，并遵守 limit 上限

#### Scenario: 删除指定记忆
- **WHEN** 管理方按记忆 ID 删除长期记忆
- **THEN** 系统 MUST 删除或标记删除该记忆的持久化记录，清理对应向量索引，并阻止该记忆继续参与召回

#### Scenario: 清空作用域记忆
- **WHEN** 管理方请求清空某个租户、团队或集群作用域下的长期记忆
- **THEN** 系统 MUST 对该作用域内匹配的长期记忆执行删除流程，并保留可审计的删除事件

#### Scenario: 普通用户越权查询被拒绝
- **WHEN** 普通用户查询其他租户或其他团队的长期记忆
- **THEN** 系统 MUST 拒绝请求，且 MUST NOT 返回目标作用域中的任何记忆内容

### Requirement: 持久化数据库边界
系统 MUST 允许 `agent_service` 使用自有持久化数据库保存长期记忆、策略、审计事件和可选摘要 checkpoint，但监控事实查询 MUST 继续通过 `api_gateway` 完成。短期最近消息 MUST 使用热状态存储作为主路径，不得强制依赖持久化数据库。

#### Scenario: 保存长期记忆数据
- **WHEN** 系统创建、更新、删除或召回长期记忆
- **THEN** 系统 MUST 使用 `agent_service` 自有记忆权威存储读写长期记忆数据和治理字段

#### Scenario: 保存短期会话热状态
- **WHEN** 系统保存当前会话最近消息或摘要热状态
- **THEN** 系统 MUST 使用短期会话热状态存储，并可按配置异步或低频写入持久化摘要 checkpoint

#### Scenario: 查询监控事实
- **WHEN** 助手需要获取集群概览、服务器异常、性能趋势或指标明细
- **THEN** 系统 MUST 继续通过 `api_gateway` 工具查询监控事实，而不是从记忆数据库读取实时监控事实
