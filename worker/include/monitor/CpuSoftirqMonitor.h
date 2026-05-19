/*!
 * @file          CpuLoadMonitor.h
 * @author        harry
 * @date          2026-02-09
 */
#pragma once

#include <chrono>
#include <string>
#include <unordered_map>

#include "MonitorInter.h"

namespace monitor {
/**
 * @brief         CPU 软中断监控器，采集各 CPU 核心不同软中断类型的速率
 *
 */
class CpuSoftIrqMonitor : public MonitorInter {
    /**
     * @brief         软中断累计值缓存，用于计算两次采样间的中断速率
     *
     */
    struct SoftIrq {
        std::string cpu_name;
        int64_t hi;
        int64_t timer;
        int64_t net_tx;
        int64_t net_rx;
        int64_t block;
        int64_t irq_poll;
        int64_t tasklet;
        int64_t sched;
        int64_t hrtimer;
        int64_t rcu;
        std::chrono::steady_clock::time_point timepoint;
    };

public:
    CpuSoftIrqMonitor() = default;

    /**
     * @brief         采集一次软中断数据并写入 MonitorInfo
     *
     * @param         monitorInfo 监控数据输出对象
     */
    void updateOnce(monitor::proto::MonitorInfo *monitorInfo) override;

    /**
     * @brief         停止软中断监控器，当前实现无额外资源需要释放
     *
     */
    void stop() override {}

private:
    std::unordered_map<std::string, struct SoftIrq> cpuSoftIrqs_; // 上一次软中断采样缓存
};
} // namespace monitor
