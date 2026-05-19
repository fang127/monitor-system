/*!
 * @file          CpuLoadMonitor.h
 * @author        harry
 * @date          2026-02-09
 */
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "MonitorInter.h"
#include "monitor_info.pb.h"

namespace monitor {
/**
 * @brief         CPU 使用率监控器，采集各 CPU 核心的用户态、系统态、空闲、I/O 等待和中断占比
 *
 */
class CpuStatMonitor : public MonitorInter {
    /**
     * @brief         CPU 时间片累计值缓存，用于计算两次采样间的使用率
     *
     */
    struct CpuStat {
        std::string cpu_name;
        uint64_t user;
        uint64_t system;
        uint64_t idle;
        uint64_t nice;
        uint64_t io_wait;
        uint64_t irq;
        uint64_t soft_irq;
        uint64_t steal;
        uint64_t guest;
        uint64_t guest_nice;
    };

public:
    CpuStatMonitor() {}

    /**
     * @brief         采集一次 CPU 使用率数据并写入 MonitorInfo
     *
     * @param         monitorInfo 监控数据输出对象
     */
    void updateOnce(monitor::proto::MonitorInfo *monitorInfo) override;

    /**
     * @brief         停止 CPU 使用率监控器，当前实现无额外资源需要释放
     *
     */
    void stop() override {}

private:
    std::unordered_map<std::string, struct CpuStat> cpuStatMap_; // 上一次 CPU 采样缓存
};

} // namespace monitor
