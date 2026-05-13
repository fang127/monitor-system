# monitor-system

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![gRPC](https://img.shields.io/badge/gRPC-1.50+-green.svg)](https://grpc.io/)
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)](https://www.linux.org/)

分布式服务器性能监控系统，采用 Push 模式架构，支持多服务器性能数据采集、存储和查询。基于内核模块和 eBPF 技术实现高效的系统指标采集。

## ✨ 特性

- 🚀 **高效采集** - 基于内核模块和 eBPF 的低开销数据采集
- 📊 **全面监控** - CPU、内存、磁盘、网络、软中断等全方位指标
- 🔄 **Push 模式** - Worker 主动推送，降低 Manager 负载
- 📈 **健康评分** - 多维度加权评分算法，快速评估服务器状态
- 🔍 **丰富查询** - 9 个 gRPC 查询接口，支持历史数据、趋势分析、异常检测
- 🌐 **HTTP API 网关** - Go/Gin API Gateway 将 Manager 的 gRPC 查询能力转换为 JSON HTTP 接口
- 💾 **数据持久化** - MySQL 存储历史数据

## 📐 系统架构

```
┌─────────────────┐     gRPC Push      ┌─────────────────┐
│     Worker      │ ─────────────────► │     Manager     │
│  (被监控服务器)  │   MonitorInfo      │   (管理服务器)   │
│                 │   每10秒推送        │                 │
│  - CPU 采集     │                    │  - 数据接收     │
│  - 内存采集     │                    │  - 评分计算     │
│  - 磁盘采集     │                    │  - MySQL 存储   │
│  - 网络采集     │                    │  - 查询服务     │
└─────────────────┘                    └─────────────────┘
        │                                      │
        │ 内核模块/eBPF                         │ QueryService
        ▼                                      ▼
   /dev/cpu_stat_monitor                  9个查询接口
   /dev/cpu_softirq_monitor
```

### manager

```
Worker(多台)
   |
   |  gRPC: SetMonitorInfo(MonitorInfo)
   v
+-----------------------------------------------------------+
|                         Manager进程                        |
|                                                           |
|  +--------------------+       callback       +-----------+ |
|  | GrpcServerImpl     |--------------------->| HostManager| |
|  | (GrpcManager服务)  |                      | 评分/写库  | |
|  +--------------------+                      +-----------+ |
|           |                                          |     |
|           | 保存每台主机最新 MonitorInfo/时间戳        | 写入 |
|           v                                          v     |
|    hostDatas_ map                             +-------------------+
|                                              |      MySQL         |
|                                              | server_performance |
|                                              | server_*_detail    |
|                                              +-------------------+
|                                                           |
|  +--------------------+     调用SQL封装     +-------------+ |
|  | QueryServiceImpl   |------------------->| QueryManager | |
|  | (QueryService服务) |  组装proto响应      | 查询/聚合   | |
|  +--------------------+                    +-------------+ |
|                                                           |
+-----------------------------------------------------------+
             ^
             | gRPC: Query*(...)  (客户端查询)
             |
         Query Client
```

### api_gateway

`api_gateway` 是新增的 Go HTTP 服务，对外提供 REST 风格 JSON API，对内连接 Manager 的 `QueryService` gRPC 服务。默认连接地址为 `127.0.0.1:50051`，可通过 `MANAGER_GRPC_ADDR` 覆盖。

```
HTTP Client
    |
    |  JSON/HTTP
    v
+-------------------------------+
|          api_gateway          |
|  Go + Gin HTTP Server         |
|                               |
|  GET /health                  |
|  GET /api/version             |
|  GET /api/servers/latest      |
|  GET /api/servers/:server/... |
+---------------+---------------+
                |
                | gRPC QueryService
                v
           Manager(C++)
```

### worker

```
+----------------------+
|   Worker进程(main)   |
+----------+-----------+
           |
           | start()
           v
+------------------------------+
| MonitorPusher                |
| - intervalSeconds           |
| - grpc stub(GrpcManager)     |
| - MetricCollector collector  |
+---------------+--------------+
                |
                | pushLoop(): 每隔N秒
                v
         +--------------+
         | pushOnce()   |
         +------+-------+
                |
                | collector.collectAll(&MonitorInfo)
                v
+-------------------------------------+
| MetricCollector                      |
| 依次调用多个 Monitor(采集器)         |
+----+----------+-----------+----------+
     |          |           | 
     v          v           v
 Cpu*Monitor  MemMonitor  DiskMonitor  Net(Proc/eBPF)  HostInfoMonitor ...
 (updateOnce) (updateOnce)(updateOnce) (updateOnce)    (updateOnce)
     |
     | 将数据写入同一个 MonitorInfo
     v
 MonitorInfo(一整包)
     |
     | gRPC: SetMonitorInfo(MonitorInfo)
     v
 Manager
```

## 📁 项目结构

```
monitor_system/
├── configs/                   # 运行时配置
│   ├── app.env                # 统一运行配置入口
│   ├── README.md              # 配置说明
│   └── manager.env            # Manager/MySQL 环境变量
├── deploy/                    # 部署配置
│   └── docker-compose.yml     # 除 manager/worker 外的一键启动服务
├── api_gateway/               # Go HTTP API 网关
│   ├── cmd/server             # Gin HTTP Server 入口
│   ├── internal/config        # 配置加载
│   ├── internal/grpcclient    # QueryService gRPC client
│   ├── internal/handler       # HTTP Handler
│   ├── internal/logger        # 日志初始化
│   ├── internal/response      # JSON 响应封装
│   ├── Makefile               # make run / make proto
│   └── README.md              # API 网关说明
├── worker/                    # 工作者服务器（部署在被监控机器）
│   ├── include/               # 头文件
│   │   ├── monitor/           # 监控器接口
│   │   ├── rpc/               # RPC 相关
│   │   └── utils/             # 工具类
│   ├── src/
│   │   ├── monitor/           # 各类监控器实现
│   │   ├── rpc/               # 数据推送
│   │   ├── kmod/              # 内核模块源码
│   │   └── ebpf/              # eBPF 程序源码
│   └── scripts/               # 辅助脚本
│
├── manager/                   # 管理者服务器（部署在管理端）
│   ├── include/               # 头文件
│   ├── src/                   # 源码实现
│   └── sql/                   # 数据库初始化脚本
│
├── tests/                     # 本地集成测试工具
│   ├── simulated_workers_push.cc  # 模拟多台 worker 推送数据
│   └── README.md              # 模拟测试说明
│
├── proto/                     # Protobuf/gRPC 定义
└── CMakeLists.txt             # 构建配置
```

### mysql 数据库设计

```
monitor-system
  |
  +-- server_performance       (汇总/主表：每次上报的一行“总览 + score + 变化率”)
  +-- server_net_detail        (网络明细：每网卡每次上报一行)
  +-- server_softirq_detail    (软中断明细：每CPU每次上报一行)
  +-- server_mem_detail        (内存明细：每次上报一行，字段更全)
  +-- server_disk_detail       (磁盘明细：每磁盘每次上报一行)
```

## 🔧 环境要求

- **操作系统**: Linux (Ubuntu 20.04+, CentOS 8+ 推荐)
- **编译器**: GCC 9+ 或 Clang 10+ (支持 C++17)
- **CMake**: 3.10+
- **内核版本**: 5.4+ (eBPF 功能需要)
- **MySQL**: 8.0+ (必须)
- **Conan**: 1.40+ (依赖管理)
- **Python**: 3.6+ (构建脚本)
- **Go**: 1.22+ (api_gateway)
- **protoc + protoc-gen-go + protoc-gen-go-grpc**: 生成 Go gRPC client（`api_gateway/make proto` 会自动安装 Go 插件到本地 `.bin/`）

## 🌐 API Gateway

`api_gateway` 提供面向前端或运维脚本的 HTTP JSON API，避免客户端直接依赖 Manager 的 gRPC 协议。

### 启动

```bash
cd api_gateway
make run
```

### 配置

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `API_GATEWAY_PORT` | `8080` | HTTP 服务端口 |
| `API_GATEWAY_VERSION` | `v0.1.0` | API 网关版本号 |
| `GIN_MODE` | `debug` | Gin 运行模式 |
| `MANAGER_GRPC_ADDR` | `127.0.0.1:50051` | Manager gRPC 地址 |
| `MANAGER_GRPC_TIMEOUT` | `5s` | Manager 调用超时时间 |

### 接口

| 方法 | 路径 | 说明 |
|------|------|------|
| `GET` | `/health` | 健康检查 |
| `GET` | `/api/version` | 版本信息 |
| `GET` | `/api/servers/latest` | 所有服务器最新评分与集群统计 |
| `GET` | `/api/servers/score-rank` | 服务器评分排序 |
| `GET` | `/api/servers/:server/performance` | 指定服务器历史性能数据 |
| `GET` | `/api/servers/:server/trend` | 指定服务器趋势数据 |
| `GET` | `/api/servers/:server/anomalies` | 指定服务器异常数据 |
| `GET` | `/api/servers/:server/net-detail` | 指定服务器网络明细 |
| `GET` | `/api/servers/:server/disk-detail` | 指定服务器磁盘明细 |
| `GET` | `/api/servers/:server/mem-detail` | 指定服务器内存明细 |
| `GET` | `/api/servers/:server/softirq-detail` | 指定服务器软中断明细 |

### 生成 Go gRPC client

```bash
cd api_gateway
make proto
```

生成文件输出到 `api_gateway/internal/pb/queryapi/`。

## 📚 系统指标说明

### Worker 采集项

| 监控项 | 数据来源 | 采集内容 |
|--------|----------|----------|
| CPU 状态 | 内核模块 / procfs | 各核心使用率、用户态/内核态/空闲占比 |
| CPU 负载 | `/proc/loadavg` | 1/3/15 分钟平均负载 |
| 软中断 | 内核模块 / procfs | 各 CPU 核心软中断统计 |
| 内存 | `/proc/meminfo` | 总量、可用、缓存、Swap 等 |
| 磁盘 | `/proc/diskstats` | 读写速率、IOPS、延迟、利用率 |
| 网络 | eBPF / procfs | 收发速率、包数、错误/丢包统计 |

### 指标单位约定

| 指标 | 字段/表 | 单位 | 说明 |
|------|---------|------|------|
| 网络吞吐 | `NetInfo.send_rate`、`NetInfo.rcv_rate`、`server_performance.send_rate/rcv_rate`、`server_net_detail.*_bytes_rate` | B/s | 全链路按字节每秒保存；日志直接显示 B/s |
| 网络包速率 | `send_packets_rate`、`rcv_packets_rate`、`*_packets_rate` | packets/s | 每秒包数 |
| 内存容量 | `MemInfo.total/free/avail/...`、`server_mem_detail`、`server_performance.total/free/avail` | MB | 来自 `/proc/meminfo` 后统一换算为 MB |
| 内存使用率 | `used_percent`、`mem_used_percent` | % | 百分比数值，范围通常为 0-100 |
| 磁盘吞吐 | `DiskInfo.read_bytes_per_sec/write_bytes_per_sec`、`server_disk_detail.*_bytes_per_sec` | B/s | MySQL 保存 B/s；Manager 控制台为了可读性显示为 KB/s |
| 磁盘 IOPS | `read_iops`、`write_iops` | ops/s | 每秒 IO 次数 |
| 磁盘延迟 | `avg_read_latency_ms`、`avg_write_latency_ms` | ms | 平均读写延迟 |
| 磁盘利用率 | `util_percent`、`disk_util_percent` | % | 百分比数值，范围通常为 0-100 |
| 变化率字段 | `*_rate` 后缀 | ratio | 保存相对变化率；日志展示时乘以 100 显示为百分比 |

### Manager 查询接口

| 接口 | 功能 | 用途 |
|------|------|------|
| `QueryPerformance` | 时间段性能数据 | 历史数据分析 |
| `QueryTrend` | 变化率趋势 | 性能趋势预测 |
| `QueryAnomaly` | 异常数据检测 | 告警和问题定位 |
| `QueryScoreRank` | 评分排序 | 服务器选择/调度 |
| `QueryLatestScore` | 最新评分 | 实时状态概览 |
| `QueryNetDetail` | 网络详细数据 | 网络问题排查 |
| `QueryDiskDetail` | 磁盘详细数据 | IO 性能分析 |
| `QueryMemDetail` | 内存详细数据 | 内存使用分析 |
| `QuerySoftIrqDetail` | 软中断详细数据 | 中断负载分析 |

### 健康评分算法

```
Score = CPU_Score × 35% + Mem_Score × 30% + Load_Score × 15% 
      + Disk_Score × 15% + Net_Score × 5%

其中：
- CPU_Score = 1 - cpu_percent / 100
- Mem_Score = 1 - mem_used_percent / 100
- Load_Score = 1 - load_avg_1 / (cpu_cores × 1.5)
- Disk_Score = 1 - disk_util_percent / 100
- Net_Score = 1 - bandwidth_usage / max_bandwidth
```

### 关键参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| 推送间隔 | 10 秒 | Worker 向 Manager 推送数据的间隔 |
| 模拟测试推送间隔 | 2 秒 | `simulated_workers_push` 默认推送间隔 |
| 模拟测试轮次 | 5 轮 | `simulated_workers_push` 默认每台 worker 推送 5 轮；设置为 0 表示持续推送 |
| 离线阈值 | 60 秒 | 超过此时间无数据视为离线 |
| gRPC 端口 | 50051 | Manager 监听端口 |

## 🛠️ 技术栈

- **语言**: C++
- **RPC 框架**: gRPC + Protocol Buffers
- **数据采集**: Linux 内核模块 + eBPF + procfs
- **数据库**: MySQL
- **构建系统**: CMake + conan + Makefile + python 脚本

## 🚀 快速开始

### 1. 启动容器化模块

```bash
docker compose --env-file configs/app.env -f deploy/docker-compose.yml up -d
```

Compose 会启动 MySQL、Redis、Milvus、Attu、agent_service、api_gateway 和 web。
`manager` 与 `worker` 仍作为宿主机进程启动。Compose 会把初始化脚本
`manager/sql table/init_server_performance.sql` 挂载到
`/docker-entrypoint-initdb.d/`，首次创建 MySQL 数据目录时自动建库建表。

### 2. 加载统一配置

```bash
set -a
source configs/app.env
set +a
```

默认配置：

```env
MYSQL_HOST=127.0.0.1
MYSQL_PORT=3306
MYSQL_USER=root
MYSQL_PASSWORD=123456
MYSQL_DATABASE=monitor-system
```

### 3. 运行构建脚本

```bash
python3 ./build_debug.py
```

### 4. 启动 Manager 服务器

```bash
./build/manager/manager
```

输出：
```
Starting Monitor Client (Manager Mode)...
Monitor Client listening on 0.0.0.0:50051
Query service available for performance data queries
```

### 5. 加载内核模块

```bash
sudo insmod worker/src/kmod/CpuStatCollector.ko
sudo insmod worker/src/kmod/SoftirqCollector.ko

# 验证加载
ls /dev/CpuStatCollector /dev/SoftirqCollector
```

### 6. 启动 Worker（被监控机器）

```bash
# 需要 sudo 权限以加载 eBPF 程序
sudo ./build/worker/worker <manager_ip>:50051

# 示例
sudo ./build/worker/worker 192.168.1.100:50051
```

### 7. 验证运行

Manager 端显示：
```
Received monitor data from: server1
Processed data from server1_192.168.1.101, score: 75.32
```

### 8. 使用模拟 Worker 测试

如果只想验证 Manager 接收、评分和 MySQL 写库，可以使用 `tests/simulated_workers_push`，不需要启动真实 worker、内核模块或 eBPF。测试程序默认模拟 5 台 worker：`worker-01` 到 `worker-05`，IP 为 `10.10.0.101` 到 `10.10.0.105`。

构建测试工具：

```bash
conan install . --build=missing --settings=build_type=Debug
cmake -S . -B build/Debug \
  -DCMAKE_TOOLCHAIN_FILE=build/Debug/generators/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_MANAGER=OFF \
  -DENABLE_EBPF=OFF \
  -DBUILD_TESTS=ON
cmake --build build/Debug --target simulated_workers_push
```

启动模拟推送：

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

Manager 并发接收多台 worker 推送时，`std::cout`/`std::cerr` 日志可能交叉显示；这只影响控制台可读性，不代表数据接收或 MySQL 写库失败。

### 9. 停止服务和卸载内核模块

```bash
# 停止 Worker 和 Manager（Ctrl+C）

# 卸载内核模块
sudo rmmod SoftirqCollector
sudo rmmod CpuStatCollector

# 验证卸载
lsmod | grep -E "CpuStat|Softirq"

# 停止容器化模块
docker compose --env-file configs/app.env -f deploy/docker-compose.yml down
```

## MySQL 字段注意事项

`server_disk_detail` 使用 `read_ops` 和 `write_ops` 保存磁盘读写累计次数，避免使用 `reads`/`writes` 这类容易与 SQL 关键字或语法产生歧义的列名。

网络明细、内存明细和磁盘明细已经按上面的单位约定写入 MySQL：网络吞吐为 B/s，内存容量为 MB，磁盘吞吐为 B/s。

## 📄 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件
