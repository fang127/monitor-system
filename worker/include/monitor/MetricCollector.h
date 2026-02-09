/*!
 * @file          CpuLoadMonitor.h
 * @author        harry
 * @date          2026-02-09
 */
#pragma once

#include <vector>
#include <string>
#include <memory>

#include "MonitorInter.h"

namespace monitor
{
class MetricCollector
{
public:
    MetricCollector();
    ~MetricCollector();

    /**
     * @brief 采集所有指标并填充到 MonitorInfo
     * @param[in] &info
     * @return
     */
    void collectAll(monitor::proto::MonitorInfo *monitorInfo);

private:
    std::vector<std::unique_ptr<MonitorInter>> monitors_;
    std::string hostname_;
};
}; // namespace monitor