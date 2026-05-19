/*!
 * @file          CpuLoadMonitor.h
 * @author        harry
 * @date          2026-02-09
 */
#pragma once

#include <string>
#include <unordered_map>
#include <chrono>
#include <vector>
#include <memory>

#include "MonitorInter.h"

struct bpf_object;

namespace monitor {

/**
 * 基于 eBPF 的网络流量监控器
 *
 * 使用 eBPF tracepoint 挂载到内核网络路径，
 * 实时统计每个网卡的收发流量。
 */
class NetEbpfMonitor : public MonitorInter {
public:
    NetEbpfMonitor();
    ~NetEbpfMonitor() override;

    void updateOnce(monitor::proto::MonitorInfo *monitorInfo) override;
    void stop() override;

    /**
     * @brief         检查 eBPF 程序是否已经成功加载
     *
     * @return        已加载返回 true，否则返回 false
     */
    bool isLoaded() const { return loaded_; }

private:
    /**
     * @brief         初始化 eBPF 程序并附加到网卡 TC hook
     *
     * @return        初始化成功返回 true，否则返回 false
     */
    bool initEbpf();

    /**
     * @brief         清理 eBPF 程序、map 和 TC hook 相关资源
     *
     */
    void cleanupEbpf();

    /**
     * @brief         根据 ifindex 获取网卡名称
     *
     * @param         ifindex 网卡索引
     * @return        网卡名称
     */
    std::string getIfName(uint32_t ifindex);

    /**
     * @brief         上一次采集的数据，用于计算速率
     *
     */
    struct NetStatCache {
        uint64_t rcv_bytes;
        uint64_t rcv_packets;
        uint64_t snd_bytes;
        uint64_t snd_packets;
        std::chrono::steady_clock::time_point timestamp;
    };

    std::unordered_map<uint32_t, NetStatCache> cache_;      // 键为 ifindex
    std::unordered_map<uint32_t, std::string> ifnameCache_; // ifindex 到网卡名称的缓存
    std::vector<uint32_t> attachedIfindexes_;               // 已附加 TC hook 的网卡

    struct bpf_object *bpfObj_ = nullptr;
    int mapFd_ = -1;
    bool loaded_ = false;

    std::chrono::steady_clock::time_point lastUpdate_;
};

} // namespace monitor
