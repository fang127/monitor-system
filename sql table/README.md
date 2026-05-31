# MySQL 表结构说明

本目录集中保存 `monitor-system` 使用的 MySQL 初始化脚本。部署和排查数据库问题时，请优先查看这里，而不是到各模块目录中寻找表结构。

## 脚本列表

| 脚本                          | 归属模块                        | 说明                                             |
| ----------------------------- | ------------------------------- | ------------------------------------------------ |
| `identity_access_schema.sql`  | api_gateway、web、agent_service | 创建租户、团队、登录用户和用户团队成员关系表。   |
| `init_server_performance.sql` | manager、api_gateway、worker    | 创建监控主库、服务器性能汇总表和各类指标明细表。 |
| `monitor_asset_schema.sql`    | manager、api_gateway、worker    | 创建集群、服务器和 worker 注册归属表。           |
| `agent_memory_schema.sql`     | agent_service                   | 创建 AI 运维服务长期记忆相关表。                 |

这些脚本都包含：

```sql
CREATE DATABASE IF NOT EXISTS `monitor-system` DEFAULT CHARACTER SET utf8mb4;
USE `monitor-system`;
```

因此它们可以独立执行，也可以在 Docker MySQL 首次初始化时一起挂载到 `/docker-entrypoint-initdb.d/`。

## 执行方式

本地已有 MySQL 时，可以从项目根目录执行：

```bash
mysql -h 127.0.0.1 -P 3306 -uroot -p < "sql table/init_server_performance.sql"
mysql -h 127.0.0.1 -P 3306 -uroot -p < "sql table/identity_access_schema.sql"
mysql -h 127.0.0.1 -P 3306 -uroot -p < "sql table/monitor_asset_schema.sql"
mysql -h 127.0.0.1 -P 3306 -uroot -p < "sql table/agent_memory_schema.sql"
```

使用根目录 Docker Compose 时，`deploy/docker-compose.yml` 会把整个 `sql table` 目录挂载到 MySQL 初始化目录。只有首次创建 `mysql_data` 数据卷时，MySQL 官方镜像才会自动执行这些脚本；如果数据卷已存在，需要手动执行迁移脚本或重建数据卷。

## 表分组

### 基础身份与鉴权

| 表名                    | 写入方        | 主要消费方                            | 说明                                                                                                                                                                                                            |
| ----------------------- | ------------- | ------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `tenants`               | `api_gateway` | `api_gateway`、`agent_service`        | 租户表，是用户、团队、资产和记忆作用域的顶层可信来源。                                                                                                                                                          |
| `teams`                 | `api_gateway` | `api_gateway`、`web`、`agent_service` | 团队表，属于租户；登录后 JWT 当前团队来自该表和成员关系。                                                                                                                                                       |
| `users`                 | `api_gateway` | `api_gateway`、`web`                  | HTTP API 登录账号表。`api_gateway` 直接读写该表；`web` 通过 HTTP API 使用登录和用户管理能力。`api_gateway` 启动时也会通过 GORM 确保该表存在，并在表为空时按 `ADMIN_USERNAME`、`ADMIN_PASSWORD` 引导创建管理员。 |
| `user_team_memberships` | `api_gateway` | `api_gateway`、`agent_service`        | 用户与团队成员关系表，保存成员角色和状态，是普通用户访问租户/团队资源的授权依据。                                                                                                                               |

`identity_access_schema.sql` 只创建表结构，不自动创建默认租户或默认团队。部署时必须显式创建租户、团队和成员关系；缺少租户或团队的请求会被 `agent_service` 记忆层拒绝。

### 监控资产归属

| 表名                   | 写入方                   | 主要消费方                                | 说明                                                                |
| ---------------------- | ------------------------ | ----------------------------------------- | ------------------------------------------------------------------- |
| `clusters`             | `manager`、`api_gateway` | `manager`、`api_gateway`、`agent_service` | 集群归属表，将集群绑定到租户和团队。                                |
| `servers`              | `manager`、`api_gateway` | `manager`、`api_gateway`                  | 服务器资产表，用于把监控事实关联到租户、团队和集群。                |
| `worker_registrations` | `manager`                | `manager`                                 | worker 注册表，manager 通过该表校验 worker 身份并解析可信数据归属。 |

### 监控数据

| 表名                    | 写入方    | 主要消费方                                             | 说明                                                                                                       |
| ----------------------- | --------- | ------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------- |
| `server_performance`    | `manager` | `manager` QueryService、`api_gateway`                  | 每次 worker 上报后的主机汇总性能、健康评分和核心变化率。                                                   |
| `server_net_detail`     | `manager` | `manager` QueryService、`api_gateway`                  | 每张网卡的收发速率、包速率、错误数、丢弃数和变化率。                                                       |
| `server_softirq_detail` | `manager` | `manager` QueryService、`api_gateway`                  | 每个 CPU 的软中断计数和变化率。                                                                            |
| `server_mem_detail`     | `manager` | `manager` QueryService、`api_gateway`                  | 内存容量、缓存、Swap、Dirty、Slab 等明细和变化率。                                                         |
| `server_disk_detail`    | `manager` | `manager` QueryService、`api_gateway`                  | 每块磁盘的 I/O 计数、吞吐、IOPS、延迟、利用率和变化率。                                                    |
| `server_mysql_detail`   | `manager` | `manager` QueryService、`api_gateway`、`agent_service` | worker 采集到的 MySQL 实例可用性、连接压力、QPS/TPS、慢查询、锁等待、Buffer Pool 命中率和复制状态。        |
| `server_redis_detail`   | `manager` | `manager` QueryService、`api_gateway`、`agent_service` | worker 采集到的 Redis 实例可用性、连接压力、内存压力、命令吞吐、命中率、淘汰、拒绝连接、复制状态和慢日志。 |

所有监控事实表都保存 `tenant_id`、`team_id`、`cluster_id` 和 `server_id`。manager 写入时不会从 worker 自报值采信这些字段，而是通过 `worker_registrations` 解析可信归属。无法解析显式租户、团队和集群的上报会被拒绝写入。

监控事实的写入链路是：

```text
worker -> manager -> MySQL
```

`agent_service` 不直接读取这些表，它通过 `api_gateway` 查询监控事实。

### agent_service 记忆系统

| 表名                       | 写入方          | 主要消费方      | 说明                                                |
| -------------------------- | --------------- | --------------- | --------------------------------------------------- |
| `agent_memory_scopes`      | `agent_service` | `agent_service` | 保存租户、团队、集群级长期记忆开关和策略。          |
| `agent_session_memories`   | `agent_service` | `agent_service` | 保存会话摘要和最近消息窗口，按租户、团队、集群、用户和 session 隔离。 |
| `agent_long_term_memories` | `agent_service` | `agent_service` | 保存长期记忆正文、来源、创建者、置信度、状态和向量索引 ID。           |
| `agent_memory_events`      | `agent_service` | `agent_service` | 记录记忆创建、召回、更新、删除等治理事件和操作者。                    |

长期记忆默认关闭。启用前需要先准备这些 MySQL 表，并保持 Milvus 中的长期记忆 collection `agent_long_term_memories` 与内部文档知识库 `ops_docs` 分开。

## 维护约定

- 新增或调整 MySQL 表结构时，把迁移脚本放在本目录。
- 根目录 README 只保留数据库入口链接，表级说明统一维护在本文件。
- 模块 README 可以说明自己依赖哪些表，但不要复制完整建表语句。
