-- Monitor System Database Schema
-- 用于存储服务器性能监控数据

CREATE DATABASE IF NOT EXISTS `monitor-system` DEFAULT CHARACTER SET utf8mb4;
USE `monitor-system`;

-- 1. 后端登录用户表
CREATE TABLE IF NOT EXISTS users (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(64) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    role ENUM('admin','user') NOT NULL DEFAULT 'user',
    status ENUM('active','disabled') NOT NULL DEFAULT 'active',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP 
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 2. 服务器性能汇总表（主表）
CREATE TABLE IF NOT EXISTS server_performance (
    id INT AUTO_INCREMENT PRIMARY KEY,
    server_name VARCHAR(255) NOT NULL,
    -- CPU 指标
    cpu_percent FLOAT DEFAULT 0,              -- CPU总使用率，单位：%
    usr_percent FLOAT DEFAULT 0,              -- 用户态CPU使用率，单位：%
    system_percent FLOAT DEFAULT 0,           -- 内核态CPU使用率，单位：%
    nice_percent FLOAT DEFAULT 0,             -- 低优先级用户态CPU使用率，单位：%
    idle_percent FLOAT DEFAULT 0,             -- 空闲时间占比，单位：%
    io_wait_percent FLOAT DEFAULT 0,          -- IO等待时间占比，单位：%
    irq_percent FLOAT DEFAULT 0,              -- 硬中断时间百分比，单位：%
    soft_irq_percent FLOAT DEFAULT 0,         -- 软中断时间百分比，单位：%
    -- 负载指标
    load_avg_1 FLOAT DEFAULT 0,               -- 1分钟负载
    load_avg_3 FLOAT DEFAULT 0,               -- 3分钟负载
    load_avg_15 FLOAT DEFAULT 0,              -- 15分钟负载
    -- 内存指标
    mem_used_percent FLOAT DEFAULT 0,         -- 内存使用率，单位：%
    total FLOAT DEFAULT 0,                    -- 总内存，单位：MB
    free FLOAT DEFAULT 0,                     -- 空闲内存，单位：MB
    avail FLOAT DEFAULT 0,                    -- 可用内存，单位：MB
    -- 磁盘指标
    disk_util_percent FLOAT DEFAULT 0,        -- 磁盘利用率（最大值），单位：%
    -- 网络指标
    send_rate FLOAT DEFAULT 0,                -- 发送速率，单位：B/s
    rcv_rate FLOAT DEFAULT 0,                 -- 接收速率，单位：B/s
    -- 性能评分
    score FLOAT DEFAULT 0,                    -- 综合评分，范围：0-100，分数越高性能越好
    -- CPU 变化率
    cpu_percent_rate FLOAT DEFAULT 0,         -- CPU总使用率变化率
    usr_percent_rate FLOAT DEFAULT 0,         -- 用户态CPU变化率
    system_percent_rate FLOAT DEFAULT 0,      -- 内核态CPU变化率
    nice_percent_rate FLOAT DEFAULT 0,        -- 低优先级用户态CPU变化率
    idle_percent_rate FLOAT DEFAULT 0,        -- 空闲CPU变化率
    io_wait_percent_rate FLOAT DEFAULT 0,     -- IO等待变化率
    irq_percent_rate FLOAT DEFAULT 0,         -- 硬中断变化率
    soft_irq_percent_rate FLOAT DEFAULT 0,    -- 软中断变化率
    -- 负载变化率
    load_avg_1_rate FLOAT DEFAULT 0,          -- 1分钟负载变化率
    load_avg_3_rate FLOAT DEFAULT 0,          -- 3分钟负载变化率
    load_avg_15_rate FLOAT DEFAULT 0,         -- 15分钟负载变化率
    -- 内存变化率
    mem_used_percent_rate FLOAT DEFAULT 0,    -- 内存使用率变化率
    total_rate FLOAT DEFAULT 0,               -- 总内存变化率
    free_rate FLOAT DEFAULT 0,                -- 空闲内存变化率
    avail_rate FLOAT DEFAULT 0,               -- 可用内存变化率
    -- 磁盘变化率
    disk_util_percent_rate FLOAT DEFAULT 0,   -- 磁盘利用率变化率
    -- 网络变化率
    send_rate_rate FLOAT DEFAULT 0,           -- 发送速率变化率
    rcv_rate_rate FLOAT DEFAULT 0,            -- 接收速率变化率
    -- 时间戳
    timestamp DATETIME NOT NULL,              -- 采集时间
    INDEX idx_server_time(server_name, timestamp),
    INDEX idx_score(score)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 3. 网络详细数据表
CREATE TABLE IF NOT EXISTS server_net_detail (
    id INT AUTO_INCREMENT PRIMARY KEY,
    server_name VARCHAR(255) NOT NULL,
    net_name VARCHAR(64) NOT NULL,              -- 网卡名称，如 eth0, ens33
    -- 错误和丢弃统计
    err_in BIGINT DEFAULT 0,                    -- 接收错误数
    err_out BIGINT DEFAULT 0,                   -- 发送错误数
    drop_in BIGINT DEFAULT 0,                   -- 接收丢弃数
    drop_out BIGINT DEFAULT 0,                  -- 发送丢弃数
    -- 速率指标
    rcv_bytes_rate FLOAT DEFAULT 0,             -- 接收速率，单位：B/s
    rcv_packets_rate FLOAT DEFAULT 0,           -- 接收包数速率，单位：packets/s
    snd_bytes_rate FLOAT DEFAULT 0,             -- 发送速率，单位：B/s
    snd_packets_rate FLOAT DEFAULT 0,           -- 发送包数速率，单位：packets/s
    -- 速率变化率
    rcv_bytes_rate_rate FLOAT DEFAULT 0,        -- 接收速率变化率
    rcv_packets_rate_rate FLOAT DEFAULT 0,      -- 接收包数速率变化率
    snd_bytes_rate_rate FLOAT DEFAULT 0,        -- 发送速率变化率
    snd_packets_rate_rate FLOAT DEFAULT 0,      -- 发送包数速率变化率
    -- 错误和丢弃变化率
    err_in_rate FLOAT DEFAULT 0,                -- 接收错误数变化率
    err_out_rate FLOAT DEFAULT 0,               -- 发送错误数变化率
    drop_in_rate FLOAT DEFAULT 0,               -- 接收丢弃数变化率
    drop_out_rate FLOAT DEFAULT 0,              -- 发送丢弃数变化率
    -- 时间戳
    timestamp DATETIME NOT NULL,
    INDEX idx_server_net_time(server_name, net_name, timestamp)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 4. 软中断详细数据表
CREATE TABLE IF NOT EXISTS server_softirq_detail (
    id INT AUTO_INCREMENT PRIMARY KEY,
    server_name VARCHAR(255) NOT NULL,
    cpu_name VARCHAR(64) NOT NULL,
    -- 软中断计数
    hi BIGINT DEFAULT 0,
    timer BIGINT DEFAULT 0,
    net_tx BIGINT DEFAULT 0,
    net_rx BIGINT DEFAULT 0,
    block BIGINT DEFAULT 0,
    irq_poll BIGINT DEFAULT 0,
    tasklet BIGINT DEFAULT 0,
    sched BIGINT DEFAULT 0,
    hrtimer BIGINT DEFAULT 0,
    rcu BIGINT DEFAULT 0,
    -- 变化率
    hi_rate FLOAT DEFAULT 0,
    timer_rate FLOAT DEFAULT 0,
    net_tx_rate FLOAT DEFAULT 0,
    net_rx_rate FLOAT DEFAULT 0,
    block_rate FLOAT DEFAULT 0,
    irq_poll_rate FLOAT DEFAULT 0,
    tasklet_rate FLOAT DEFAULT 0,
    sched_rate FLOAT DEFAULT 0,
    hrtimer_rate FLOAT DEFAULT 0,
    rcu_rate FLOAT DEFAULT 0,
    -- 时间戳
    timestamp DATETIME NOT NULL,
    INDEX idx_server_cpu_time(server_name, cpu_name, timestamp)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 5. 内存详细数据表
CREATE TABLE IF NOT EXISTS server_mem_detail (
    id INT AUTO_INCREMENT PRIMARY KEY,
    server_name VARCHAR(255) NOT NULL,
    -- 内存指标
    total FLOAT DEFAULT 0,
    free FLOAT DEFAULT 0,
    avail FLOAT DEFAULT 0,
    buffers FLOAT DEFAULT 0,
    cached FLOAT DEFAULT 0,
    swap_cached FLOAT DEFAULT 0,
    active FLOAT DEFAULT 0,
    inactive FLOAT DEFAULT 0,
    active_anon FLOAT DEFAULT 0,
    inactive_anon FLOAT DEFAULT 0,
    active_file FLOAT DEFAULT 0,
    inactive_file FLOAT DEFAULT 0,
    dirty FLOAT DEFAULT 0,
    writeback FLOAT DEFAULT 0,
    anon_pages FLOAT DEFAULT 0,
    mapped FLOAT DEFAULT 0,
    kreclaimable FLOAT DEFAULT 0,
    sreclaimable FLOAT DEFAULT 0,
    sunreclaim FLOAT DEFAULT 0,
    -- 变化率
    total_rate FLOAT DEFAULT 0,
    free_rate FLOAT DEFAULT 0,
    avail_rate FLOAT DEFAULT 0,
    buffers_rate FLOAT DEFAULT 0,
    cached_rate FLOAT DEFAULT 0,
    swap_cached_rate FLOAT DEFAULT 0,
    active_rate FLOAT DEFAULT 0,
    inactive_rate FLOAT DEFAULT 0,
    active_anon_rate FLOAT DEFAULT 0,
    inactive_anon_rate FLOAT DEFAULT 0,
    active_file_rate FLOAT DEFAULT 0,
    inactive_file_rate FLOAT DEFAULT 0,
    dirty_rate FLOAT DEFAULT 0,
    writeback_rate FLOAT DEFAULT 0,
    anon_pages_rate FLOAT DEFAULT 0,
    mapped_rate FLOAT DEFAULT 0,
    kreclaimable_rate FLOAT DEFAULT 0,
    sreclaimable_rate FLOAT DEFAULT 0,
    sunreclaim_rate FLOAT DEFAULT 0,
    -- 时间戳
    timestamp DATETIME NOT NULL,
    INDEX idx_server_time(server_name, timestamp),
    INDEX idx_mem_used(total, free, avail)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 6. 磁盘详细数据表
CREATE TABLE IF NOT EXISTS server_disk_detail (
    id INT AUTO_INCREMENT PRIMARY KEY,
    server_name VARCHAR(255) NOT NULL,
    disk_name VARCHAR(64) NOT NULL,
    -- 磁盘计数器
    read_ops BIGINT DEFAULT 0,
    write_ops BIGINT DEFAULT 0,
    sectors_read BIGINT DEFAULT 0,
    sectors_written BIGINT DEFAULT 0,
    read_time_ms BIGINT DEFAULT 0,
    write_time_ms BIGINT DEFAULT 0,
    io_in_progress BIGINT DEFAULT 0,
    io_time_ms BIGINT DEFAULT 0,
    weighted_io_time_ms BIGINT DEFAULT 0,
    -- 计算指标
    read_bytes_per_sec FLOAT DEFAULT 0,
    write_bytes_per_sec FLOAT DEFAULT 0,
    read_iops FLOAT DEFAULT 0,
    write_iops FLOAT DEFAULT 0,
    avg_read_latency_ms FLOAT DEFAULT 0,
    avg_write_latency_ms FLOAT DEFAULT 0,
    util_percent FLOAT DEFAULT 0,
    -- 变化率
    read_bytes_per_sec_rate FLOAT DEFAULT 0,
    write_bytes_per_sec_rate FLOAT DEFAULT 0,
    read_iops_rate FLOAT DEFAULT 0,
    write_iops_rate FLOAT DEFAULT 0,
    avg_read_latency_ms_rate FLOAT DEFAULT 0,
    avg_write_latency_ms_rate FLOAT DEFAULT 0,
    util_percent_rate FLOAT DEFAULT 0,
    -- 时间戳
    timestamp DATETIME NOT NULL,
    INDEX idx_server_disk_time(server_name, disk_name, timestamp)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 7. MySQL 实例详细数据表
CREATE TABLE IF NOT EXISTS server_mysql_detail (
    id INT AUTO_INCREMENT PRIMARY KEY,
    server_name VARCHAR(255) NOT NULL,
    instance VARCHAR(255) NOT NULL,
    mysql_host VARCHAR(255) NOT NULL,
    mysql_port INT DEFAULT 0,
    up TINYINT(1) DEFAULT 0,
    version VARCHAR(128) DEFAULT '',
    `role` VARCHAR(32) DEFAULT 'unknown',
    -- 连接指标
    max_connections BIGINT UNSIGNED DEFAULT 0,
    threads_connected BIGINT UNSIGNED DEFAULT 0,
    threads_running BIGINT UNSIGNED DEFAULT 0,
    aborted_connects BIGINT UNSIGNED DEFAULT 0,
    -- 查询和事务计数器
    questions BIGINT UNSIGNED DEFAULT 0,
    com_select BIGINT UNSIGNED DEFAULT 0,
    com_insert BIGINT UNSIGNED DEFAULT 0,
    com_update BIGINT UNSIGNED DEFAULT 0,
    com_delete BIGINT UNSIGNED DEFAULT 0,
    com_commit BIGINT UNSIGNED DEFAULT 0,
    com_rollback BIGINT UNSIGNED DEFAULT 0,
    slow_queries BIGINT UNSIGNED DEFAULT 0,
    -- InnoDB 指标
    innodb_buffer_pool_read_requests BIGINT UNSIGNED DEFAULT 0,
    innodb_buffer_pool_reads BIGINT UNSIGNED DEFAULT 0,
    innodb_buffer_pool_hit_percent FLOAT DEFAULT 0,
    innodb_row_lock_waits BIGINT UNSIGNED DEFAULT 0,
    innodb_row_lock_time_avg_ms FLOAT DEFAULT 0,
    -- 复制指标
    replication_configured TINYINT(1) DEFAULT 0,
    replication_running TINYINT(1) DEFAULT 0,
    replication_lag_seconds FLOAT DEFAULT 0,
    -- 派生速率
    connection_used_percent FLOAT DEFAULT 0, -- 连接使用率，单位：%
    qps FLOAT DEFAULT 0, -- 每秒查询数，单位：queries/s
    tps FLOAT DEFAULT 0, -- 每秒事务数，单位：transactions/s
    slow_queries_rate FLOAT DEFAULT 0, -- 慢查询率，单位：%
    innodb_row_lock_waits_rate FLOAT DEFAULT 0, -- InnoDB行锁等待率，单位：%
    -- 时间戳
    timestamp DATETIME NOT NULL,
    INDEX idx_server_mysql_time(server_name, instance, timestamp),
    INDEX idx_mysql_instance(instance, timestamp),
    INDEX idx_mysql_up(up, timestamp)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 8. Redis 实例详细数据表
CREATE TABLE IF NOT EXISTS server_redis_detail (
    id INT AUTO_INCREMENT PRIMARY KEY,
    server_name VARCHAR(255) NOT NULL,
    instance VARCHAR(255) NOT NULL,
    redis_host VARCHAR(255) NOT NULL,
    redis_port INT DEFAULT 0,
    up TINYINT(1) DEFAULT 0,
    version VARCHAR(128) DEFAULT '',
    `role` VARCHAR(32) DEFAULT 'unknown',
    uptime_in_seconds BIGINT UNSIGNED DEFAULT 0,
    -- 连接指标
    connected_clients BIGINT UNSIGNED DEFAULT 0,
    blocked_clients BIGINT UNSIGNED DEFAULT 0,
    maxclients BIGINT UNSIGNED DEFAULT 0,
    connection_used_percent FLOAT DEFAULT 0,
    -- 内存指标
    used_memory BIGINT UNSIGNED DEFAULT 0,
    maxmemory BIGINT UNSIGNED DEFAULT 0,
    mem_fragmentation_ratio FLOAT DEFAULT 0,
    memory_used_percent FLOAT DEFAULT 0,
    -- 命令和命中率
    total_commands_processed BIGINT UNSIGNED DEFAULT 0,
    instantaneous_ops_per_sec FLOAT DEFAULT 0,
    commands_per_sec FLOAT DEFAULT 0,
    keyspace_hits BIGINT UNSIGNED DEFAULT 0,
    keyspace_misses BIGINT UNSIGNED DEFAULT 0,
    keyspace_hit_percent FLOAT DEFAULT 0,
    -- 键淘汰、错误和网络计数器
    expired_keys BIGINT UNSIGNED DEFAULT 0,
    evicted_keys BIGINT UNSIGNED DEFAULT 0,
    rejected_connections BIGINT UNSIGNED DEFAULT 0,
    total_error_replies BIGINT UNSIGNED DEFAULT 0,
    total_net_input_bytes BIGINT UNSIGNED DEFAULT 0,
    total_net_output_bytes BIGINT UNSIGNED DEFAULT 0,
    net_input_bytes_per_sec FLOAT DEFAULT 0,
    net_output_bytes_per_sec FLOAT DEFAULT 0,
    -- 复制和慢日志
    replication_configured TINYINT(1) DEFAULT 0,
    master_link_up TINYINT(1) DEFAULT 0,
    connected_slaves BIGINT UNSIGNED DEFAULT 0,
    master_last_io_seconds_ago FLOAT DEFAULT 0,
    slowlog_len BIGINT UNSIGNED DEFAULT 0,
    slowlog_growth FLOAT DEFAULT 0,
    timestamp DATETIME NOT NULL,
    INDEX idx_server_redis_time(server_name, instance, timestamp),
    INDEX idx_redis_instance(instance, timestamp),
    INDEX idx_redis_up(up, timestamp)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
