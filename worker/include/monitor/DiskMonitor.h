/*!
 * @file          CpuLoadMonitor.h
 * @author        harry
 * @date          2026-02-09
 */
#pragma once

#include "MonitorInter.h"
#include "monitor_info.pb.h"

namespace monitor {

class DiskMonitor : public MonitorInter {
public:
    DiskMonitor() {}
    void updateOnce(monitor::proto::MonitorInfo *monitorInfo) override;
    void stop() override {}
};

} // namespace monitor
