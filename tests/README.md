# Simulated worker push test

这个目录提供两个 manager 测试工具：

- `simulated_workers_push`：轻量级 gRPC 推送测试程序，用来模拟多台 worker 主机向 manager 上报 `MonitorInfo` 数据。
- `manager_stress_test`：manager 压测客户端，用来探测 worker 推送侧、query 查询侧，以及二者同时工作时的最大通过并发。

## manager 压测

压测判定默认使用三个条件：

- 成功率 `>= 0.99`
- 成功请求 p95 延迟 `<= 2000ms`
- gRPC 调用没有出现明显的 `DEADLINE_EXCEEDED`、`RESOURCE_EXHAUSTED` 等错误堆积

一键自动探测：

```bash
cmake --build build/Debug --target manager_stress_test
./tests/manager_pressure_test.sh localhost:50051 \
  --duration-seconds 10 \
  --worker-interval-ms 10000 \
  --query-interval-ms 100 \
  --max-worker-concurrency 256 \
  --max-query-concurrency 256
```

输出最后的 `Summary` 会给出：

```text
worker_only_max_concurrency=...
query_only_max_concurrency=...
mixed_max_worker_concurrency=...
mixed_max_query_concurrency=...
mixed_max_total_concurrency=...
```

也可以单独压某一侧：

```bash
./build/Debug/tests/manager_stress_test --manager localhost:50051 --mode worker --worker-concurrency 64
./build/Debug/tests/manager_stress_test --manager localhost:50051 --mode query --query-concurrency 64
./build/Debug/tests/manager_stress_test --manager localhost:50051 --mode mixed --worker-concurrency 32 --query-concurrency 32
```

默认 `--worker-interval-ms 0` 和 `--query-interval-ms 0`，表示不限制频率：每个并发线程在一次 RPC 返回后立刻发下一次。要模拟真实 worker 每 10 秒推送一次，可以设置：

```bash
./build/Debug/tests/manager_stress_test \
  --manager localhost:50051 \
  --mode mixed \
  --worker-concurrency 256 \
  --query-concurrency 32 \
  --worker-interval-ms 10000 \
  --query-interval-ms 100
```

查询默认压 `QueryLatestScore`。如果想压更重的 SQL 查询，可指定：

```bash
./tests/manager_pressure_test.sh localhost:50051 --query-kind performance --query-server stress-worker-0001
```

可调参数：

```text
--min-success-rate FLOAT    默认 0.99
--max-p95-ms FLOAT          默认 2000，传 0 表示不限制 p95
--request-timeout-ms N      默认 3000
--duration-seconds N        默认 10
--worker-interval-ms N      默认 0，单个 worker 并发线程两次请求开始时间的间隔
--query-interval-ms N       默认 0，单个 query 并发线程两次请求开始时间的间隔
--query-kind latest|rank|performance
```

自动模式会先分别寻找 worker-only 和 query-only 的最大通过并发，再按两者独立最大值的比例缩放，搜索 mixed 场景下可同时通过的 worker/query 并发组合。

## 模拟 worker 推送

默认行为：

- 模拟 5 台主机：`worker-01` 到 `worker-05`
- 每台主机使用不同 IP：`10.10.0.101` 到 `10.10.0.105`
- 每台主机额外模拟 1 个 MySQL 实例和 1 个 Redis 实例，随轮次递增累计计数器，用于验证 `server_mysql_detail`、`server_redis_detail` 落库和派生速率
- 默认每 2 秒推送一次
- 默认每台 worker 推送 5 轮后退出

`manager_stress_test` 的 worker 侧压测请求也会携带 MySQL/Redis 明细。压测时需要把这部分写库成本计入吞吐结果，观察 manager metrics 中的 `mysql_errors`、`redis_errors`、`dropped_monitor_samples` 和 MySQL/Redis 明细表增长是否符合预期。

## 构建

项目默认使用 Conan 依赖。先安装依赖：

```bash
conan install . --build=missing --settings=build_type=Debug
```

只构建 proto、worker 和测试工具时，可以关闭 manager，避免本地没有 MySQL 开发库时构建失败：

```bash
cmake -S . -B build/Debug \
  -DCMAKE_TOOLCHAIN_FILE=build/Debug/generators/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_MANAGER=OFF \
  -DENABLE_EBPF=OFF \
  -DBUILD_TESTS=ON
cmake --build build/Debug --target simulated_workers_push
```

## 运行

先启动 manager：

```bash
./build/Debug/manager/manager 0.0.0.0:50051
```

再启动模拟 worker：

```bash
./build/Debug/tests/simulated_workers_push localhost:50051
```

参数格式：

```bash
./build/Debug/tests/simulated_workers_push [manager_address] [worker_count] [interval_seconds] [rounds]
```

示例：模拟 5 台 worker，每 1 秒推送一次，每台推送 20 轮：

```bash
./build/Debug/tests/simulated_workers_push localhost:50051 5 1 20
```

示例：持续推送，手动按 Ctrl+C 停止：

```bash
./build/Debug/tests/simulated_workers_push localhost:50051 5 2 0
```

manager 端应能看到类似日志：

```text
Received monitor data from host: worker-01
Received monitor data from host: worker-02
Received monitor data from host: worker-03
Received monitor data from host: worker-04
Received monitor data from host: worker-05
```
