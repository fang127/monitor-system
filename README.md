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

## 🔧 环境要求

- **操作系统**: Linux (Ubuntu 20.04+, CentOS 8+ 推荐)
- **编译器**: GCC 9+ 或 Clang 10+ (支持 C++17)
- **CMake**: 3.10+
- **内核版本**: 5.4+ (eBPF 功能需要)
- **MySQL**: 8.0+ (必须)

## 📚 系统指标说明

### 1. CPU 负载

cat /proc/loadavg 获取系统负载信息，包括1分钟、5分钟和15分钟的平均负载。

### 2. CPU 软中断

cat /proc/softirqs 获取各类软中断的统计信息，如网络中断、定时器中断等。

### 3. CPU 状态

cat /proc/stat 获取CPU的各种状态信息，如用户态、系统态、空闲态等的时间统计。

### 4. 磁盘状态

cat /proc/diskstats 获取磁盘的读写操作统计信息。

### 5. 内存状态

cat /proc/meminfo 获取内存的使用情况，包括总内存、可用内存、缓存等。

### 6. 网络状态

cat /proc/net/dev 获取网络接口的流量统计信息。
