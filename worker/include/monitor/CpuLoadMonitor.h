/*!
 * @file          CpuLoadMonitor.h
 * @author        harry
 * @date          2026-02-09
 */
#pragma once

#include "MonitorInter.h"
#include "monitor_info.pb.h"

namespace monitor
{
class CpuLoadMonitor : public MonitorInter
{
public:
    CpuLoadMonitor() {}
    void updateOnce(monitor::proto::MonitorInfo *monitorInfo) override;
    void stop() override {}

private:
    float loadAvg1_;
    float loadAvg3_;
    float loadAvg15_;
};
}; // namespace monitor