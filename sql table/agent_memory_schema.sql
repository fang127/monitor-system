-- agent_service 记忆系统自有表结构。
-- 这些表只保存 AI 助手的工作上下文和治理信息，不保存实时监控事实。

CREATE DATABASE IF NOT EXISTS `monitor-system` DEFAULT CHARACTER SET utf8mb4;
USE `monitor-system`;

-- agent_memory_scopes 定义了记忆的作用范围和相关配置。
CREATE TABLE IF NOT EXISTS agent_memory_scopes (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    scope_level VARCHAR(32) NOT NULL,
    tenant_id VARCHAR(128) NOT NULL,
    team_id VARCHAR(128) NOT NULL,
    cluster_id VARCHAR(128) NOT NULL DEFAULT '',
    long_term_enabled BOOLEAN NULL,
    write_enabled BOOLEAN NULL,
    recall_enabled BOOLEAN NULL,
    recent_window INT NULL,
    summary_window INT NULL,
    recall_limit INT NULL,
    token_budget INT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    UNIQUE KEY uk_agent_memory_scope (scope_level, tenant_id, team_id, cluster_id)
);

-- agent_session_memories 保存了每个会话的短期记忆，包括摘要和近期消息。
CREATE TABLE IF NOT EXISTS agent_session_memories (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    tenant_id VARCHAR(128) NOT NULL,
    team_id VARCHAR(128) NOT NULL,
    cluster_id VARCHAR(128) NOT NULL DEFAULT '',
    user_id VARCHAR(64) NOT NULL DEFAULT 'system',
    session_id VARCHAR(128) NOT NULL,
    summary TEXT NOT NULL, -- 会话摘要，供快速回顾使用
    recent_messages JSON NULL, -- 最近消息列表，包含角色、内容、时间等信息
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    UNIQUE KEY uk_agent_session_memory (tenant_id, team_id, cluster_id, user_id, session_id),
    INDEX idx_agent_session_scope_user (tenant_id, team_id, cluster_id, user_id)
);

-- agent_long_term_memories 保存了长期记忆的详细信息，包括内容、来源、置信度等。
CREATE TABLE IF NOT EXISTS agent_long_term_memories (
    id VARCHAR(64) PRIMARY KEY,
    tenant_id VARCHAR(128) NOT NULL,
    team_id VARCHAR(128) NOT NULL,
    cluster_id VARCHAR(128) NOT NULL DEFAULT '',
    scope_level VARCHAR(32) NOT NULL DEFAULT 'cluster',
    memory_type VARCHAR(64) NOT NULL, -- 记忆类型，如事实、规则、经验等
    content TEXT NOT NULL, -- 记忆内容，可能是文本、JSON等格式
    content_hash CHAR(64) NOT NULL DEFAULT '', -- 内容哈希值，用于快速去重和检索
    source VARCHAR(64) NOT NULL, -- 记忆来源，如用户输入、系统观察、外部数据等
    confidence DECIMAL(4,3) NOT NULL DEFAULT 0.800, -- 置信度，0.000-1.000
    reason TEXT NULL, -- 记忆形成的原因或依据，供审计和回顾使用
    sensitivity VARCHAR(32) NOT NULL DEFAULT 'low', -- 敏感度级别，如 low、medium、high
    conflict_of VARCHAR(64) NULL, -- 如果是冲突记忆，指向被冲突的记忆 ID
    status VARCHAR(32) NOT NULL, -- 记忆状态，如 active、inactive、conflicted、expired 等
    created_by VARCHAR(64) NOT NULL, -- 记忆创建者的标识，如用户 ID、系统 ID 等
    created_by_user_id VARCHAR(64) NULL, -- 记忆创建者的用户 ID，便于查询和权限控制
    vector_id VARCHAR(128) NOT NULL, -- 向量数据库中的向量 ID，便于快速检索相关记忆
    expires_at TIMESTAMP NULL, -- 记忆过期时间，过期后可以自动失效或删除
    last_used_at TIMESTAMP NULL, -- 记忆最后一次被使用的时间，便于清理和优化
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_agent_memory_scope_status (tenant_id, team_id, cluster_id, status),
    INDEX idx_agent_memory_type_status (memory_type, status),
    INDEX idx_agent_memory_created_by_user (created_by_user_id),
    INDEX idx_agent_memory_content_hash (content_hash),
    INDEX idx_agent_memory_conflict_of (conflict_of)
);

-- agent_memory_events 记录了与记忆相关的事件日志，便于审计和分析。
CREATE TABLE IF NOT EXISTS agent_memory_events (
    id VARCHAR(64) PRIMARY KEY,
    memory_id VARCHAR(64) NULL, -- 关联的记忆 ID，如果事件与某条记忆相关
    tenant_id VARCHAR(128) NOT NULL,
    team_id VARCHAR(128) NOT NULL,
    cluster_id VARCHAR(128) NOT NULL DEFAULT '',
    actor_user_id VARCHAR(64) NULL, -- 事件触发者的用户 ID，可能是用户操作或系统行为
    action VARCHAR(64) NOT NULL, -- 事件类型，如 create、update、delete、recall、expire 等
    detail TEXT NULL, -- 事件详情，包含事件发生的上下文信息，如变更内容、原因等
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_agent_memory_events_memory_id (memory_id),
    INDEX idx_agent_memory_events_scope (tenant_id, team_id, cluster_id),
    INDEX idx_agent_memory_events_actor (actor_user_id)
);
