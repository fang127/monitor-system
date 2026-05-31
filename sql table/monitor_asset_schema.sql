-- 监控资产归属表结构。
-- manager 通过这些表把 worker 上报数据解析到可信的租户、团队、集群和服务器作用域。

CREATE DATABASE IF NOT EXISTS `monitor-system` DEFAULT CHARACTER SET utf8mb4;
USE `monitor-system`;

CREATE TABLE IF NOT EXISTS clusters (
    id VARCHAR(128) PRIMARY KEY,
    tenant_id VARCHAR(128) NOT NULL,
    team_id VARCHAR(128) NOT NULL,
    name VARCHAR(255) NOT NULL,
    status ENUM('active','disabled') NOT NULL DEFAULT 'active',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    UNIQUE KEY uk_clusters_scope_name (tenant_id, team_id, name),
    INDEX idx_clusters_scope_status (tenant_id, team_id, status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS servers (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    tenant_id VARCHAR(128) NOT NULL,
    team_id VARCHAR(128) NOT NULL,
    cluster_id VARCHAR(128) NOT NULL,
    server_name VARCHAR(255) NOT NULL,
    host_name VARCHAR(255) NOT NULL DEFAULT '',
    ip_address VARCHAR(64) NOT NULL DEFAULT '',
    status ENUM('active','disabled') NOT NULL DEFAULT 'active',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    UNIQUE KEY uk_servers_scope_name (tenant_id, team_id, cluster_id, server_name),
    INDEX idx_servers_scope_status (tenant_id, team_id, cluster_id, status),
    INDEX idx_servers_name (server_name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS worker_registrations (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    worker_id VARCHAR(128) NOT NULL UNIQUE,
    token_hash VARCHAR(255) NOT NULL DEFAULT '',
    tenant_id VARCHAR(128) NOT NULL,
    team_id VARCHAR(128) NOT NULL,
    cluster_id VARCHAR(128) NOT NULL,
    server_id BIGINT NULL,
    status ENUM('active','disabled') NOT NULL DEFAULT 'active',
    last_seen_at DATETIME NULL,
    metadata JSON NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_worker_scope_status (tenant_id, team_id, cluster_id, status),
    INDEX idx_worker_server (server_id),
    CONSTRAINT fk_worker_server FOREIGN KEY (server_id) REFERENCES servers(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
