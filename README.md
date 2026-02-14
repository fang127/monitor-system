<!--
 * @Author: harry
 * @Date: 2026-02-04 21:23:05
 * @Version: 1.0
 * @LastEditors: harry
 * @LastEditTime: 2026-02-05 01:20:03
 * @Description: 
 * @FilePath: /monitor-system/README.md
-->
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
|    host_data_ map                             +-------------------+
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
├── proto/                     # Protobuf/gRPC 定义
└── CMakeLists.txt             # 构建配置
```

### mysql 数据库设计

```
monitor_db
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
| 离线阈值 | 60 秒 | 超过此时间无数据视为离线 |
| gRPC 端口 | 50051 | Manager 监听端口 |

## 🛠️ 技术栈

- **语言**: C++
- **RPC 框架**: gRPC + Protocol Buffers
- **数据采集**: Linux 内核模块 + eBPF + procfs
- **数据库**: MySQL
- **构建系统**: CMake + conan + Makefile + python 脚本

## 🚀 快速开始

### 1. 安装并启动 MySQL

```bash
sudo systemctl start mysql
sudo systemctl enable mysql
```

### 2. 创建数据库和用户

```bash
sudo mysql -u root -p
```

```sql
-- 创建数据库
CREATE DATABASE monitor_db;

-- 创建用户并授权
CREATE USER 'yourUser'@'localhost' IDENTIFIED BY 'yourPassword';
GRANT ALL PRIVILEGES ON monitor_db.* TO 'yourUser'@'localhost';
FLUSH PRIVILEGES;
EXIT;
```

### 3. 导入表结构

```bash
mysql -u monitor -pyourPassword monitor_db < manager/sql/init_server_performance.sql
```

### 4. 修改代码中的数据库配置

在以下两个文件中修改数据库连接信息：

**文件**: `manager/src/main.cpp` 和 `manager/src/host_manager.cpp`

```cpp
// 修改为你的 MySQL 配置
const char* host = "localhost";
const char* user = "monitor";        // 你的用户名
const char* password = "monitor123"; // 你的密码
const char* database = "monitor_db";
```

### 5. 运行构建脚本

```bash
python3 ./build_debug.py
```

### 6. 启动 Manager 服务器

```bash
./build/manager/manager
```

输出：
```
Starting Monitor Client (Manager Mode)...
Monitor Client listening on 0.0.0.0:50051
Query service available for performance data queries
```

### 7. 加载内核模块

```bash
sudo insmod worker/src/kmod/CpuStatCollector.ko
sudo insmod worker/src/kmod/SoftirqCollector.ko

# 验证加载
ls /dev/CpuStatCollector /dev/SoftirqCollector
```

### 8. 启动 Worker（被监控机器）

```bash
# 需要 sudo 权限以加载 eBPF 程序
sudo ./build/worker/worker <manager_ip>:50051

# 示例
sudo ./build/worker/worker 192.168.1.100:50051
```

### 9. 验证运行

Manager 端显示：
```
Received monitor data from: server1
Processed data from server1_192.168.1.101, score: 75.32
```

### 10. 停止服务和卸载内核模块

```bash
# 停止 Worker 和 Manager（Ctrl+C）

# 卸载内核模块
sudo rmmod SoftirqCollector
sudo rmmod CpuStatCollector

# 验证卸载
lsmod | grep -E "CpuStat|Softirq"
```

## 📄 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件

## 📄 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件