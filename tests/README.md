# Simulated worker push test

这个目录提供一个轻量级 gRPC 推送测试程序，用来模拟多台 worker 主机向 manager 上报 `MonitorInfo` 数据。

默认行为：

- 模拟 5 台主机：`worker-01` 到 `worker-05`
- 每台主机使用不同 IP：`10.10.0.101` 到 `10.10.0.105`
- 默认每 2 秒推送一次
- 默认每台 worker 推送 5 轮后退出

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
