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

## 测试记录 6：mixed 升至 1024+2048

### 测试配置

| 项目 | 值 |
|---|---:|
| worker 并发 | `1024` |
| query 并发 | `2048` |
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
  --worker-concurrency 1024 \
  --query-concurrency 2048 \
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

[run] workers=1024 queries=2048 duration=120s worker_interval_ms=10000 query_interval_ms=100 query_kind=latest
  verdict=PASS elapsed=121.17s
  worker: total=12288 success=12288 failure=0 success_rate=1.0000 rps=101.4 p50_ms=373.17 p95_ms=505.53 p99_ms=532.24
  query : total=199957 success=199957 failure=0 success_rate=1.0000 rps=1650.2 p50_ms=1210.02 p95_ms=1420.57 p99_ms=1506.53
```

### manager metrics

```text
[manager.metrics] worker_requests=13312 query_requests=209408 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=2 redis_available=4 queue_rejected=0 dropped_monitor_samples=1628 task_timeouts=0 task_errors=0 mysql_errors=0 redis_errors=0 pool_timeouts=0 pool_reconnects=38
[manager.metrics] worker_requests=13312 query_requests=209408 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=2 redis_available=4 queue_rejected=0 dropped_monitor_samples=1628 task_timeouts=0 task_errors=0 mysql_errors=0 redis_errors=0 pool_timeouts=0 pool_reconnects=44
[manager.metrics] worker_requests=13312 query_requests=209408 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=8 queue_rejected=0 dropped_monitor_samples=1628 task_timeouts=0 task_errors=0 mysql_errors=0 redis_errors=0 pool_timeouts=0 pool_reconnects=50
[manager.metrics] worker_requests=13312 query_requests=209408 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=8 queue_rejected=0 dropped_monitor_samples=1628 task_timeouts=0 task_errors=0 mysql_errors=0 redis_errors=0 pool_timeouts=0 pool_reconnects=50
```

### 统一分析

| 项目 | 结果 |
|---|---|
| 客户端 RPC | worker/query 均 `success_rate=1.0000`，无失败 |
| worker 吞吐 | `101.4 RPS`，约等于 `1024 workers / 10s` |
| worker 延迟 | p50 `373.17ms`，p95 `505.53ms`，p99 `532.24ms` |
| query 吞吐 | `1650.2 RPS` |
| query 延迟 | p50 `1210.02ms`，p95 `1420.57ms`，p99 `1506.53ms` |
| manager 队列 | `queue_size=0`，`queue_rejected=0`，dispatcher 无积压和拒绝 |
| query | `task_timeouts=0`，无查询任务超时 |
| 持久化 | `dropped_monitor_samples=1628`，出现样本丢弃 |
| MySQL | `mysql_errors=0`，无 MySQL 错误 |

### 结论

本轮客户端视角 PASS，但无损判定 FAIL。`queue_rejected=0` 和 `task_timeouts=0` 说明 gRPC 接收、dispatcher 队列和查询任务仍能工作；失败点在持久化链路，`dropped_monitor_samples=1628` 表明 MySQL 写队列或写入路径已经跟不上 `1024` 台 worker 每 `10s` 上报一次的 mixed 压力。

与测试记录 5 相比，worker p95 从 `463.17ms` 上升到 `505.53ms`，query p95 从 `1060.02ms` 上升到 `1420.57ms`，但仍低于 `2000ms`。query 吞吐仍在约 `1.65k QPS`，主要变化是延迟继续升高。当前无损上界尚未到 `1024+2048`，最近确认的无损档位仍是 `768 workers + 1536 query threads`。

`worker_requests=13312` 比客户端统计的 `12288` 多 `1024`，符合 warmup 额外一轮 worker 上报的预期。

## 测试记录 7：mixed 二分测试 896+1792

### 测试配置

| 项目 | 值 |
|---|---:|
| worker 并发 | `896` |
| query 并发 | `1792` |
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
  --worker-concurrency 896 \
  --query-concurrency 1792 \
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

[run] workers=896 queries=1792 duration=120s worker_interval_ms=10000 query_interval_ms=100 query_kind=latest
  verdict=PASS elapsed=120.98s
  worker: total=10752 success=10752 failure=0 success_rate=1.0000 rps=88.9 p50_ms=328.29 p95_ms=476.54 p99_ms=514.41
  query : total=193327 success=193327 failure=0 success_rate=1.0000 rps=1598.0 p50_ms=1083.87 p95_ms=1330.44 p99_ms=1560.15
```

### manager metrics

```text
[manager.metrics] worker_requests=11648 query_requests=202655 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=8 queue_rejected=0 dropped_monitor_samples=0 task_timeouts=0 task_errors=0 mysql_errors=0 redis_errors=0 pool_timeouts=0 pool_reconnects=14
[manager.metrics] worker_requests=11648 query_requests=202655 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=8 queue_rejected=0 dropped_monitor_samples=0 task_timeouts=0 task_errors=0 mysql_errors=0 redis_errors=0 pool_timeouts=0 pool_reconnects=14
[manager.metrics] worker_requests=11648 query_requests=202655 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=8 queue_rejected=0 dropped_monitor_samples=0 task_timeouts=0 task_errors=0 mysql_errors=0 redis_errors=0 pool_timeouts=0 pool_reconnects=14
[manager.metrics] worker_requests=11648 query_requests=202655 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=8 queue_rejected=0 dropped_monitor_samples=0 task_timeouts=0 task_errors=0 mysql_errors=0 redis_errors=0 pool_timeouts=0 pool_reconnects=14
```

### 统一分析

| 项目 | 结果 |
|---|---|
| 客户端 RPC | worker/query 均 `success_rate=1.0000`，无失败 |
| worker 吞吐 | `88.9 RPS`，约等于 `896 workers / 10s` |
| worker 延迟 | p50 `328.29ms`，p95 `476.54ms`，p99 `514.41ms` |
| query 吞吐 | `1598.0 RPS` |
| query 延迟 | p50 `1083.87ms`，p95 `1330.44ms`，p99 `1560.15ms` |
| manager 队列 | `queue_size=0`，`queue_rejected=0`，无积压和拒绝 |
| query | `task_timeouts=0`，无查询任务超时 |
| 持久化 | `dropped_monitor_samples=0`，无样本丢弃 |
| MySQL | `mysql_errors=0`，无 MySQL 错误 |

### 结论

本轮是明确的无损 PASS。manager 在 `896` 台 worker 每 `10s` 上报一次，同时叠加 `1792` 个 `latest` 查询线程的压力下，仍保持无队列拒绝、无任务超时、无样本丢弃、无 MySQL 错误。当前确认的 mixed 无损基线从 `768 workers + 1536 query threads` 提升到 `896 workers + 1792 query threads`。

与失败档 `1024+2048` 相比，本轮 worker 吞吐从 `101.4 RPS` 降到 `88.9 RPS`，query p95 从 `1420.57ms` 降到 `1330.44ms`，且持久化恢复无损。当前 mixed 无损边界位于 `896+1792` 与 `1024+2048` 之间。

`worker_requests=11648` 比客户端统计的 `10752` 多 `896`，符合 warmup 额外一轮 worker 上报的预期。

## 测试记录 8：mixed 二分测试 960+1920

### 测试配置

| 项目 | 值 |
|---|---:|
| worker 并发 | `960` |
| query 并发 | `1920` |
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
  --worker-concurrency 960 \
  --query-concurrency 1920 \
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

[run] workers=960 queries=1920 duration=120s worker_interval_ms=10000 query_interval_ms=100 query_kind=latest
  verdict=PASS elapsed=121.04s
  worker: total=11520 success=11520 failure=0 success_rate=1.0000 rps=95.2 p50_ms=391.05 p95_ms=549.85 p99_ms=594.84
  query : total=203735 success=203735 failure=0 success_rate=1.0000 rps=1683.2 p50_ms=1117.02 p95_ms=1290.36 p99_ms=1411.12
```

### manager metrics

```text
[manager.metrics] worker_requests=12480 query_requests=213265 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=8 queue_rejected=0 dropped_monitor_samples=681 task_timeouts=0 task_errors=0 mysql_errors=1 redis_errors=0 pool_timeouts=0 pool_reconnects=15
[manager.metrics] worker_requests=12480 query_requests=213265 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=8 queue_rejected=0 dropped_monitor_samples=681 task_timeouts=0 task_errors=0 mysql_errors=1 redis_errors=0 pool_timeouts=0 pool_reconnects=15
[manager.metrics] worker_requests=12480 query_requests=213265 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=8 queue_rejected=0 dropped_monitor_samples=681 task_timeouts=0 task_errors=0 mysql_errors=1 redis_errors=0 pool_timeouts=0 pool_reconnects=15
[manager.metrics] worker_requests=12480 query_requests=213265 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=8 queue_rejected=0 dropped_monitor_samples=681 task_timeouts=0 task_errors=0 mysql_errors=1 redis_errors=0 pool_timeouts=0 pool_reconnects=15
```

### 统一分析

| 项目 | 结果 |
|---|---|
| 客户端 RPC | worker/query 均 `success_rate=1.0000`，无失败 |
| worker 吞吐 | `95.2 RPS`，约等于 `960 workers / 10s` |
| worker 延迟 | p50 `391.05ms`，p95 `549.85ms`，p99 `594.84ms` |
| query 吞吐 | `1683.2 RPS` |
| query 延迟 | p50 `1117.02ms`，p95 `1290.36ms`，p99 `1411.12ms` |
| manager 队列 | `queue_size=0`，`queue_rejected=0`，dispatcher 无积压和拒绝 |
| query | `task_timeouts=0`，无查询任务超时 |
| 持久化 | `dropped_monitor_samples=681`，出现样本丢弃 |
| MySQL | `mysql_errors=1`，出现 MySQL 错误 |

### 结论

本轮客户端视角 PASS，但无损判定 FAIL。虽然所有 RPC 均成功且 query p95 低于 `2000ms`，manager 侧出现 `dropped_monitor_samples=681` 和 `mysql_errors=1`，说明持久化链路在 `960` 台 worker 每 `10s` 上报一次、叠加 `1920` 个 query 线程时已经不稳定。

与测试记录 7 相比，worker p95 从 `476.54ms` 上升到 `549.85ms`，worker RPS 从 `88.9` 上升到 `95.2`；query p95 从 `1330.44ms` 降到 `1290.36ms`，但 manager 持久化指标变差。因此当前无损边界位于 `896+1792` 与 `960+1920` 之间，最近确认的无损档位仍是 `896 workers + 1792 query threads`。

`worker_requests=12480` 比客户端统计的 `11520` 多 `960`，符合 warmup 额外一轮 worker 上报的预期。

## 测试记录 9：mixed 最后一轮 928+1856

### 测试配置

| 项目 | 值 |
|---|---:|
| worker 并发 | `928` |
| query 并发 | `1856` |
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
  --worker-concurrency 928 \
  --query-concurrency 1856 \
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

[run] workers=928 queries=1856 duration=120s worker_interval_ms=10000 query_interval_ms=100 query_kind=latest
  verdict=PASS elapsed=121.02s
  worker: total=11136 success=11136 failure=0 success_rate=1.0000 rps=92.0 p50_ms=332.03 p95_ms=445.01 p99_ms=511.10
  query : total=202471 success=202471 failure=0 success_rate=1.0000 rps=1673.0 p50_ms=1085.07 p95_ms=1212.53 p99_ms=1315.16
```

### manager metrics

```text
[manager.metrics] worker_requests=12064 query_requests=209702 queue_size=1819 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=7 queue_rejected=0 dropped_monitor_samples=52 task_timeouts=0 task_errors=0 mysql_errors=0 redis_errors=0 pool_timeouts=0 pool_reconnects=14
[manager.metrics] worker_requests=12064 query_requests=211675 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=8 queue_rejected=0 dropped_monitor_samples=52 task_timeouts=0 task_errors=0 mysql_errors=0 redis_errors=0 pool_timeouts=0 pool_reconnects=14
[manager.metrics] worker_requests=12064 query_requests=211675 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=8 queue_rejected=0 dropped_monitor_samples=52 task_timeouts=0 task_errors=0 mysql_errors=0 redis_errors=0 pool_timeouts=0 pool_reconnects=14
[manager.metrics] worker_requests=12064 query_requests=211675 queue_size=0 business_threads=12 mysql_write_available=1 mysql_query_available=4 redis_available=8 queue_rejected=0 dropped_monitor_samples=52 task_timeouts=0 task_errors=0 mysql_errors=0 redis_errors=0 pool_timeouts=0 pool_reconnects=14
```

### 统一分析

| 项目 | 结果 |
|---|---|
| 客户端 RPC | worker/query 均 `success_rate=1.0000`，无失败 |
| worker 吞吐 | `92.0 RPS`，约等于 `928 workers / 10s` |
| worker 延迟 | p50 `332.03ms`，p95 `445.01ms`，p99 `511.10ms` |
| query 吞吐 | `1673.0 RPS` |
| query 延迟 | p50 `1085.07ms`，p95 `1212.53ms`，p99 `1315.16ms` |
| manager 队列 | 峰值观测到 `queue_size=1819`，随后回落到 `0`，`queue_rejected=0` |
| query | `task_timeouts=0`，无查询任务超时 |
| 持久化 | `dropped_monitor_samples=52`，出现少量样本丢弃 |
| MySQL | `mysql_errors=0`，无 MySQL 错误 |

### 结论

本轮客户端视角 PASS，但无损判定 FAIL。虽然所有 RPC 成功、query p95 低于 `2000ms`，manager 侧出现 `dropped_monitor_samples=52`，说明 `928` 台 worker 每 `10s` 上报一次、叠加 `1856` 个 query 线程时已经越过了当前无损持久化边界。

本轮首次在 metrics 中观测到 `queue_size=1819`，之后队列回落到 `0`，说明短时间内出现过内部任务积压但最终被消费完；不过样本丢弃已经发生，因此不能按无损通过计。

`worker_requests=12064` 比客户端统计的 `11136` 多 `928`，符合 warmup 额外一轮 worker 上报的预期。

## 阶段性总结

| 阶段 | worker/query 压力 | 客户端结果 | manager 结果 | 结论 |
|---|---|---|---|---|
| 测试 1 | 无间隔极限压测，最高 `255+255` mixed | PASS | `dropped_monitor_samples=535093` | RPC 可用，但持久化严重丢样本 |
| 测试 2 | `996+996` mixed，worker 10s，query 100ms | PASS | `dropped_monitor_samples=2418`，`mysql_errors=2` | 接收/query 通过，但不是无损 |
| 测试 3 | `255+510` mixed；`256` worker-only | PASS | `dropped_monitor_samples=0`，`mysql_errors=0` | 建立无损基线 |
| 测试 4 | `512+1024` mixed，worker 10s，query 100ms | PASS | `dropped_monitor_samples=0`，`mysql_errors=0` | mixed 无损基线上升到 `512+1024` |
| 测试 5 | `768+1536` mixed，worker 10s，query 100ms | PASS | `dropped_monitor_samples=0`，`mysql_errors=0` | mixed 无损基线上升到 `768+1536` |
| 测试 6 | `1024+2048` mixed，worker 10s，query 100ms | 客户端 PASS | `dropped_monitor_samples=1628` | RPC 通过，但无损判定失败 |
| 测试 7 | `896+1792` mixed，worker 10s，query 100ms | PASS | `dropped_monitor_samples=0`，`mysql_errors=0` | mixed 无损基线上升到 `896+1792` |
| 测试 8 | `960+1920` mixed，worker 10s，query 100ms | 客户端 PASS | `dropped_monitor_samples=681`，`mysql_errors=1` | RPC 通过，但无损判定失败 |
| 测试 9 | `928+1856` mixed，worker 10s，query 100ms | 客户端 PASS | `dropped_monitor_samples=52` | RPC 通过，但无损判定失败 |

当前可信结论：manager 在 `896` 台 worker 每 `10s` 上报一次、同时叠加 `1792` 个 `latest` 查询线程的配置下可以无损运行。`928 workers + 1856 query threads` 已出现 `52` 个 dropped monitor samples，因此本轮 mixed 压测确认的无损边界位于 `896+1792` 与 `928+1856` 之间。

## Mixed 最终结论

在本轮测试条件下：

| 项目 | 最终结论 |
|---|---|
| worker 上报间隔 | `10000ms` |
| query 请求间隔 | `100ms` |
| query 类型 | `QueryLatestScore` |
| 压测时长 | `120s`，warmup `5s` |
| 客户端通过条件 | `success_rate=1.0`，p95 `< 2000ms` |
| 无损通过条件 | `dropped_monitor_samples_delta=0`，`mysql_errors_delta=0`，`queue_rejected_delta=0`，`task_timeouts_delta=0` |
| 最后确认无损档位 | `896 workers + 1792 query threads` |
| 最近失败档位 | `928 workers + 1856 query threads` |
| 确认边界区间 | `(896+1792, 928+1856]` |

最后确认的无损档位性能：

```text
worker: 88.9 RPS, p95=476.54ms, p99=514.41ms
query : 1598.0 RPS, p95=1330.44ms, p99=1560.15ms
manager: queue_rejected=0, dropped_monitor_samples=0, task_timeouts=0, mysql_errors=0
```

因此，当前 mixed 场景建议按 `896 workers + 1792 query threads` 作为可发布的无损容量结论。`928+1856` 虽然客户端成功率和延迟仍达标，但 manager 已有样本丢弃，不能作为无损容量。
