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
class CpuStatMonitor : public MonitorInter {
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
    void updateOnce(monitor::proto::MonitorInfo *monitorInfo) override;
    void stop() override {}

private:
    std::unordered_map<std::string, struct CpuStat> cpuStatMap_;
};

} // namespace monitor
