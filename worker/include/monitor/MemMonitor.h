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
/**
 * @brief         内存监控器，从 /proc/meminfo 采集内存容量、缓存、活跃页和脏页等指标
 *
 */
class MemMonitor : public MonitorInter {
    /**
     * @brief         /proc/meminfo 中使用到的内存字段集合
     *
     */
    struct MenInfo {
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

    /**
     * @brief         采集一次内存指标并写入 MonitorInfo
     *
     * @param         monitorInfo 监控数据输出对象
     */
    void updateOnce(monitor::proto::MonitorInfo *monitorInfo) override;

    /**
     * @brief         停止内存监控器，当前实现无额外资源需要释放
     *
     */
    void stop() override {}
};

} // namespace monitor
