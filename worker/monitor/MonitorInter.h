#pragma once

#include "monitor_info.pb.h"

namespace monitor
{
class MonitorInter
{
public:
    MonitorInter() = default;
    virtual ~MonitorInter() = default;
    virtual void updateOnce(monitor::proto::MonitorInfo& info) = 0;
    virtual void stop() = 0;
};
}; // namespace monitor