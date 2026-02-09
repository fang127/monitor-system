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

namespace monitor
{

/**
 * 基于 eBPF 的网络流量监控器
 *
 * 使用 eBPF tracepoint 挂载到内核网络路径，
 * 实时统计每个网卡的收发流量。
 */
class NetEbpfMonitor : public MonitorInter
{
public:
    NetEbpfMonitor();
    ~NetEbpfMonitor() override;

    void updateOnce(monitor::proto::MonitorInfo *monitorInfo) override;
    void stop() override;

    // 检查 eBPF 是否成功加载
    bool isLoaded() const { return loaded_; }

private:
    // 初始化 eBPF 程序
    bool initEbpf();

    // 清理 eBPF 资源
    void cleanupEbpf();

    // 根据 ifindex 获取网卡名称
    std::string getIfName(uint32_t ifindex);

    // 上一次采集的数据，用于计算速率
    struct NetStatCache
    {
        uint64_t rcv_bytes;
        uint64_t rcv_packets;
        uint64_t snd_bytes;
        uint64_t snd_packets;
        std::chrono::steady_clock::time_point timestamp;
    };

    std::unordered_map<uint32_t, NetStatCache> cache_;      // key: ifindex
    std::unordered_map<uint32_t, std::string> ifnameCache_; // ifindex -> name
    std::vector<uint32_t> attachedIfindexes_; // 已附加 TC hook 的网卡

    struct bpf_object *bpfObj_ = nullptr;
    int mapFd_ = -1;
    bool loaded_ = false;

    std::chrono::steady_clock::time_point lastUpdate_;
};

} // namespace monitor