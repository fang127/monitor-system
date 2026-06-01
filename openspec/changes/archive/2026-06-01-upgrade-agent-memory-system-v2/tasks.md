## 1. 架构拆分

- [x] 1.1 拆分短期会话存储接口与长期记忆权威存储接口，避免 MySQL 成为短期最近消息主路径。
- [x] 1.2 移除聊天主流程和工具入口对 `SimpleMemory` 的依赖，旧内存系统不再作为兼容路径。
- [x] 1.3 增加记忆配置项：短期 TTL、召回相似度阈值、抽取置信度阈值、pending 阈值和 token 预算。

## 2. 短期会话热存储

- [x] 2.1 实现进程内 LRU/TTL 短期会话存储，用于 Redis 不可用或未配置时的热路径降级。
- [x] 2.2 预留 Redis 短期会话存储实现边界和配置，不把每轮 recent messages 写入 MySQL。
- [x] 2.3 将会话摘要升级为真实摘要/分层摘要，失败时记录日志但不阻断当前回答。

## 3. 长期记忆权威存储和向量索引

- [x] 3.1 实现 MySQL 长期记忆存储接口，覆盖策略、长期记忆、状态更新和审计事件。
- [x] 3.2 实现 Milvus 长期记忆向量索引，使用独立 collection `agent_long_term_memories`。
- [x] 3.3 修改长期记忆召回为向量候选 ID + MySQL 回表过滤，无相似命中时返回空。
- [x] 3.4 保持精确作用域召回，cluster 会话不得自动补充 team/tenant 级记忆。

## 4. 结构化抽取和治理

- [x] 4.1 实现结构化 LLM 长期记忆抽取器，输出 type、content、scope_level、confidence、reason、expires_at、sensitivity、should_store。
- [x] 4.2 增加规则护栏，过滤凭据、token、一次性状态、未确认猜测和实时指标值。
- [x] 4.3 实现去重、合并和冲突检测，高风险或冲突候选进入 pending。
- [x] 4.4 为抽取、创建、更新、冲突、召回和删除写入审计事件。

## 5. 管理接口和可观察性

- [x] 5.1 补全长期记忆管理接口 query、limit、status 的行为。
- [x] 5.2 给 AppendTurn、SummarizeSession、ExtractDurableMemories、向量写入/删除和审计失败路径补日志。
- [x] 5.3 更新 SQL schema，补充 hash、reason、sensitivity、scope_level、conflict_of 等治理字段。

## 6. 验证

- [x] 6.1 补充短期会话隔离、窗口裁剪、摘要失败降级测试。
- [x] 6.2 补充长期记忆 query、limit、status、租户越权、删除中状态、过期记忆不召回测试。
- [x] 6.3 运行 agent_service Go 测试并修复失败。
