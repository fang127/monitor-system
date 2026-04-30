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
class CpuSoftIrqMonitor : public MonitorInter {
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
    void updateOnce(monitor::proto::MonitorInfo *monitorInfo) override;
    void stop() override {}

private:
    std::unordered_map<std::string, struct SoftIrq> cpuSoftIrqs_;
};
} // namespace monitor