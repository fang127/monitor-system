# Manager

`manager` 是监控系统的中心服务，负责接收所有 worker 推送的 `MonitorInfo` 数据，计算主机健康评分，将数据写入 MySQL，并通过 gRPC `QueryService` 对外提供查询能力。它是一个同步 gRPC 服务进程，默认监听 `0.0.0.0:50051`。

## 角色定位

在整个系统中，manager 位于数据链路的中间层：

```text
worker nodes
    |
    | gRPC GrpcManager.SetMonitorInfo(MonitorInfo)
    v
+----------------------------------------------------+
| manager                                            |
|                                                    |
|  GrpcServerImpl       ManagerDispatcher            |
|  接收 worker push --> 统一业务队列/线程池          |
|                               |                    |
|                               v                    |
|                         HostManager                |
|                         评分/变化率/最新状态       |
|                               |                    |
|                               v                    |
|                         MySQL 写入队列/写连接池    |
|                                                    |
|  QueryServiceImpl --> ManagerDispatcher --> QueryManager
|       ^                                      |
|       |                                      v
|       +--------- gRPC 查询客户端/API Gateway/MySQL
+----------------------------------------------------+
```

manager 同时注册两个 gRPC service：

- `GrpcManager`：接收 worker 推送，核心接口是 `SetMonitorInfo`。
- `QueryService`：查询历史性能、趋势、异常、评分排行和详细指标。

## 源码结构

```text
manager/
├── CMakeLists.txt
├── include/
│   ├── HostManager.h              # 主机状态、评分、写库入口
│   ├── ManagerConfig.h            # manager 环境变量配置
│   ├── ManagerDispatcher.h        # 统一业务任务队列和线程池
│   ├── ManagerMetrics.h           # 运行期计数指标
│   ├── MysqlConfig.h              # MySQL 环境变量配置
│   ├── MysqlConnectionPool.h      # MySQL 连接池
│   ├── QueryManager.h             # SQL 查询封装
│   ├── RedisConnectionPool.h      # Redis 连接池和缓存封装
│   └── rpc/
│       ├── GrpcServer.h           # GrpcManager 实现声明
│       └── QueryService.h         # QueryService 实现声明
├── src/
│   ├── main.cc                    # 进程入口和服务组装
│   ├── HostManager.cc             # 评分、变化率、MySQL 写入
│   ├── ManagerConfig.cc           # 环境变量加载
│   ├── ManagerDispatcher.cc       # 队列、线程扩缩容、超时控制
│   ├── MysqlConnectionPool.cc
│   ├── QueryManager.cc            # 查询 SQL 实现
│   ├── RedisConnectionPool.cc
│   └── rpc/
│       ├── GrpcServer.cc
│       └── QueryService.cc
└── README.md
```

数据库脚本统一维护在根目录 `sql table/`，表结构说明见 [sql table/README.md](../sql%20table/README.md)。

## 启动流程

`src/main.cc` 是 manager 的装配入口，主要步骤如下：

1. 读取监听地址。未传参时使用 `0.0.0.0:50051`，也可以通过第一个命令行参数覆盖。
2. 调用 `loadManagerConfigFromEnv()` 加载 gRPC、任务队列、线程池、MySQL、Redis 和日志配置。
3. 创建 `ManagerMetrics`，用于统计请求量、队列拒绝、超时、连接池错误等运行指标。
4. 创建并启动 `ManagerDispatcher`，它负责把监控写入任务和查询任务放到统一业务队列中执行。
5. 初始化 MySQL 写连接池和查询连接池。
6. 如果 `MANAGER_REDIS_ENABLED=true`，初始化 Redis 连接池和缓存对象。
7. 创建 `GrpcServerImpl`，把 worker 推送回调绑定到 `HostManager::onDataReceived()`。
8. 创建并启动 `HostManager`，维护在线主机状态并异步写入 MySQL。
9. 创建 `QueryManager`，连接 MySQL 并供 `QueryServiceImpl` 使用。
10. 创建 `QueryServiceImpl`，注册 gRPC 查询服务。
11. 启动 gRPC server，并周期性打印 manager metrics。

## 数据接收链路

worker 会调用：

```proto
rpc SetMonitorInfo(MonitorInfo) returns (google.protobuf.Empty)
```

manager 端处理过程：

1. `GrpcServerImpl::SetMonitorInfo()` 校验请求、主机名和 `x-monitor-worker-id`。
2. 将最新 `MonitorInfo` 放入 `hostDatas_` 内存 map，保留最近一次上报快照。
3. 如果配置了 `ManagerDispatcher`，把数据处理封装成 `MonitorPush` 任务提交到业务队列。
4. 业务线程执行 `HostManager::onDataReceived(info, workerIdentity)`。
5. `HostManager` 通过 `worker_registrations` 校验 worker 凭证，并解析租户、团队、集群和服务器归属；未知、禁用、凭证错误或缺少显式作用域的 worker 不会写入监控事实。
6. `HostManager` 生成主机 ID、计算评分、计算变化率、更新最新主机状态。
7. 写入 Redis 最新评分缓存。
8. 将带作用域的 `HostMonitoringData` 放入 MySQL 写队列，由写线程异步落库。

主机 ID 生成规则：

```text
host_info.hostname + "_" + host_info.ip_address
如果缺少其中之一，则优先使用 ip 或 hostname
兼容旧协议时回退到 MonitorInfo.name
```

## 调度器架构

`ManagerDispatcher` 使用 `folly::MPMCQueue<ManagerTask>` 作为多生产者多消费者任务队列，统一承载两类任务：

- `MonitorPush`：worker 上报后的业务处理。
- `Query`：gRPC 查询请求。

关键行为：

- `task_queue_capacity` 控制队列容量。
- 队列达到 `task_queue_high_watermark` 时会尝试扩容业务线程。
- 业务线程数量在 `business_threads_min` 和 `business_threads_max` 之间伸缩。
- 查询任务通过 `promise/future` 同步等待结果。
- 查询超过 `MANAGER_TASK_TIMEOUT_MS` 会返回 `DEADLINE_EXCEEDED`。
- 队列满时查询返回 `RESOURCE_EXHAUSTED`，监控样本会被计入丢弃指标。

这层设计把 gRPC poller 线程和实际业务处理解耦，避免慢 SQL 或大量 worker push 直接阻塞 gRPC 收包线程。

## 评分模型

`HostManager::calculateScore()` 将每台主机评分归一到 `0-100`，分数越高表示当前负载越健康。当前权重：

| 维度        | 权重 | 数据来源                          |
| ----------- | ---: | --------------------------------- |
| CPU 使用率  | 0.35 | `cpu_stat(0).cpu_percent`         |
| 内存使用率  | 0.30 | `mem_info.used_percent`           |
| 1 分钟 load | 0.15 | `cpu_load.load_avg_1`             |
| 磁盘利用率  | 0.15 | 所有磁盘 `util_percent` 最大值    |
| 网络吞吐    | 0.05 | 第一张网卡收发速率，按 1Gbps 归一 |

load 评分使用 `cpu_cores * 1.5` 作为归一化上限，更偏向 I/O intensive 场景。最终评分会 clamp 到 `[0, 100]`。

## MySQL 存储

数据库初始化脚本位于根目录：

```text
sql table/init_server_performance.sql
```

manager 当前写入 7 张监控数据表：

| 表名                    | 用途                                                                                            |
| ----------------------- | ----------------------------------------------------------------------------------------------- |
| `server_performance`    | 每次上报的汇总数据、健康评分、核心变化率                                                        |
| `server_net_detail`     | 每网卡网络速率、包速率、错误、丢弃                                                              |
| `server_softirq_detail` | 每 CPU 软中断计数和变化率                                                                       |
| `server_mem_detail`     | 内存明细和变化率                                                                                |
| `server_disk_detail`    | 每磁盘 I/O 计数、速率、延迟、利用率                                                             |
| `server_mysql_detail`   | 每个 MySQL 实例的可用性、连接压力、QPS/TPS、慢查询、锁等待、Buffer Pool 命中率和复制状态        |
| `server_redis_detail`   | 每个 Redis 实例的可用性、连接压力、内存压力、命令吞吐、命中率、淘汰、拒绝连接、复制状态和慢日志 |

这些事实表都包含 `tenant_id`、`team_id`、`cluster_id` 和 `server_id` 字段。`QueryService` 请求必须携带来自 `api_gateway` 当前 JWT 的查询作用域，`QueryManager` 会在概览、评分、趋势、异常和明细查询中统一应用该过滤条件。

写库链路是异步的：

```text
HostManager::onDataReceived()
    -> enqueueMysqlWrite()
    -> mysqlWriteLoop()
    -> writeToMysql()
```

`mysqlWriteLoop()` 使用多个写线程消费队列，但 `writeToMysql()` 内部目前仍通过互斥锁串行保护部分“上一轮样本”状态，因为详细变化率需要依赖每台主机、每块磁盘、每张网卡的前一次采样。

## Redis 缓存

当 `MANAGER_REDIS_ENABLED=true` 时，manager 会创建 Redis 连接池和 `RedisCache`：

- `manager:latest_score:<hostID>`：`HostManager` 在收到上报后写入单台主机最新评分 JSON。
- `manager:query:latest_score:<tenant_id>:<team_id>:<cluster_id>`：`QueryLatestScore` 查询结果的序列化 protobuf 缓存。

缓存 TTL 由 `MANAGER_REDIS_CACHE_TTL_SECONDS` 控制，默认 5 秒。Redis 不是必需组件；关闭后查询会直接读 MySQL。

## 查询服务

`QueryService` 定义在 `proto/query_api.proto`，manager 当前提供 11 个查询接口：

| RPC                  | 说明                                   |
| -------------------- | -------------------------------------- |
| `QueryPerformance`   | 按服务器和时间范围分页查询汇总性能数据 |
| `QueryTrend`         | 查询趋势，可按 `interval_seconds` 聚合 |
| `QueryAnomaly`       | 按阈值查询 CPU、内存、磁盘、变化率异常 |
| `QueryScoreRank`     | 查询服务器评分排行                     |
| `QueryLatestScore`   | 查询所有服务器最新评分和集群统计       |
| `QueryNetDetail`     | 查询网卡明细                           |
| `QueryDiskDetail`    | 查询磁盘明细                           |
| `QueryMemDetail`     | 查询内存明细                           |
| `QuerySoftIrqDetail` | 查询软中断明细                         |
| `QueryMysqlDetail`   | 查询 MySQL 实例明细                    |
| `QueryRedisDetail`   | 查询 Redis 实例明细                    |

所有查询最终进入 `QueryManager` 执行 SQL。`QueryServiceImpl` 会先校验查询作用域、时间范围和分页参数，再把内部结构转换为 protobuf 响应。缺少租户或团队作用域的查询会返回 `UNAUTHENTICATED`。

## 配置

MySQL 连接配置来自 `MysqlConfig`：

| 变量             | 默认/示例        | 说明       |
| ---------------- | ---------------- | ---------- |
| `MYSQL_HOST`     | `127.0.0.1`      | MySQL 地址 |
| `MYSQL_PORT`     | `3306`           | MySQL 端口 |
| `MYSQL_USER`     | `root`           | 用户名     |
| `MYSQL_PASSWORD` | `123456`         | 密码       |
| `MYSQL_DATABASE` | `monitor-system` | 数据库名   |

manager 运行配置来自 `ManagerConfig`：

| 变量                                   |                 默认值 | 说明                       |
| -------------------------------------- | ---------------------: | -------------------------- |
| `MANAGER_GRPC_NUM_CQS`                 |                      4 | gRPC completion queue 数   |
| `MANAGER_GRPC_MIN_POLLERS`             |                      8 | gRPC 最小 poller 数        |
| `MANAGER_GRPC_MAX_POLLERS`             |                     64 | gRPC 最大 poller 数        |
| `MANAGER_TASK_QUEUE_CAPACITY`          |                  10000 | 业务任务队列容量           |
| `MANAGER_TASK_QUEUE_HIGH_WATERMARK`    |                   8000 | 高水位，触发扩容           |
| `MANAGER_TASK_QUEUE_LOW_WATERMARK`     |                   3000 | 低水位，预留给流控扩展     |
| `MANAGER_TASK_TIMEOUT_MS`              |                   3000 | 查询任务等待超时           |
| `MANAGER_BUSINESS_THREADS_MIN`         |                      4 | 业务线程最小数量           |
| `MANAGER_BUSINESS_THREADS_MAX`         |                     16 | 业务线程最大数量           |
| `MANAGER_BUSINESS_IDLE_SHRINK_SECONDS` |                     30 | 空闲线程回收时间           |
| `MANAGER_MYSQL_WRITE_POOL_MIN`         |                      2 | MySQL 写连接池最小连接数   |
| `MANAGER_MYSQL_WRITE_POOL_MAX`         |                      4 | MySQL 写连接池最大连接数   |
| `MANAGER_MYSQL_QUERY_POOL_MIN`         |                      4 | MySQL 查询连接池最小连接数 |
| `MANAGER_MYSQL_QUERY_POOL_MAX`         |                      8 | MySQL 查询连接池最大连接数 |
| `MANAGER_MYSQL_CONNECT_TIMEOUT_MS`     |                   1000 | MySQL 连接超时             |
| `MANAGER_MYSQL_READ_TIMEOUT_MS`        |                   2000 | MySQL 读取超时             |
| `MANAGER_MYSQL_HEALTH_CHECK_SECONDS`   |                     10 | MySQL 连接健康检查间隔     |
| `MANAGER_MYSQL_IDLE_TTL_SECONDS`       |                     60 | MySQL 空闲连接 TTL         |
| `MANAGER_REDIS_ENABLED`                |                   true | 是否启用 Redis 缓存        |
| `MANAGER_REDIS_URI`                    | `tcp://127.0.0.1:6379` | Redis URI                  |
| `MANAGER_REDIS_POOL_MIN`               |                      2 | Redis 连接池最小连接数     |
| `MANAGER_REDIS_POOL_MAX`               |                      8 | Redis 连接池最大连接数     |
| `MANAGER_REDIS_CACHE_TTL_SECONDS`      |                      5 | 查询缓存 TTL               |
| `MANAGER_VERBOSE_LOG`                  |                  false | 是否打印每次上报的详细数据 |
| `MANAGER_METRICS_LOG_INTERVAL_SECONDS` |                     10 | metrics 日志间隔           |

示例配置文件：

```text
configs/app.env
```

加载方式：

```bash
set -a
source configs/app.env
set +a
```

## 构建

manager 依赖 gRPC、libmysqlclient、folly、redis-plus-plus。项目使用 Conan + CMake：

```bash
conan install . --build=missing --settings=build_type=Debug
cmake --preset conan-debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build --preset conan-debug --target manager
```

也可以使用根目录脚本执行完整 Debug 构建：

```bash
python3 build_debug.py
```

如果只想构建 worker 或测试工具，可以在顶层 CMake 配置中关闭 manager：

```bash
cmake -S . -B build/Debug \
  -DCMAKE_TOOLCHAIN_FILE=build/Debug/generators/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_MANAGER=OFF
```

## 运行

先启动基础服务：

```bash
set -a
source configs/app.env
set +a
docker compose --env-file configs/app.env -f deploy/docker-compose.yml up -d
```

启动 manager：

```bash
set -a
source configs/app.env
set +a
./build/Debug/manager/manager 0.0.0.0:50051
```

如果 manager 和 worker 在同一台机器上，也可以监听本地地址：

```bash
./build/Debug/manager/manager 127.0.0.1:50051
```

worker 侧连接地址需要和这里一致，例如：

```bash
./build/Debug/worker/worker 127.0.0.1:50051 10
```

## 本地验证

可以使用模拟 worker 工具快速验证 manager 的接收和写库链路：

```bash
./build/Debug/tests/simulated_workers_push localhost:50051 5 2 20
```

参数含义：

```text
simulated_workers_push [manager_address] [worker_count] [interval_seconds] [rounds]
```

`rounds=0` 表示持续推送，手动按 `Ctrl+C` 停止。

## 运行期观测

manager 会按 `MANAGER_METRICS_LOG_INTERVAL_SECONDS` 打印一行运行指标：

```text
[manager.metrics] worker_requests=... query_requests=... queue_size=...
```

重点关注：

- `queue_size`：业务队列长度，持续增长说明处理能力不足。
- `business_threads`：当前业务线程数。
- `queue_rejected`：队列满导致的拒绝次数。
- `dropped_monitor_samples`：被丢弃的监控样本数。
- `task_timeouts`：查询任务超时次数。
- `mysql_errors` / `redis_errors`：外部存储错误。
- `pool_timeouts` / `pool_reconnects`：连接池等待超时和重连次数。

调试单次上报内容时，可设置：

```bash
export MANAGER_VERBOSE_LOG=true
```

## 常见问题

### manager 启动后 QueryService 不可用

通常是 MySQL 初始化失败。检查：

- `MYSQL_HOST`、`MYSQL_PORT`、`MYSQL_USER`、`MYSQL_PASSWORD`、`MYSQL_DATABASE` 是否正确。
- `docker compose --env-file configs/app.env -f deploy/docker-compose.yml ps` 中 MySQL 是否 healthy。
- `sql table/init_server_performance.sql` 是否已执行。

### worker 推送成功但查不到数据

先确认 manager 控制台是否有 `worker_requests` 增长，再检查：

- `MANAGER_MYSQL_WRITE_POOL_MAX` 是否过小。
- MySQL 表是否存在。
- manager 是否编译了 `ENABLE_MYSQL`。
- 写库错误日志中是否出现字段或表结构不匹配。

### 查询偶发 DEADLINE_EXCEEDED

查询任务超过 `MANAGER_TASK_TIMEOUT_MS` 会返回超时。可以：

- 增大 `MANAGER_TASK_TIMEOUT_MS`。
- 增大 `MANAGER_BUSINESS_THREADS_MAX`。
- 增大 `MANAGER_MYSQL_QUERY_POOL_MAX`。
- 检查 MySQL 索引和慢查询。

### 队列拒绝或样本丢弃

说明 push 或 query 压力超过当前处理能力。可以：

- 增大 `MANAGER_TASK_QUEUE_CAPACITY`。
- 增大业务线程上限。
- 增大 MySQL 写连接池。
- 降低 worker 推送频率。
- 将 API Gateway 查询压力分散或增加缓存 TTL。

## 扩展建议

- 新增查询接口时，先修改 `proto/query_api.proto`，再在 `QueryServiceImpl` 做参数校验和 protobuf 转换，最后在 `QueryManager` 中实现 SQL。
- 新增持久化字段时，同时更新 `MonitorInfo`、`HostManager::writeToMysql()`、SQL 初始化脚本和查询结构。
- 如果要调整健康评分，只需要集中修改 `HostManager::calculateScore()`，并保持评分含义为“越高越健康”。
