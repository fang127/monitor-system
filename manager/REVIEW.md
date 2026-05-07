下面是一份方便 review 的 Markdown 报告，你可以直接放到 PR 描述里。

```markdown
# Manager 高并发高可用改造说明

## 总体目标

本次改造围绕 manager 的并发瓶颈展开：

- worker 上报和 api_gateway 查询统一进入业务调度层。
- gRPC handler 不直接执行重业务，避免网络 IO 线程被 MySQL/Redis/日志阻塞。
- 引入 `folly::MPMCQueue` 作为统一任务入口。
- MySQL/Redis 使用连接池、健康检查、RAII guard，降低连接泄漏和串行阻塞风险。
- 所有核心并发参数从环境变量读取，不写死。

## 新增文件

### `manager/include/ManagerConfig.h`
### `manager/src/ManagerConfig.cc`

新增 manager 配置结构 `ManagerConfig` 和环境变量加载逻辑。

覆盖配置包括：

- gRPC poller：`MANAGER_GRPC_NUM_CQS`、`MANAGER_GRPC_MIN_POLLERS`、`MANAGER_GRPC_MAX_POLLERS`
- 任务队列：`MANAGER_TASK_QUEUE_CAPACITY`、高低水位、任务超时
- 业务线程池：最小/最大线程数、空闲回收时间
- MySQL 读写池：min/max、连接超时、读写超时、健康检查、idle TTL
- Redis 池：min/max、URI、超时、健康检查、缓存 TTL
- 日志/指标输出开关

### `manager/include/ManagerMetrics.h`

新增轻量级原子指标结构，用于周期输出：

- worker/query 请求数
- 队列拒绝数
- 丢弃样本数
- 任务超时/错误数
- MySQL/Redis 错误数
- 连接池超时和重连次数

### `manager/include/ManagerDispatcher.h`
### `manager/src/ManagerDispatcher.cc`

新增统一业务调度器。

核心行为：

- 使用 `folly::MPMCQueue<ManagerTask>` 作为统一入口队列。
- gRPC handler 是 producer，业务线程是 consumer。
- worker 上报任务通过 `submitMonitorTask()` 投递。
- api_gateway 查询任务通过 `submitQueryTask()` 投递，并使用 `future/promise` 等待结果。
- 查询任务等待有超时，超时返回 `DEADLINE_EXCEEDED`。
- 队列高水位触发业务线程扩容，最大线程数受配置限制。
- 队列满时 worker 样本允许被拒绝并计数，避免拖垮进程。

### `manager/include/MysqlConnectionPool.h`
### `manager/src/MysqlConnectionPool.cc`

新增 MySQL 连接池。

核心行为：

- 支持 min/max 连接数。
- `Guard` RAII 借还连接，避免泄漏。
- 借连接带超时。
- 借出前/后台定期 `mysql_ping` 健康检查。
- 不健康连接会重建。
- 空闲连接超过 TTL 后回收，但保留 min 连接数。
- 支持读池和写池分开实例化。

### `manager/include/RedisConnectionPool.h`
### `manager/src/RedisConnectionPool.cc`

新增 Redis 连接池和 `RedisCache` 包装。

核心行为：

- 支持 min/max 连接数。
- `Guard` RAII 借还连接。
- 后台 ping 健康检查。
- Redis 不可用时降级，不阻塞 worker 上报。
- `RedisCache` 提供 `set/get`，用于最新分数和热点查询缓存。

注意：`redis-plus-plus 1.3.15` 里 `ConnectionOptions` 没有 URI 构造，也没有 `parse_uri()`；应使用 `sw::redis::Redis(uri)` 构造，超时参数通过 URI query string 传入。

## 修改文件

### `conanfile.txt` / 建议升级为 `conanfile.py`

新增依赖：

- `folly/2024.08.12.00`
- `redis-plus-plus/1.3.15`

由于 Folly 引入的 `lz4/1.10.0` 和原依赖图中的 `lz4/1.9.4` 冲突，建议将 `conanfile.txt` 升级为 `conanfile.py`，并加：

```python
self.requires("lz4/1.10.0", override=True)
```

### `manager/CMakeLists.txt`

新增 manager 源文件编译项，并链接 Folly / redis-plus-plus。

需要注意 target：

- Folly target 应使用 `Folly::folly`
- redis-plus-plus 静态库 target 通常是 `redis++::redis++_static`
- 如果 shared 构建，可能是 `redis++::redis++`

建议 CMake 对 Redis target 做兼容判断。

### `manager/src/main.cc`

入口改造：

- 加载 `ManagerConfig`
- 创建 `ManagerMetrics`
- 启动 `ManagerDispatcher`
- 创建 MySQL 写连接池和查询连接池
- 创建 Redis 连接池和缓存
- 将 `GrpcServerImpl` 和 `QueryServiceImpl` 接入统一 dispatcher
- 配置 gRPC 同步服务 poller 参数
- 启动周期 metrics 输出线程

### `manager/include/rpc/GrpcServer.h`
### `manager/src/rpc/GrpcServer.cc`

worker 上报服务改造：

- 增加 `ManagerDispatcher*`
- `SetMonitorInfo` 仍做基础校验和最新 `hostDatas_` 更新
- callback 不再直接同步执行，而是复制请求后投递到 dispatcher
- gRPC handler 快速返回，避免被 `HostManager::onDataReceived()`、MySQL 或日志阻塞

### `manager/include/rpc/QueryService.h`
### `manager/src/rpc/QueryService.cc`

查询服务改造：

- 增加 `ManagerDispatcher*` 和 `RedisCache*`
- 每个 `Query*` RPC 外层先复制 request，投递到 dispatcher
- 业务线程中复用原有查询逻辑，填充临时 response
- future 成功后再复制到 gRPC response，避免超时后写悬空指针
- `QueryLatestScore` 增加 Redis 缓存读写

### `manager/include/HostManager.h`
### `manager/src/HostManager.cc`

HostManager 改造：

- 新增 `configure()` 注入配置、metrics、MySQL 写池、Redis cache
- 移除接收路径上的全局 `receiveMutex_`
- `onDataReceived()` 只做：
  - hostID 解析
  - score 计算
  - 变化率计算
  - 内存 score 快照更新
  - Redis 最新分数缓存
  - MySQL 写任务入队
- MySQL 写入改为后台写线程处理
- MySQL 写队列有容量限制，满时丢弃旧样本并计数
- 默认关闭每条上报的大段日志，只有 `MANAGER_VERBOSE_LOG=true` 时打印详细数据

注意：`writeToMysql()` 内部还有一些静态历史样本 map，用于详细表变化率计算，因此当前写入线程会串行调用该函数，避免这些静态状态并发竞争。

### `manager/include/QueryManager.h`
### `manager/src/QueryManager.cc`

查询管理器改造：

- `init()` 支持注入 MySQL 查询连接池
- `isInitialized()` 支持连接池模式
- 新增 `acquireConnection()`，优先从查询池借连接
- 查询方法不再使用单个 `conn_ + mutex_` 串行所有请求
- fallback 模式仍保留旧单连接逻辑

## Review 重点

- 确认 `CompatMPMCQueue.h` 这类临时 fallback 文件不应保留。
- 确认 `ManagerDispatcher` 中 future 超时后不会写原始 gRPC response。
- 确认 MySQL/Redis guard 析构路径一定归还连接。
- 确认 Redis URI 构造方式兼容当前 `redis-plus-plus 1.3.15`。
- 确认 `HostManager::writeToMysql()` 的静态状态仍被串行保护。
- 确认 `QueryManager` 所有 SQL 查询都使用借出的 `MYSQL* conn`，没有遗漏 `conn_`。
- 确认 CMake target 名称：`Folly::folly` 和 `redis++::redis++_static`。
```