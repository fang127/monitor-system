#pragma once

#include "MonitorInter.h"
#include "monitor_info.pb.h"

#include <cstdint>
#include <string>
#include <vector>

namespace monitor {

/**
 * @brief         MySQL 监控器，按配置采集一个或多个 MySQL 实例的连接、QPS、事务、慢查询和复制状态指标
 *
 */
class MysqlMonitor : public MonitorInter {
public:
    /**
     * @brief         构造 MySQL 监控器并从环境变量加载监控目标
     *
     */
    MysqlMonitor();

    /**
     * @brief         采集一次 MySQL 指标并写入 MonitorInfo
     *
     * @param         monitorInfo 监控数据输出对象
     */
    void updateOnce(monitor::proto::MonitorInfo *monitorInfo) override;

    /**
     * @brief         停止 MySQL 监控器，当前实现无额外资源需要释放
     *
     */
    void stop() override {}

private:
    /**
     * @brief         单个 MySQL 监控目标配置
     *
     */
    struct Target {
        std::string instance;
        std::string host = "127.0.0.1";
        uint32_t port = 3306;
        std::string user;
        std::string password;
        std::string role_hint = "unknown";
    };

    bool enabled_ = false;            // 是否启用 MySQL 监控
    unsigned int timeoutSeconds_ = 3; // MySQL 连接与读写超时时间，单位秒
    std::vector<Target> targets_;     // MySQL 监控目标列表
};

} // namespace monitor
