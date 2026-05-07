# Worker

`worker` 部署在被监控主机上，负责周期性采集本机 CPU、负载、内存、网络、磁盘、软中断和主机标识信息，然后通过 gRPC 主动推送给 manager。当前采用 Push 模式，默认连接 `localhost:50051`，默认每 10 秒推送一次。

## 角色定位

worker 是采集端，不负责存储和查询：

```text
+-------------------------------------------------------+
| worker                                                |
|                                                       |
|  main.cc                                              |
|    |                                                  |
|    v                                                  |
|  MonitorPusher                                        |
|    |  每 intervalSeconds 秒执行一次                   |
|    v                                                  |
|  MetricCollector                                      |
|    |                                                  |
|    +--> CpuLoadMonitor                                |
|    +--> CpuStatMonitor                                |
|    +--> CpuSoftIrqMonitor                             |
|    +--> MemMonitor                                    |
|    +--> NetEbpfMonitor 或 NetMonitor                  |
|    +--> DiskMonitor                                   |
|    +--> HostInfoMonitor                               |
|                                                       |
|  MonitorInfo protobuf                                 |
|    |                                                  |
|    v                                                  |
|  gRPC GrpcManager.SetMonitorInfo(MonitorInfo)         |
+-------------------------------------------------------+
                         |
                         v
                      manager
```

核心原则：

- worker 每轮组装一整个 `MonitorInfo` 包。
- worker 主动 push，manager 不需要轮询每台机器。
- 采集器之间通过统一接口 `MonitorInter::updateOnce()` 扩展。
- 网络采集优先使用 eBPF；关闭 eBPF 时回退到 `/proc/net/dev`。

## 源码结构

```text
worker/
├── CMakeLists.txt
├── include/
│   ├── monitor/
│   │   ├── MonitorInter.h          # 所有采集器的统一接口
│   │   ├── MetricCollector.h       # 多采集器组合
│   │   ├── CpuLoadMonitor.h        # load average
│   │   ├── CpuStatMonitor.h        # CPU 使用率
│   │   ├── CpuSoftirqMonitor.h     # 软中断
│   │   ├── MemMonitor.h            # 内存
│   │   ├── NetMonitor.h            # /proc/net/dev 网络采集
│   │   ├── NetEbpfMonitor.h        # eBPF TC 网络采集
│   │   ├── DiskMonitor.h           # 磁盘 I/O
│   │   └── HostInfoMonitor.h       # 主机名和主 IP
│   ├── rpc/
│   │   └── MonitorPusher.h         # 周期推送逻辑
│   └── utils/
│       └── ReadFile.h
├── src/
│   ├── main.cc
│   ├── monitor/
│   ├── rpc/
│   ├── kmod/                       # CpuStatCollector/SoftirqCollector 内核模块
│   └── ebpf/                       # net_stats.bpf.c 和 skeleton 生成
└── test/
```

## 启动流程

`src/main.cc` 只做三件事：

1. 解析 manager 地址，默认 `localhost:50051`。
2. 解析推送间隔，默认 `10` 秒；传入非正数时回退默认值。
3. 创建 `MonitorPusher` 并调用 `start()`，主线程随后常驻。

命令格式：

```bash
./build/Debug/worker/worker [manager_address] [interval_seconds]
```

示例：

```bash
./build/Debug/worker/worker 192.168.1.10:50051 10
```

## 推送机制

`MonitorPusher` 是 worker 的运行核心：

1. 构造时创建 gRPC channel 和 `GrpcManager::Stub`。
2. 构造 `MetricCollector`。
3. `start()` 启动后台线程执行 `pushLoop()`。
4. `pushLoop()` 每轮调用 `pushOnce()`。
5. `pushOnce()` 创建空的 `MonitorInfo`。
6. 调用 `collector_->collectAll(&info)` 依次采集所有指标。
7. 打印本轮采集结果。
8. 调用 `stub_->SetMonitorInfo(&context, info, &response)` 推送给 manager。
9. 成功则打印 success，失败打印 gRPC 错误信息。

推送间隔内每秒检查一次 `running_`，因此停止时不需要等待完整间隔结束。

## 数据模型

worker 上报的数据定义在 `proto/monitor_info.proto`：

```proto
message MonitorInfo {
  string name = 1;
  HostInfo host_info = 2;
  repeated SoftIrq soft_irq = 4;
  CpuLoad cpu_load = 5;
  repeated CpuStat cpu_stat = 6;
  MemInfo mem_info = 7;
  repeated NetInfo net_info = 8;
  repeated DiskInfo disk_info = 9;
}
```

主要字段：

| 字段 | 来源 | 说明 |
|---|---|---|
| `name` | `gethostname()` | 兼容旧版本的主机名 |
| `host_info` | `HostInfoMonitor` | 主机名和主 IPv4 地址 |
| `cpu_load` | `CpuLoadMonitor` | 1/3/15 分钟 load average |
| `cpu_stat` | `CpuStatMonitor` | 每 CPU 或总 CPU 使用率拆分 |
| `soft_irq` | `CpuSoftIrqMonitor` | 每 CPU 软中断速率或计数 |
| `mem_info` | `MemMonitor` | 内存容量、可用量、cache、dirty 等 |
| `net_info` | `NetEbpfMonitor` 或 `NetMonitor` | 网卡吞吐、包速率、错误和丢弃 |
| `disk_info` | `DiskMonitor` | 磁盘 I/O 计数、速率、IOPS、延迟、利用率 |

## 采集器架构

所有采集器都继承 `MonitorInter`：

```cpp
class MonitorInter {
public:
    virtual void updateOnce(monitor::proto::MonitorInfo *info) = 0;
    virtual void stop() = 0;
};
```

`MetricCollector` 构造时按顺序注册：

1. `CpuLoadMonitor`
2. `CpuStatMonitor`
3. `CpuSoftIrqMonitor`
4. `MemMonitor`
5. `NetEbpfMonitor` 或 `NetMonitor`
6. `DiskMonitor`
7. `HostInfoMonitor`

每轮采集时，`MetricCollector::collectAll()` 先设置 `MonitorInfo.name`，再依次调用每个采集器的 `updateOnce()`。采集器直接把结果写入同一个 protobuf 对象。

## 指标采集来源

### CPU Load

`CpuLoadMonitor` 优先尝试读取 `/dev/CpuStatCollector` 并通过 `mmap()` 获得内核模块输出的 `cpu_load`。如果设备不存在或映射失败，则回退读取：

```text
/proc/loadavg
```

### CPU 使用率

`CpuStatMonitor` 读取：

```text
/dev/CpuStatCollector
```

通过 `mmap()` 读取最多 128 个 `cpu_stat` 结构体。它会缓存上一轮 CPU jiffies，并在下一轮计算：

```text
cpu_percent = busy_delta / total_delta * 100
```

同时计算 user、system、nice、idle、iowait、irq、softirq 等占比。首次采集没有上一轮样本时，只缓存数据，不一定能生成完整百分比。

### 软中断

`CpuSoftIrqMonitor` 读取：

```text
/dev/SoftirqCollector
```

通过 `mmap()` 读取最多 256 个 CPU 的软中断计数。第二次及之后采样会按时间差换算为每秒速率：

```text
(current_counter - previous_counter) / seconds
```

设备不存在时会静默跳过，通常表示内核模块未加载。

### 内存

`MemMonitor` 读取：

```text
/proc/meminfo
```

采集 total、free、available、buffers、cached、swap cached、active、inactive、dirty、writeback、anon pages、mapped、slab reclaim 等字段，并计算：

```text
used_percent = (MemTotal - MemAvailable) / MemTotal * 100
```

当前实现把 `/proc/meminfo` 中的 KB 数值除以 `1000 * 1000` 后写入 protobuf，实际更接近 GB 口径；proto 注释里写的是 MB，使用时要注意这一点。

### 网络

网络有两种实现。

启用 eBPF 时，CMake 会定义 `ENABLE_EBPF=1`，worker 使用 `NetEbpfMonitor`：

- 加载 `net_stats.bpf.c` 生成的 BPF skeleton。
- 遍历 `/sys/class/net` 获取网卡 ifindex。
- 跳过 loopback。
- 为网卡创建 `clsact` qdisc。
- 将 ingress/egress TC 程序 attach 到网卡。
- 从 BPF map 中读取累计收发 bytes/packets。
- 和上一轮缓存做差，换算为 bytes/sec 与 packets/sec。

关闭 eBPF 时，worker 使用 `NetMonitor`：

- 读取 `/proc/net/dev`。
- 跳过 `lo`。
- 解析收发 bytes、packets、errors、drops。
- 和上一轮缓存做差，计算速率。

注意：`NetMonitor` 代码中速率计算除以了 `1024.0`，注释显示为 KB/s；而 proto 注释写 B/s。eBPF 路径输出是 B/s。后续如果要统一单位，建议优先在这里修正。

### 磁盘

`DiskMonitor` 读取：

```text
/proc/diskstats
```

跳过 `loop*` 和 `ram*` 设备，保留真实磁盘或块设备。它会缓存上一轮计数器和时间戳，并计算：

- `read_bytes_per_sec`
- `write_bytes_per_sec`
- `read_iops`
- `write_iops`
- `avg_read_latency_ms`
- `avg_write_latency_ms`
- `util_percent`

磁盘扇区按 512 bytes 换算。

### 主机标识

`HostInfoMonitor` 首次采集时缓存：

- `gethostname()` 返回的主机名。
- 第一张非 loopback、非 docker/veth/br-/virbr 的 IPv4 地址。

后续采集复用缓存，避免每轮重复遍历网卡。

manager 会优先使用 `hostname_ip` 作为 server ID，所以同名容器或同名虚拟机也能通过 IP 区分。

## 构建

worker 依赖 gRPC。默认 `ENABLE_EBPF=ON` 时，还依赖 libbpf、elfutils、ZLIB、clang、bpftool 和系统 eBPF 头文件。

完整 Debug 构建：

```bash
conan install . --build=missing --settings=build_type=Debug
cmake --preset conan-debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build --preset conan-debug --target worker
```

如果当前机器没有 eBPF 构建环境，可以关闭 eBPF，使用 `/proc/net/dev` 网络采集：

```bash
cmake -S . -B build/Debug \
  -DCMAKE_TOOLCHAIN_FILE=build/Debug/generators/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_EBPF=OFF
cmake --build build/Debug --target worker
```

也可以使用根目录脚本构建整个 Debug 工程：

```bash
python3 build_debug.py
```

## eBPF 构建和运行要求

eBPF 构建由 `worker/src/ebpf/Makefile` 完成。CMake 构建 worker 时会：

1. 调用 `make -C worker/src/ebpf`。
2. 编译 `net_stats.bpf.c` 为 BPF object。
3. 使用 `bpftool gen skeleton` 生成 `net_stats.skel.h`。
4. 将 skeleton 复制到 worker 构建目录。

默认变量：

| 变量 | 默认值 | 说明 |
|---|---|---|
| `CLANG` | `/opt/LLVM-22.1.1-Linux-X64/bin/clang` | 编译 BPF 程序的 clang |
| `BPFTOOL` | `bpftool` | 生成 skeleton |
| `VMLINUX_H` | `/usr/include/vmlinux.h` | BTF 类型头文件 |
| `LIBBPF_INCLUDE` | `/usr/include` | libbpf include 路径 |

运行 eBPF 网络采集通常需要 root 或足够的 Linux capabilities，因为它会加载 BPF 程序并 attach TC hook：

```bash
sudo ./build/Debug/worker/worker 127.0.0.1:50051 10
```

如果 eBPF 加载失败，当前 `NetEbpfMonitor` 会打印 fallback 提示，但 `updateOnce()` 在 `loaded_=false` 时直接返回；因此真正想使用 `/proc/net/dev` 回退路径，建议在构建时显式设置：

```bash
-DENABLE_EBPF=OFF
```

## 内核模块

CPU stat 和 softirq 的高效采集依赖 `worker/src/kmod` 中的两个内核模块：

- `CpuStatCollector.ko`，创建设备 `/dev/CpuStatCollector`。
- `SoftirqCollector.ko`，创建设备 `/dev/SoftirqCollector`。

构建：

```bash
cmake --build --preset conan-debug --target kernel_modules
```

或者直接：

```bash
make -C worker/src/kmod
```

加载：

```bash
make -C worker/src/kmod install
```

卸载：

```bash
make -C worker/src/kmod uninstall
```

查看模块信息和日志：

```bash
make -C worker/src/kmod info
make -C worker/src/kmod log
```

如果模块没有加载，CPU stat 和 softirq 相关采集会缺失或只获得部分 fallback 数据；worker 进程本身仍可运行。

## 运行

启动 manager 后，在被监控机器运行：

```bash
./build/Debug/worker/worker <manager_host>:50051 10
```

示例：

```bash
./build/Debug/worker/worker 192.168.56.10:50051 10
```

如果启用了 eBPF，推荐使用：

```bash
sudo ./build/Debug/worker/worker 192.168.56.10:50051 10
```

运行后控制台会打印每轮采集的指标，并显示：

```text
>>> Pushed monitor data to <manager> successfully <<<
```

## 本地验证

如果只想验证 manager 接收链路，不想依赖真实 worker 环境，可以使用根目录 `tests/simulated_workers_push.cc` 编译出的模拟工具：

```bash
./build/Debug/tests/simulated_workers_push localhost:50051 5 2 20
```

如果要验证真实 worker：

1. 启动 manager。
2. 确认 manager 监听地址对 worker 可达。
3. 可选加载内核模块。
4. 根据环境选择 `ENABLE_EBPF=ON` 或 `OFF` 构建 worker。
5. 运行 worker。
6. 在 manager 控制台确认 `worker_requests` 增长。
7. 查询 MySQL `server_performance` 是否出现当前主机记录。

## 常见问题

### worker 显示 Push failed

检查：

- manager 是否已启动。
- worker 使用的地址是否正确，例如容器和宿主机之间不能总用 `localhost`。
- manager 防火墙是否开放 50051。
- manager 是否返回 `Missing hostname`，这通常表示 `MonitorInfo.name` 和 `host_info.hostname` 都为空。

### 没有 CPU 使用率

`CpuStatMonitor` 依赖 `/dev/CpuStatCollector`。检查：

```bash
ls -l /dev/CpuStatCollector
lsmod | grep CpuStatCollector
make -C worker/src/kmod log
```

首次采样还没有上一轮 jiffies，可能要等第二轮推送才会出现百分比。

### 没有软中断数据

检查 `/dev/SoftirqCollector` 是否存在：

```bash
ls -l /dev/SoftirqCollector
lsmod | grep SoftirqCollector
```

设备不存在时采集器会静默跳过。

### eBPF 网络采集失败

常见原因：

- 没有 root 权限或缺少 BPF/NET_ADMIN capability。
- 缺少 `bpftool`。
- `VMLINUX_H` 路径不正确。
- 内核未开启 BTF。
- 网卡不支持当前 TC attach。

开发时可以先用：

```bash
-DENABLE_EBPF=OFF
```

确认其余链路正常，再单独排查 eBPF。

### manager 里同一台机器出现多个 server_name

manager 的 ID 是 `hostname_ip`。如果主机 IP 变化、容器网络变化，或者 `HostInfoMonitor` 每次选到不同网卡，就会生成不同 server ID。可以检查 worker 日志中的 host 和 IP，必要时固定网络配置或改造 `HostInfoMonitor` 的网卡选择策略。

## 扩展新指标

新增一个采集器通常需要：

1. 在 `proto/monitor_info.proto` 或独立 proto 中增加消息字段。
2. 重新生成 protobuf/gRPC 代码。
3. 新建 `include/monitor/XxxMonitor.h` 和 `src/monitor/XxxMonitor.cc`。
4. 继承 `MonitorInter`，实现 `updateOnce()` 和 `stop()`。
5. 在 `MetricCollector` 构造函数中加入 `monitors_.push_back(std::make_unique<XxxMonitor>())`。
6. 在 `worker/CMakeLists.txt` 中加入源文件。
7. 如果 manager 需要持久化或查询该指标，同步更新 manager 写库逻辑、SQL 表结构和查询接口。

采集器应尽量只做本地读取和 protobuf 填充，不直接承担网络发送、存储或查询逻辑，这样 worker 的 Push 架构会保持清晰。
