/*
 * @Author: harry
 * @Date: 2026-02-08 23:47:48
 * @Version: 1.0
 * @LastEditors: harry
 * @LastEditTime: 2026-02-09 00:37:30
 * @Description:
 * @FilePath: /monitor-system/worker/monitor/CpuLoadMonitor.h
 */
#pragma once

#include <string>

#include "MonitorInter.h"
#include "monitor_info.pb.h"

namespace monitor
{
class CpuLoadMonitor : public MonitorInter
{
    CpuLoadMonitor() {}
    void updateOnce(monitor::proto::MonitorInfo *monitorInfo) override;
    void stop() override {}

private:
    float loadAvg1_;
    float loadAvg3_;
    float loadAvg15_;
};
}; // namespace monitor