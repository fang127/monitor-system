/*
 * @Author: harry
 * @Date: 2026-02-09 00:25:10
 * @Version: 1.0
 * @LastEditors: harry
 * @LastEditTime: 2026-02-09 00:27:04
 * @Description:
 * @FilePath: /monitor-system/worker/monitor/DiskMonitor.h
 */
#pragma once

#include <string>
#include <unordered_map>

#include "monitor/MonitorInter.h"
#include "monitor_info.pb.h"

namespace monitor
{

class DiskMonitor : public MonitorInter
{
public:
    DiskMonitor() {}
    void updateOnce(monitor::proto::MonitorInfo *monitorInfo) override;
    void stop() override {}
};

}  // namespace monitor
