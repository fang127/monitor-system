#pragma once

#include "MonitorInter.h"
#include "monitor_info.pb.h"

#include <cstdint>
#include <string>
#include <vector>

namespace monitor {

class MysqlMonitor : public MonitorInter {
public:
    MysqlMonitor();
    void updateOnce(monitor::proto::MonitorInfo *monitorInfo) override;
    void stop() override {}

private:
    struct Target {
        std::string instance;
        std::string host = "127.0.0.1";
        uint32_t port = 3306;
        std::string user;
        std::string password;
        std::string role_hint = "unknown";
    };

    bool enabled_ = false;
    unsigned int timeoutSeconds_ = 3;
    std::vector<Target> targets_;
};

} // namespace monitor
