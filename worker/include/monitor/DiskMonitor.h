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
 * @brief         磁盘监控器，从 /proc/diskstats 采集磁盘吞吐、IOPS、延迟和利用率
 *
 */
class DiskMonitor : public MonitorInter {
public:
    DiskMonitor() {}

    /**
     * @brief         采集一次磁盘指标并写入 MonitorInfo
     *
     * @param         monitorInfo 监控数据输出对象
     */
    void updateOnce(monitor::proto::MonitorInfo *monitorInfo) override;

    /**
     * @brief         停止磁盘监控器，当前实现无额外资源需要释放
     *
     */
    void stop() override {}
};

} // namespace monitor
