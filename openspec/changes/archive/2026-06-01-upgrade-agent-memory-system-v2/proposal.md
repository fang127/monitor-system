## Why

当前 `agent_service` 记忆系统虽然已经抽象出 `MemoryManager`、策略和长期记忆模型，但运行时仍主要依赖进程内内存、关键词抽取和文本匹配召回，容易漏记稳定事实、重复抽取，并且缺少生产级存储、向量召回、去重合并、冲突治理和可解释审计。

本变更将记忆系统升级为“LLM 结构化抽取 + 规则护栏 + Redis 短期热存储 + MySQL 长期权威存储 + Milvus 语义索引”的生产形态，同时去除旧的简单内存记忆路径，不再兼容旧系统。

## What Changes

- **BREAKING**：移除聊天主流程对 `SimpleMemory`、内存长期记忆和文本匹配向量模拟的依赖，不再提供旧系统兼容路径。
- 新增短期会话热存储策略：最近消息和滚动摘要优先使用 Redis/本地 LRU，避免每轮聊天读写 MySQL；MySQL 仅作为可选摘要 checkpoint 或治理型持久化，不作为短期会话主路径。
- 新增 MySQL 长期记忆存储实现，作为长期记忆正文、策略、状态、审计事件和可选摘要 checkpoint 的权威来源。
- 新增长期记忆专用 Milvus 向量索引，collection 使用 `agent_long_term_memories`，不得复用内部文档知识库 `ops_docs`。
- 修改长期记忆召回为“Milvus 搜索候选 ID + MySQL 回表过滤”，无相似命中时返回空，不再 fallback 到最近记忆。
- 保持长期记忆精确作用域召回：cluster 会话只召回同一 `tenant_id + team_id + cluster_id` 的长期记忆，不自动补充 team/tenant 级记忆。
- 将长期记忆抽取升级为结构化 LLM JSON 输出，包含 `type`、`content`、`scope_level`、`confidence`、`reason`、`expires_at`、`sensitivity`、`should_store` 等字段。
- 增加规则护栏，禁止保存密码、token、一次性排障中间状态、未确认猜测、实时指标值和明显敏感内容。
- 增加长期记忆去重、合并和冲突检测：相似候选更新旧记忆或进入冲突/待确认流程，而不是无条件新增。
- 增加高风险候选 `pending` 治理路径，并为抽取、召回、更新、删除记录审计事件。
- 将会话摘要从窗口拼接升级为真实摘要或分层摘要，并按 token 预算裁剪记忆 prompt。
- 补充长期记忆管理接口行为测试，覆盖 query、limit、status、租户越权、删除中状态和过期记忆不召回。

## Capabilities

### New Capabilities

### Modified Capabilities

- `agent-memory`: 升级记忆系统的存储边界、短期会话策略、长期记忆抽取、去重合并、语义召回、精确作用域、审计和管理接口行为。

## Impact

- 影响 `agent_service/internal/session/memory`：需要拆分短期会话存储、长期 MySQL 存储、Milvus 向量索引、LLM 抽取器和治理逻辑。
- 影响 `agent_service/internal/handler/agent`：聊天入口需要记录 `AppendTurn()`、`ExtractDurableMemories()` 等错误日志，管理接口需要支持 query、limit、status 行为。
- 影响 `agent_service/internal/storage/memory` 与配置：需要新增 MySQL/Redis/Milvus 相关运行时配置和存储实现。
- 影响 SQL schema：可能需要补充摘要 checkpoint、记忆 hash、冲突状态、敏感度、原因、作用域层级等字段或迁移脚本。
- 影响测试：需要新增存储、召回、抽取治理、越权、删除和过期过滤测试。
- 影响部署：长期记忆开启前需要准备 MySQL 表、Redis、Milvus collection 和模型配置；默认仍应保持安全关闭。
