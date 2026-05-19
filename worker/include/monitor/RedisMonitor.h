#pragma once

#include "MonitorInter.h"
#include "monitor_info.pb.h"

#include <cstdint>
#include <string>
#include <vector>

namespace monitor {

/**
 * @brief Redis 实例监控器，按环境变量配置采集一个或多个 Redis 实例的 INFO、复制和慢日志指标。
 */
class RedisMonitor : public MonitorInter {
public:
    /**
     * @brief 构造 Redis 监控器并从环境变量加载监控目标。
     */
    RedisMonitor();

    /**
     * @brief 采集一次 Redis 指标并写入 MonitorInfo。
     *
     * @param monitorInfo 监控数据输出对象
     */
    void updateOnce(monitor::proto::MonitorInfo *monitorInfo) override;

    /**
     * @brief 停止 Redis 监控器，当前实现无额外资源需要释放。
     */
    void stop() override {}

private:
    /**
     * @brief 单个 Redis 监控目标配置。
     */
    struct Target {
        std::string instance;
        std::string host = "127.0.0.1";
        uint32_t port = 6379;
        std::string password;
    };

    bool enabled_ = false;            // 是否启用 Redis 监控
    unsigned int timeoutSeconds_ = 3; // Redis 连接与读写超时时间，单位秒
    std::vector<Target> targets_;     // Redis 监控目标列表
};

} // namespace monitor
