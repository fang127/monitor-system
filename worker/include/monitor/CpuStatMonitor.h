/*!
 * @file          CpuLoadMonitor.h
 * @author        harry
 * @date          2026-02-09
 */
#pragma once

#include <string>
#include <unordered_map>

#include "MonitorInter.h"
#include "monitor_info.pb.h"

namespace monitor {
class CpuStatMonitor : public MonitorInter {
    struct CpuStat {
        std::string cpu_name;
        float user;
        float system;
        float idle;
        float nice;
        float io_wait;
        float irq;
        float soft_irq;
        float steal;
        float guest;
        float guest_nice;
    };

public:
    CpuStatMonitor() {}
    void updateOnce(monitor::proto::MonitorInfo *monitorInfo) override;
    void stop() override {}

private:
    std::unordered_map<std::string, struct CpuStat> cpuStatMap_;
};

} // namespace monitor