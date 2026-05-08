# Manager 压测结果记录

本文档统一记录 `manager_stress_test` 的压测 CLI 输出、manager 侧 metrics、单次分析和阶段性结论。manager metrics 为累计计数；如果测试前没有重启 manager 或记录基线，分析时只把它作为趋势参考。

## 统一判定标准

| 指标 | 通过条件 | 说明 |
|---|---:|---|
| `success_rate` | `>= 0.99`，无损压测建议 `1.0` | 客户端 RPC 成功率 |
| `p95_ms` | `<= 2000ms` | 客户端成功请求 p95 延迟 |
| `queue_rejected_delta` | `0` | manager 内部队列不能拒绝请求 |
| `task_timeouts_delta` | `0` | query 任务不能超时 |
| `dropped_monitor_samples_delta` | `0` | 无损落库压测必须为 0 |
| `mysql_errors_delta` | `0` | MySQL 写入或查询不能出现错误 |

## 测试记录 1：无间隔极限压测

### 测试配置

| 项目 | 值 |
|---|---:|
| worker 间隔 | 未配置，RPC 返回后立刻发下一次 |
| query 间隔 | 未配置，RPC 返回后立刻发下一次 |
| query 类型 | `latest` |
| auto 搜索上限 | worker `256`，query `256` |

### 压测客户端输出

```text
Summary
  worker_only_max_concurrency=256
  query_only_max_concurrency=256
  mixed_max_worker_concurrency=255
  mixed_max_query_concurrency=255
  mixed_max_total_concurrency=510
```

### manager metrics

```text
[manager.metrics] worker_requests=549435 query_requests=330488 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=6 queue_rejected=0 dropped_monitor_samples=535093 task_timeouts=0 task_errors=0 mysql_errors=0 redis_errors=0 pool_timeouts=0 pool_reconnects=24
[manager.metrics] worker_requests=549435 query_requests=330488 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=8 queue_rejected=0 dropped_monitor_samples=535093 task_timeouts=0 task_errors=0 mysql_errors=0 redis_errors=0 pool_timeouts=0 pool_reconnects=26
```

### 统一分析

| 项目 | 结果 |
|---|---|
| 客户端 RPC | 在 256/256 上限内通过，说明 gRPC 接收和查询返回没有被压垮 |
| manager 队列 | `queue_size=0`，`queue_rejected=0`，dispatcher 无积压和拒绝 |
| query | `task_timeouts=0`，查询没有 manager 内部超时 |
| 持久化 | `dropped_monitor_samples=535093`，样本丢弃非常严重 |
| MySQL | `mysql_errors=0`，没有显式 SQL 错误，但写入吞吐明显跟不上 |

### 结论

本轮只能证明 manager 的 RPC 接收/query 响应在当前上限内可用，不能证明无损落库。`dropped_monitor_samples / worker_requests = 535093 / 549435 ≈ 97.39%`，绝大多数 worker 样本被 manager 内部写队列丢弃。原因是 worker/query 都没有间隔，压测变成极限吞吐测试，MySQL 写队列和串行写入路径跟不上。

## 测试记录 2：加入间隔后的高并发混合压测

### 测试配置

| 项目 | 值 |
|---|---:|
| worker 并发 | mixed 阶段 `996` |
| query 并发 | mixed 阶段 `996` |
| duration | `10s` |
| worker 间隔 | `10000ms` |
| query 间隔 | `100ms` |
| query 类型 | `latest` |
| auto 搜索上限 | worker `1000`，query `1000` |

### 压测客户端输出

```text
[run] workers=996 queries=996 duration=10s worker_interval_ms=10000 query_interval_ms=100 query_kind=latest
  verdict=PASS elapsed=10.52s
  worker: total=996 success=996 failure=0 success_rate=1.0000 rps=94.7 p50_ms=386.42 p95_ms=429.72 p99_ms=441.60
  query : total=16660 success=16660 failure=0 success_rate=1.0000 rps=1583.5 p50_ms=589.50 p95_ms=722.75 p99_ms=867.44

Summary
  worker_only_max_concurrency=1000
  query_only_max_concurrency=1000
  mixed_max_worker_concurrency=996
  mixed_max_query_concurrency=996
  mixed_max_total_concurrency=1992
```

### manager metrics

```text
[manager.metrics] worker_requests=18148 query_requests=235641 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=8 queue_rejected=0 dropped_monitor_samples=2418 task_timeouts=0 task_errors=0 mysql_errors=2 redis_errors=0 pool_timeouts=0 pool_reconnects=28
[manager.metrics] worker_requests=18148 query_requests=235641 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=2 queue_rejected=0 dropped_monitor_samples=2418 task_timeouts=0 task_errors=0 mysql_errors=2 redis_errors=0 pool_timeouts=0 pool_reconnects=28
[manager.metrics] worker_requests=18148 query_requests=235641 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=6 queue_rejected=0 dropped_monitor_samples=2418 task_timeouts=0 task_errors=0 mysql_errors=2 redis_errors=0 pool_timeouts=0 pool_reconnects=34
```

### 统一分析

| 项目 | 结果 |
|---|---|
| 客户端 RPC | `success_rate=1.0000`，worker/query 均无 RPC 失败 |
| worker 延迟 | p95 `429.72ms`，通过默认延迟阈值 |
| query 延迟 | p95 `722.75ms`，通过默认延迟阈值 |
| manager 队列 | `queue_size=0`，`queue_rejected=0`，无队列拒绝 |
| query | `task_timeouts=0`，无查询任务超时 |
| 持久化 | `dropped_monitor_samples=2418`，仍存在样本丢弃 |
| MySQL | `mysql_errors=2`，存在少量 MySQL 错误 |

### 结论

本轮在客户端视角 PASS，但不是无损通过。`worker_interval_ms=10000` 且 `duration=10s`，每个 worker 线程基本只发送 1 次；query 侧由于 p50 已达到 `589.50ms`，实际吞吐由 RPC 耗时限制在约 `1583.5 QPS`。manager 侧仍出现 `dropped_monitor_samples` 和 `mysql_errors`，说明持久化链路仍存在压力或错误，需要以 `dropped_monitor_samples_delta == 0` 和 `mysql_errors_delta == 0` 作为下一轮目标。

## 测试记录 3：无损基线压测

### 测试配置 A：mixed auto

| 项目 | 值 |
|---|---:|
| duration | `60s` |
| warmup | `5s` |
| worker 间隔 | `10000ms` |
| query 间隔 | `100ms` |
| query 类型 | `latest` |
| auto 搜索上限 | worker `256`，query `512` |

### 压测客户端输出 A

```text
Summary
  worker_only_max_concurrency=256
  query_only_max_concurrency=512
  mixed_max_worker_concurrency=255
  mixed_max_query_concurrency=510
  mixed_max_total_concurrency=765
```

### manager metrics A

```text
[manager.metrics] worker_requests=16198 query_requests=1254291 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=8 queue_rejected=0 dropped_monitor_samples=0 task_timeouts=0 task_errors=0 mysql_errors=0 redis_errors=0 pool_timeouts=0 pool_reconnects=92
[manager.metrics] worker_requests=16198 query_requests=1254291 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=8 queue_rejected=0 dropped_monitor_samples=0 task_timeouts=0 task_errors=0 mysql_errors=0 redis_errors=0 pool_timeouts=0 pool_reconnects=92
```

### 测试配置 B：worker-only

| 项目 | 值 |
|---|---:|
| worker 并发 | `256` |
| query 并发 | `0` |
| duration | `120s` |
| worker 间隔 | `10000ms` |
| query 间隔 | `0ms` |
| query 类型 | `latest` |

### 压测客户端输出 B

```text
[run] workers=256 queries=0 duration=120s worker_interval_ms=10000 query_interval_ms=0 query_kind=latest
  verdict=PASS elapsed=120.01s
  worker: total=3072 success=3072 failure=0 success_rate=1.0000 rps=25.6 p50_ms=44.79 p95_ms=57.89 p99_ms=61.52
```

### manager metrics B

```text
[manager.metrics] worker_requests=3328 query_requests=0 queue_size=0 business_threads=12 mysql_write_available=2 mysql_query_available=2 redis_available=8 queue_rejected=0 dropped_monitor_samples=0 task_timeouts=0 task_errors=0 mysql_errors=0 redis_errors=0 pool_timeouts=0 pool_reconnects=52
[manager.metrics] worker_requests=3328 query_requests=0 queue_size=0 business_threads=12 mysql_write_available=2 mysql_query_available=2 redis_available=8 queue_rejected=0 dropped_monitor_samples=0 task_timeouts=0 task_errors=0 mysql_errors=0 redis_errors=0 pool_timeouts=0 pool_reconnects=52
```

### 统一分析

| 项目 | mixed auto 结果 | worker-only 结果 |
|---|---|---|
| 客户端 RPC | auto 到达上限内最大值 | `success_rate=1.0000` |
| worker 延迟 | 未提供详细延迟 | p95 `57.89ms`，p99 `61.52ms` |
| worker 吞吐 | 由 255 worker / 10s 推送模型决定 | `25.6 RPS` |
| query 压力 | `query_requests=1254291`，无超时 | 无 query |
| manager 队列 | `queue_size=0`，`queue_rejected=0` | `queue_size=0`，`queue_rejected=0` |
| 持久化 | `dropped_monitor_samples=0` | `dropped_monitor_samples=0` |
| MySQL | `mysql_errors=0` | `mysql_errors=0` |

### 结论

本轮建立了当前无损基线：

- `256` 台 worker，每台每 `10s` 推送一次，即约 `25.6 worker push/s`，manager 可无损接收并落库。
- worker-only 场景下 p95 约 `57.89ms`，p99 约 `61.52ms`。
- mixed 场景下，在 `255 workers + 510 query threads` 的配置内，manager 无队列拒绝、无查询超时、无样本丢弃、无 MySQL 错误。
- `worker_requests=3328` 比客户端统计的 `3072` 多 `256`，原因是 manager metrics 包含 warmup 阶段，而客户端统计排除了 warmup。

## 阶段性总结

| 阶段 | worker/query 压力 | 客户端结果 | manager 结果 | 结论 |
|---|---|---|---|---|
| 测试 1 | 无间隔极限压测，最高 `255+255` mixed | PASS | `dropped_monitor_samples=535093` | RPC 可用，但持久化严重丢样本 |
| 测试 2 | `996+996` mixed，worker 10s，query 100ms | PASS | `dropped_monitor_samples=2418`，`mysql_errors=2` | 接收/query 通过，但不是无损 |
| 测试 3 | `255+510` mixed；`256` worker-only | PASS | `dropped_monitor_samples=0`，`mysql_errors=0` | 当前确认的无损基线 |

当前可信结论：manager 在 `256` 台 worker 每 `10s` 上报一次、同时叠加约 `510` 个 `latest` 查询线程的配置下可以无损运行。更高 worker/query 上限尚未确认，需要继续升档测试。

## 下一轮建议

继续寻找无损边界，建议固定档位逐步升压，而不是一次性拉满 auto：

```bash
./build/Debug/tests/manager_stress_test \
  --manager localhost:50051 \
  --mode mixed \
  --worker-concurrency 512 \
  --query-concurrency 1024 \
  --worker-interval-ms 10000 \
  --query-interval-ms 100 \
  --duration-seconds 120 \
  --warmup-seconds 5 \
  --min-success-rate 1.0 \
  --max-p95-ms 2000
```

如果 `512 workers + 1024 queries` 仍满足 `dropped_monitor_samples_delta == 0` 和 `mysql_errors_delta == 0`，再升到 `768 workers + 1536 queries`。如果出现 dropped 或 MySQL error，则在最近一次通过档位和失败档位之间二分。

这次结果很好，已经证明了一个“无损基线”。

**结果解读**

第一组 mixed/auto：

```text
worker_only_max_concurrency=256
query_only_max_concurrency=512
mixed_max_worker_concurrency=255
mixed_max_query_concurrency=510
mixed_total=765
```

manager 侧：

```text
dropped_monitor_samples=0
mysql_errors=0
queue_rejected=0
task_timeouts=0
```

结论：在 `worker_interval_ms=10000`、`query_interval_ms=100`、worker cap 256、query cap 512 的配置下，manager 的 RPC、查询、写库链路都通过了。注意这里的 “max” 只是压测上限内的最大值，不代表真实上限，因为 worker/query 都顶到了你设置的 cap。

第二组 worker-only：

```text
workers=256
duration=120s
worker_interval_ms=10000
worker total=3072
rps=25.6
p95=57.89ms
success_rate=100%
```

manager 侧：

```text
worker_requests=3328
dropped_monitor_samples=0
mysql_errors=0
```

这里 `3328` 比客户端统计的 `3072` 多 `256`，是合理的：客户端统计排除了 warmup，manager 统计包含 warmup 阶段。`256` 个 worker 在 warmup 开始时各发了一次，所以多一轮。

**当前结论**

可以写成报告结论：

```text
在 256 台 worker 每 10 秒推送一次，即约 25.6 worker push/s 的压力下，manager 可无损接收并落库，p95 延迟约 58ms。

在同时叠加约 510 个 query 并发、单 query 线程最小间隔 100ms 的查询压力下，manager 仍无队列拒绝、无查询超时、无样本丢弃、无 MySQL 错误。

当前确认的无损并发基线：
worker-only: 256 workers
query-only: 512 query threads
mixed: 255 workers + 510 query threads
```

**下一次压测计划**

目标：继续向上找无损边界。建议不要直接用 auto 跑太大，先用固定档位，方便判断是哪一侧先出问题。

1. mixed 升级一档：

```bash
./build/Debug/tests/manager_stress_test \
  --manager localhost:50051 \
  --mode mixed \
  --worker-concurrency 512 \
  --query-concurrency 1024 \
  --worker-interval-ms 10000 \
  --query-interval-ms 100 \
  --duration-seconds 120 \
  --warmup-seconds 5 \
  --min-success-rate 1.0 \
  --max-p95-ms 2000
```

2. worker-only 升级一档：

```bash
./build/Debug/tests/manager_stress_test \
  --manager localhost:50051 \
  --mode worker \
  --worker-concurrency 512 \
  --worker-interval-ms 10000 \
  --duration-seconds 120 \
  --warmup-seconds 5 \
  --min-success-rate 1.0 \
  --max-p95-ms 2000
```

3. query-only 升级一档：

```bash
./build/Debug/tests/manager_stress_test \
  --manager localhost:50051 \
  --mode query \
  --query-concurrency 1024 \
  --query-interval-ms 100 \
  --duration-seconds 120 \
  --warmup-seconds 5 \
  --min-success-rate 1.0 \
  --max-p95-ms 2000
```

每组通过标准仍然看 manager 增量：

```text
dropped_monitor_samples_delta == 0
mysql_errors_delta == 0
queue_rejected_delta == 0
task_timeouts_delta == 0
```

如果 `512 workers + 1024 queries` 仍然无损，下一档再上 `768 workers + 1536 queries`。如果出现 dropped，就在 `256` 和失败档之间二分。