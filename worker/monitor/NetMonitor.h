/*
 * @Author: harry
 * @Date: 2026-02-09 00:41:05
 * @Version: 1.0
 * @LastEditors: harry
 * @LastEditTime: 2026-02-09 00:41:13
 * @Description:
 * @FilePath: /monitor-system/worker/monitor/NetMonitor.h
 */
#pragma once

#include <chrono>
#include <string>
#include <unordered_map>

#include "monitor/MonitorInter.h"
#include "monitor_info.pb.h"

namespace monitor
{
class NetMonitor : public MonitorInter
{
    struct NetInfo
    {
        std::string name;
        uint64_t rcv_bytes;
        uint64_t rcv_packets;
        uint64_t snd_bytes;
        uint64_t snd_packets;
        uint64_t err_in;
        uint64_t err_out;
        uint64_t drop_in;
        uint64_t drop_out;
        std::chrono::steady_clock::time_point timepoint;
    };

public:
    NetMonitor() {}
    void updateOnce(monitor::proto::MonitorInfo *monitorInfo) override;
    void stop() override {}

private:
    std::unordered_map<std::string, NetInfo> lastNetInfo_;
};

}  // namespace monitor