/*
 * @Author: harry
 * @Date: 2026-02-08 23:47:48
 * @Version: 1.0
 * @LastEditors: harry
 * @LastEditTime: 2026-02-09 00:37:54
 * @Description:
 * @FilePath: /monitor-system/worker/monitor/MetricCollector.h
 */
#pragma once

#include <vector>
#include <string>
#include <memory>

#include "monitor/MonitorInter.h"

namespace monitor
{
class MetricCollector
{
public:
    MetricCollector() = default;
    ~MetricCollector() = default;

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