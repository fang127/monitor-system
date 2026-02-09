/*
 * @Author: harry
 * @Date: 2026-02-09 00:46:18
 * @Version: 1.0
 * @LastEditors: harry
 * @LastEditTime: 2026-02-09 00:47:04
 * @Description:
 * @FilePath: /monitor-system/worker/monitor/UserMonitor.h
 */
#pragma once

#include <string>
#include "worker/monitor/MonitorInter.h"

namespace monitor
{

/**
 * 用户信息监控器
 *
 * 通过系统调用获取当前进程的实际用户ID(UID)，
 * 然后解析 /etc/passwd 文件查找对应的用户名。
 *
 * 这种方式比 getenv("USER") 更可靠，不依赖环境变量，
 * 在容器环境下也能准确获取用户信息。
 */
class UserMonitor : public MonitorInter
{
public:
    UserMonitor() = default;
    ~UserMonitor() override = default;

    void updateOnce(monitor::proto::MonitorInfo *monitorInfo) override;
    void stop() override {}

private:
    /**
     * @brief 获取用户名
     * @param[in] uid 用户ID
     * @description:用户名，如果未找到返回空字符串
     * @return
     */
    std::string getUsernameByUid(uid_t uid);
};

}  // namespace monitor
