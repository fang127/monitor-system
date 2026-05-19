/*!
 * @file          CpuLoadMonitor.h
 * @author        harry
 * @date          2026-02-09
 */
#pragma once

#include "MonitorInter.h"
#include "monitor_info.pb.h"

namespace monitor {
/**
 * @brief         CPU 负载监控器，从内核模块或 /proc/loadavg 采集 1、3、15 分钟平均负载
 *
 */
class CpuLoadMonitor : public MonitorInter {
public:
    CpuLoadMonitor() {}

    /**
     * @brief         采集一次 CPU 负载数据并写入 MonitorInfo
     *
     * @param         monitorInfo 监控数据输出对象
     */
    void updateOnce(monitor::proto::MonitorInfo *monitorInfo) override;

    /**
     * @brief         停止 CPU 负载监控器，当前实现无额外资源需要释放
     *
     */
    void stop() override {}

private:
    float loadAvg1_;  // 1 分钟平均负载
    float loadAvg3_;  // 3 分钟平均负载
    float loadAvg15_; // 15 分钟平均负载
};
}; // namespace monitor
