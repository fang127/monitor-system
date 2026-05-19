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
#include "monitor_info.pb.h"

namespace monitor {
/**
 * @brief         网络监控器，从 /proc/net/dev 采集网卡吞吐、包速率、错误和丢包指标
 *
 */
class NetMonitor : public MonitorInter {
    /**
     * @brief         网卡累计统计缓存，用于计算两次采样间的速率
     *
     */
    struct NetInfo {
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

    /**
     * @brief         采集一次网络指标并写入 MonitorInfo
     *
     * @param         monitorInfo 监控数据输出对象
     */
    void updateOnce(monitor::proto::MonitorInfo *monitorInfo) override;

    /**
     * @brief         停止网络监控器，当前实现无额外资源需要释放
     *
     */
    void stop() override {}

private:
    std::unordered_map<std::string, NetInfo> lastNetInfo_; // 上一次网卡采样缓存
};

} // namespace monitor
