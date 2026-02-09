/*
 * @Author: harry
 * @Date: 2026-02-06 01:10:50
 * @Version: 1.0
 * @LastEditors: harry
 * @LastEditTime: 2026-02-09 00:38:02
 * @Description:
 * @FilePath: /monitor-system/worker/monitor/MonitorInter.h
 */
#pragma once

#include "monitor_info.pb.h"

namespace monitor
{
class MonitorInter
{
public:
    MonitorInter() = default;
    virtual ~MonitorInter() = default;
    /**
     * @brief 更新监控信息
     * @param[in] info
     * @return
     */
    virtual void updateOnce(monitor::proto::MonitorInfo &info) = 0;
    /**
     * @brief 启动监控
     * @return
     */
    virtual void stop() = 0;
};
}; // namespace monitor