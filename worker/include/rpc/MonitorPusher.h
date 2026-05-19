/**
 * @brief         MonitorPusher 类定义
 * @file          MonitorPusher.h
 * @author        harry
 * @date          2026-02-11
 */
#pragma once

#include "MetricCollector.h"
#include "monitor_info.grpc.pb.h"

#include <string>
#include <memory>
#include <atomic>
#include <thread>

namespace monitor {
class MonitorPusher {
public:
    /**
     * @brief         构造监控数据推送器
     *
     * @param         managerAddress manager 的 gRPC 地址
     * @param         intervalSeconds 推送间隔，单位秒，默认 10 秒
     */
    explicit MonitorPusher(const std::string &managerAddress,
                           int intervalSeconds = 10);
    ~MonitorPusher();

    /**
     * @brief         启动监控推送线程，定期向 manager 推送指标
     *
     */
    void start();

    /**
     * @brief         停止监控推送线程
     *
     */
    void stop();

    /**
     * @brief         获取 manager 的 gRPC 地址
     *
     * @return        manager 地址引用
     */
    const std::string &getManagerAddress() const { return managerAddress_; }

private:
    /**
     * @brief         监控推送主循环，在线程中按固定间隔采集并推送指标，直到推送器停止
     *
     */
    void pushLoop();

    /**
     * @brief         执行一次指标采集和 gRPC 推送
     *
     * @return        推送成功返回 true，否则返回 false
     */
    bool pushOnce();

    std::string managerAddress_; // manager 的 gRPC 地址
    int intervalSeconds_;        // 指标推送间隔，单位秒
    std::atomic<bool>
        running_; // 控制推送器运行状态的标记
    std::unique_ptr<std::thread> thread_;        // 指标推送线程
    std::unique_ptr<MetricCollector> collector_; // 指标采集器实例
    std::unique_ptr<monitor::proto::GrpcManager::Stub>
        stub_; // 与 manager 通信的 gRPC stub
};
} // namespace monitor
