/*!
 * @file          CpuLoadMonitor.h
 * @author        harry
 * @date          2026-02-09
 */
#include <unistd.h>
#include <memory>

#include "MetricCollector.h"
#include "CpuLoadMonitor.h"
#include "CpuSoftirqMonitor.h"
#include "CpuStatMonitor.h"
#include "DiskMonitor.h"
#include "MemMonitor.h"
#include "HostInfoMonitor.h"

#ifdef ENABLE_EBPF
#include "NetEbpfMonitor.h"
#else
#include "NetMonitor.h"
#endif

namespace monitor {
MetricCollector::MetricCollector() {
    // 获取主机名
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0)
        hostname_ = hostname;
    else
        hostname_ = "unknown";

    // 初始化所有监控器
    monitors_.push_back(std::make_unique<CpuLoadMonitor>());
    monitors_.push_back(std::make_unique<CpuStatMonitor>());
    monitors_.push_back(std::make_unique<CpuSoftIrqMonitor>());
    monitors_.push_back(std::make_unique<MemMonitor>());
#ifdef ENABLE_EBPF
    monitors_.push_back(std::make_unique<NetEbpfMonitor>());
#else
    monitors_.push_back(std::make_unique<NetMonitor>());
#endif
    monitors_.push_back(std::make_unique<DiskMonitor>());
    monitors_.push_back(std::make_unique<HostInfoMonitor>());
}

MetricCollector::~MetricCollector() {
    for (auto &monitor : monitors_) monitor->stop();
}

void MetricCollector::collectAll(monitor::proto::MonitorInfo *monitorInfo) {
    if (!monitorInfo) return;

    // 设置主机名
    monitorInfo->set_name(hostname_);

    // 调用每个监控器的 updateOnce 方法
    for (auto &monitor : monitors_) monitor->updateOnce(monitorInfo);
}

} // namespace monitor
