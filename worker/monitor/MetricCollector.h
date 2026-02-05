#pragma once

#include <vector>
#include <string>
#include <memory>

#include "monitor/MonitorInter.h"

namespace monitor
{
class MetricCollector
{
public:
    MetricCollector() = default;
    ~MetricCollector() = default;

    void collectAll(monitor::proto::MonitorInfo &info);
private:
    std::vector<std::unique_ptr<MonitorInter>> monitors_;
    std::string hostname_;
};
}; // namespace monitor