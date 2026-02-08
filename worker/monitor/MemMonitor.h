/*
 * @Author: harry
 * @Date: 2026-02-09 00:32:28
 * @Version: 1.0
 * @LastEditors: harry
 * @LastEditTime: 2026-02-09 00:37:53
 * @Description:
 * @FilePath: /monitor-system/worker/monitor/MemMonitor.h
 */
#pragma once

#include <string>
#include <unordered_map>

#include "monitor/MonitorInter.h"
#include "monitor_info.pb.h"

namespace monitor
{
class MemMonitor : public MonitorInter
{
    struct MenInfo
    {
        int64_t total;
        int64_t free;
        int64_t avail;
        int64_t buffers;
        int64_t cached;
        int64_t swap_cached;
        int64_t active;
        int64_t in_active;
        int64_t active_anon;
        int64_t inactive_anon;
        int64_t active_file;
        int64_t inactive_file;
        int64_t dirty;
        int64_t writeback;
        int64_t anon_pages;
        int64_t mapped;
        int64_t kReclaimable;
        int64_t sReclaimable;
        int64_t sUnreclaim;
    };

public:
    MemMonitor() {}
    void updateOnce(monitor::proto::MonitorInfo *monitorInfo) override;
    void stop() override {}
};

}  // namespace monitor