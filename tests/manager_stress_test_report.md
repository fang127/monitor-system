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

## 测试记录 4：mixed 升级一档

### 测试配置

| 项目 | 值 |
|---|---:|
| worker 并发 | `512` |
| query 并发 | `1024` |
| duration | `120s` |
| warmup | `5s` |
| worker 间隔 | `10000ms` |
| query 间隔 | `100ms` |
| query 类型 | `latest` |
| success rate 阈值 | `1.0` |
| p95 阈值 | `2000ms` |

### 执行命令

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

### 压测客户端输出

```text
Manager stress test
  manager=localhost:50051
  mode=mixed
  query_kind=latest
  duration=120s warmup=5s timeout=3000ms
  intervals: worker=10000ms query=100ms
  pass: success_rate>=1 p95<=2000.000000ms

[run] workers=512 queries=1024 duration=120s worker_interval_ms=10000 query_interval_ms=100 query_kind=latest
  verdict=PASS elapsed=120.55s
  worker: total=6144 success=6144 failure=0 success_rate=1.0000 rps=51.0 p50_ms=210.04 p95_ms=385.31 p99_ms=417.60
  query : total=202614 success=202614 failure=0 success_rate=1.0000 rps=1680.8 p50_ms=596.66 p95_ms=689.43 p99_ms=762.58
```

### manager metrics

```text
Removing stale host: stress-worker-0380_10.250.1.190
Removing stale host: stress-worker-0472_10.250.2.82
[manager.metrics] worker_requests=6656 query_requests=210565 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=8 queue_rejected=0 dropped_monitor_samples=0 task_timeouts=0 task_errors=0 mysql_errors=0 redis_errors=0 pool_timeouts=0 pool_reconnects=26
[manager.metrics] worker_requests=6656 query_requests=210565 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=8 queue_rejected=0 dropped_monitor_samples=0 task_timeouts=0 task_errors=0 mysql_errors=0 redis_errors=0 pool_timeouts=0 pool_reconnects=26
[manager.metrics] worker_requests=6656 query_requests=210565 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=8 queue_rejected=0 dropped_monitor_samples=0 task_timeouts=0 task_errors=0 mysql_errors=0 redis_errors=0 pool_timeouts=0 pool_reconnects=26
```

### 统一分析

| 项目 | 结果 |
|---|---|
| 客户端 RPC | worker/query 均 `success_rate=1.0000`，无失败 |
| worker 吞吐 | `51.0 RPS`，约等于 `512 workers / 10s` |
| worker 延迟 | p50 `210.04ms`，p95 `385.31ms`，p99 `417.60ms` |
| query 吞吐 | `1680.8 RPS` |
| query 延迟 | p50 `596.66ms`，p95 `689.43ms`，p99 `762.58ms` |
| manager 队列 | `queue_size=0`，`queue_rejected=0`，无积压和拒绝 |
| query | `task_timeouts=0`，无查询任务超时 |
| 持久化 | `dropped_monitor_samples=0`，无样本丢弃 |
| MySQL | `mysql_errors=0`，无 MySQL 错误 |

### 结论

本轮是明确的无损 PASS。manager 在 `512` 台 worker 每 `10s` 上报一次，同时叠加 `1024` 个 `latest` 查询线程的压力下，仍保持无队列拒绝、无任务超时、无样本丢弃、无 MySQL 错误。当前确认的 mixed 无损基线从 `255 workers + 510 query threads` 提升到 `512 workers + 1024 query threads`。

`worker_requests=6656` 比客户端统计的 `6144` 多 `512`，符合 warmup 额外一轮 worker 上报的预期。

## 测试记录 5：mixed 再升一档

### 测试配置

| 项目 | 值 |
|---|---:|
| worker 并发 | `768` |
| query 并发 | `1536` |
| duration | `120s` |
| warmup | `5s` |
| worker 间隔 | `10000ms` |
| query 间隔 | `100ms` |
| query 类型 | `latest` |
| success rate 阈值 | `1.0` |
| p95 阈值 | `2000ms` |

### 执行命令

```bash
./build/Debug/tests/manager_stress_test \
  --manager localhost:50051 \
  --mode mixed \
  --worker-concurrency 768 \
  --query-concurrency 1536 \
  --worker-interval-ms 10000 \
  --query-interval-ms 100 \
  --duration-seconds 120 \
  --warmup-seconds 5 \
  --min-success-rate 1.0 \
  --max-p95-ms 2000
```

### 压测客户端输出

```text
Manager stress test
  manager=localhost:50051
  manager=localhost:50051
  mode=mixed
  query_kind=latest
  duration=120s warmup=5s timeout=3000ms
  intervals: worker=10000ms query=100ms
  pass: success_rate>=1 p95<=2000.000000ms

[run] workers=768 queries=1536 duration=120s worker_interval_ms=10000 query_interval_ms=100 query_kind=latest
  verdict=PASS elapsed=120.83s
  worker: total=9216 success=9216 failure=0 success_rate=1.0000 rps=76.3 p50_ms=311.21 p95_ms=463.17 p99_ms=492.23
  query : total=201271 success=201271 failure=0 success_rate=1.0000 rps=1665.8 p50_ms=890.96 p95_ms=1060.02 p99_ms=1256.57
```

### manager metrics

```text
[manager.metrics] worker_requests=9984 query_requests=210492 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=8 queue_rejected=0 dropped_monitor_samples=0 task_timeouts=0 task_errors=0 mysql_errors=0 redis_errors=0 pool_timeouts=0 pool_reconnects=14
[manager.metrics] worker_requests=9984 query_requests=210492 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=8 queue_rejected=0 dropped_monitor_samples=0 task_timeouts=0 task_errors=0 mysql_errors=0 redis_errors=0 pool_timeouts=0 pool_reconnects=14
[manager.metrics] worker_requests=9984 query_requests=210492 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=8 queue_rejected=0 dropped_monitor_samples=0 task_timeouts=0 task_errors=0 mysql_errors=0 redis_errors=0 pool_timeouts=0 pool_reconnects=14
[manager.metrics] worker_requests=9984 query_requests=210492 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=8 queue_rejected=0 dropped_monitor_samples=0 task_timeouts=0 task_errors=0 mysql_errors=0 redis_errors=0 pool_timeouts=0 pool_reconnects=14
```

### 统一分析

| 项目 | 结果 |
|---|---|
| 客户端 RPC | worker/query 均 `success_rate=1.0000`，无失败 |
| worker 吞吐 | `76.3 RPS`，约等于 `768 workers / 10s` |
| worker 延迟 | p50 `311.21ms`，p95 `463.17ms`，p99 `492.23ms` |
| query 吞吐 | `1665.8 RPS` |
| query 延迟 | p50 `890.96ms`，p95 `1060.02ms`，p99 `1256.57ms` |
| manager 队列 | `queue_size=0`，`queue_rejected=0`，无积压和拒绝 |
| query | `task_timeouts=0`，无查询任务超时 |
| 持久化 | `dropped_monitor_samples=0`，无样本丢弃 |
| MySQL | `mysql_errors=0`，无 MySQL 错误 |

### 结论

本轮是明确的无损 PASS。manager 在 `768` 台 worker 每 `10s` 上报一次，同时叠加 `1536` 个 `latest` 查询线程的压力下，仍保持无队列拒绝、无任务超时、无样本丢弃、无 MySQL 错误。当前确认的 mixed 无损基线从 `512 workers + 1024 query threads` 提升到 `768 workers + 1536 query threads`。

与测试记录 4 相比，worker p95 从 `385.31ms` 上升到 `463.17ms`，query p95 从 `689.43ms` 上升到 `1060.02ms`。query 吞吐仍在约 `1.66k QPS`，但延迟已经明显上升，说明查询侧并发继续增加时主要表现为排队/等待时间增长。

`worker_requests=9984` 比客户端统计的 `9216` 多 `768`，符合 warmup 额外一轮 worker 上报的预期。

## 阶段性总结

| 阶段 | worker/query 压力 | 客户端结果 | manager 结果 | 结论 |
|---|---|---|---|---|
| 测试 1 | 无间隔极限压测，最高 `255+255` mixed | PASS | `dropped_monitor_samples=535093` | RPC 可用，但持久化严重丢样本 |
| 测试 2 | `996+996` mixed，worker 10s，query 100ms | PASS | `dropped_monitor_samples=2418`，`mysql_errors=2` | 接收/query 通过，但不是无损 |
| 测试 3 | `255+510` mixed；`256` worker-only | PASS | `dropped_monitor_samples=0`，`mysql_errors=0` | 建立无损基线 |
| 测试 4 | `512+1024` mixed，worker 10s，query 100ms | PASS | `dropped_monitor_samples=0`，`mysql_errors=0` | mixed 无损基线上升到 `512+1024` |
| 测试 5 | `768+1536` mixed，worker 10s，query 100ms | PASS | `dropped_monitor_samples=0`，`mysql_errors=0` | mixed 无损基线上升到 `768+1536` |

当前可信结论：manager 在 `768` 台 worker 每 `10s` 上报一次、同时叠加 `1536` 个 `latest` 查询线程的配置下可以无损运行。当前 worker 写入吞吐约 `76.3 RPS`，query 吞吐约 `1665.8 RPS`。查询侧 p95 已达到约 `1.06s`，继续升档时需要重点观察 query p95/p99 和 task timeout。

## 下一轮建议

继续寻找无损边界，下一档建议升到 `1024 workers + 2048 queries`：

```bash
./build/Debug/tests/manager_stress_test \
  --manager localhost:50051 \
  --mode mixed \
  --worker-concurrency 1024 \
  --query-concurrency 2048 \
  --worker-interval-ms 10000 \
  --query-interval-ms 100 \
  --duration-seconds 120 \
  --warmup-seconds 5 \
  --min-success-rate 1.0 \
  --max-p95-ms 2000
```

通过标准仍然看 manager 增量：

```text
dropped_monitor_samples_delta == 0
mysql_errors_delta == 0
queue_rejected_delta == 0
task_timeouts_delta == 0
```

如果 `1024 workers + 2048 queries` 仍然无损，再升到 `1280 workers + 2560 queries`。如果出现 dropped、MySQL error、query p95 超过阈值或 task timeout，则在 `768+1536` 和失败档位之间二分。
