-- 基础身份与访问作用域表结构。
-- 这些表保存登录用户、租户、团队和成员关系，是所有业务作用域的可信来源。

CREATE DATABASE IF NOT EXISTS `monitor-system` DEFAULT CHARACTER SET utf8mb4;
USE `monitor-system`;

CREATE TABLE IF NOT EXISTS tenants (
    id VARCHAR(128) PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    status ENUM('active','disabled') NOT NULL DEFAULT 'active',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS teams (
    id VARCHAR(128) PRIMARY KEY,
    tenant_id VARCHAR(128) NOT NULL,
    name VARCHAR(255) NOT NULL,
    status ENUM('active','disabled') NOT NULL DEFAULT 'active',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    UNIQUE KEY uk_teams_tenant_name (tenant_id, name),
    INDEX idx_teams_tenant_status (tenant_id, status),
    CONSTRAINT fk_teams_tenant FOREIGN KEY (tenant_id) REFERENCES tenants(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS users (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(64) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    role ENUM('admin','user') NOT NULL DEFAULT 'user',
    status ENUM('active','disabled') NOT NULL DEFAULT 'active',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS user_team_memberships (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    user_id BIGINT NOT NULL,
    tenant_id VARCHAR(128) NOT NULL,
    team_id VARCHAR(128) NOT NULL,
    member_role ENUM('owner','admin','member','viewer') NOT NULL DEFAULT 'member',
    status ENUM('active','disabled') NOT NULL DEFAULT 'active',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    UNIQUE KEY uk_user_team_membership (user_id, tenant_id, team_id),
    INDEX idx_membership_scope_status (tenant_id, team_id, status),
    INDEX idx_membership_user_status (user_id, status),
    CONSTRAINT fk_membership_user FOREIGN KEY (user_id) REFERENCES users(id),
    CONSTRAINT fk_membership_tenant FOREIGN KEY (tenant_id) REFERENCES tenants(id),
    CONSTRAINT fk_membership_team FOREIGN KEY (team_id) REFERENCES teams(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
