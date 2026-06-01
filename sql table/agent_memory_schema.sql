-- agent_service 记忆系统自有表结构。
-- 这些表只保存 AI 助手的工作上下文和治理信息，不保存实时监控事实。

CREATE DATABASE IF NOT EXISTS `monitor-system` DEFAULT CHARACTER SET utf8mb4;
USE `monitor-system`;

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

CREATE TABLE IF NOT EXISTS agent_session_memories (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    tenant_id VARCHAR(128) NOT NULL,
    team_id VARCHAR(128) NOT NULL,
    cluster_id VARCHAR(128) NOT NULL DEFAULT '',
    user_id VARCHAR(64) NOT NULL DEFAULT 'system',
    session_id VARCHAR(128) NOT NULL,
    summary TEXT NOT NULL,
    recent_messages JSON NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    UNIQUE KEY uk_agent_session_memory (tenant_id, team_id, cluster_id, user_id, session_id),
    INDEX idx_agent_session_scope_user (tenant_id, team_id, cluster_id, user_id)
);

CREATE TABLE IF NOT EXISTS agent_long_term_memories (
    id VARCHAR(64) PRIMARY KEY,
    tenant_id VARCHAR(128) NOT NULL,
    team_id VARCHAR(128) NOT NULL,
    cluster_id VARCHAR(128) NOT NULL DEFAULT '',
    scope_level VARCHAR(32) NOT NULL DEFAULT 'cluster',
    memory_type VARCHAR(64) NOT NULL,
    content TEXT NOT NULL,
    content_hash CHAR(64) NOT NULL DEFAULT '',
    source VARCHAR(64) NOT NULL,
    confidence DECIMAL(4,3) NOT NULL DEFAULT 0.800,
    reason TEXT NULL,
    sensitivity VARCHAR(32) NOT NULL DEFAULT 'low',
    conflict_of VARCHAR(64) NULL,
    status VARCHAR(32) NOT NULL,
    created_by VARCHAR(64) NOT NULL,
    created_by_user_id VARCHAR(64) NULL,
    vector_id VARCHAR(128) NOT NULL,
    expires_at TIMESTAMP NULL,
    last_used_at TIMESTAMP NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_agent_memory_scope_status (tenant_id, team_id, cluster_id, status),
    INDEX idx_agent_memory_type_status (memory_type, status),
    INDEX idx_agent_memory_created_by_user (created_by_user_id),
    INDEX idx_agent_memory_content_hash (content_hash),
    INDEX idx_agent_memory_conflict_of (conflict_of)
);

CREATE TABLE IF NOT EXISTS agent_memory_events (
    id VARCHAR(64) PRIMARY KEY,
    memory_id VARCHAR(64) NULL,
    tenant_id VARCHAR(128) NOT NULL,
    team_id VARCHAR(128) NOT NULL,
    cluster_id VARCHAR(128) NOT NULL DEFAULT '',
    actor_user_id VARCHAR(64) NULL,
    action VARCHAR(64) NOT NULL,
    detail TEXT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_agent_memory_events_memory_id (memory_id),
    INDEX idx_agent_memory_events_scope (tenant_id, team_id, cluster_id),
    INDEX idx_agent_memory_events_actor (actor_user_id)
);
