/*!
 * @file          CpuLoadMonitor.h
 * @author        harry
 * @date          2026-02-09
 */
#pragma once

#include "monitor_info.pb.h"

namespace monitor {
class MonitorInter {
public:
    MonitorInter() = default;
    virtual ~MonitorInter() = default;
    /*!
     * @brief         更新监控数据
     *
     * @param         info 监控数据
     * @attention
     */
    virtual void updateOnce(monitor::proto::MonitorInfo *info) = 0;
    /**
     * @brief 启动监控
     * @return
     */
    virtual void stop() = 0;
};
}; // namespace monitor