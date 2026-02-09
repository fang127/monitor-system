#include "MemMonitor.h"
#include "ReadFile.h"

#include <vector>
#include <string>

namespace monitor
{
static constexpr float KBToGB = 1000 * 1000;

void MemMonitor::updateOnce(monitor::proto::MonitorInfo *monitorInfo)
{
    ReadFile memFile("/proc/meminfo");
    struct MenInfo memInfo;
    std::vector<std::string> memDatas;
    while (memFile.ReadLine(memDatas))
    {
        if (memDatas[0] == "MemTotal:")
        {
            memInfo.total = std::stoll(memDatas[1]);
        }
        else if (memDatas[0] == "MemFree:")
        {
            memInfo.free = std::stoll(memDatas[1]);
        }
        else if (memDatas[0] == "MemAvailable:")
        {
            memInfo.avail = std::stoll(memDatas[1]);
        }
        else if (memDatas[0] == "Buffers:")
        {
            memInfo.buffers = std::stoll(memDatas[1]);
        }
        else if (memDatas[0] == "Cached:")
        {
            memInfo.cached = std::stoll(memDatas[1]);
        }
        else if (memDatas[0] == "SwapCached:")
        {
            memInfo.swap_cached = std::stoll(memDatas[1]);
        }
        else if (memDatas[0] == "Active:")
        {
            memInfo.active = std::stoll(memDatas[1]);
        }
        else if (memDatas[0] == "Inactive:")
        {
            memInfo.in_active = std::stoll(memDatas[1]);
        }
        else if (memDatas[0] == "Active(anon):")
        {
            memInfo.active_anon = std::stoll(memDatas[1]);
        }
        else if (memDatas[0] == "Inactive(anon):")
        {
            memInfo.inactive_anon = std::stoll(memDatas[1]);
        }
        else if (memDatas[0] == "Active(file):")
        {
            memInfo.active_file = std::stoll(memDatas[1]);
        }
        else if (memDatas[0] == "Inactive(file):")
        {
            memInfo.inactive_file = std::stoll(memDatas[1]);
        }
        else if (memDatas[0] == "Dirty:")
        {
            memInfo.dirty = std::stoll(memDatas[1]);
        }
        else if (memDatas[0] == "Writeback:")
        {
            memInfo.writeback = std::stoll(memDatas[1]);
        }
        else if (memDatas[0] == "AnonPages:")
        {
            memInfo.anon_pages = std::stoll(memDatas[1]);
        }
        else if (memDatas[0] == "Mapped:")
        {
            memInfo.mapped = std::stoll(memDatas[1]);
        }
        else if (memDatas[0] == "KReclaimable:")
        {
            memInfo.kReclaimable = std::stoll(memDatas[1]);
        }
        else if (memDatas[0] == "SReclaimable:")
        {
            memInfo.sReclaimable = std::stoll(memDatas[1]);
        }
        else if (memDatas[0] == "SUnreclaim:")
        {
            memInfo.sUnreclaim = std::stoll(memDatas[1]);
        }
        memDatas.clear();
    }

    auto memDetail = monitorInfo->mutable_mem_info();

    memDetail->set_used_percent((memInfo.total - memInfo.avail) * 1.0 /
                                memInfo.total * 100.0);
    memDetail->set_total(memInfo.total / KBToGB);
    memDetail->set_free(memInfo.free / KBToGB);
    memDetail->set_avail(memInfo.avail / KBToGB);
    memDetail->set_buffers(memInfo.buffers / KBToGB);
    memDetail->set_cached(memInfo.cached / KBToGB);
    memDetail->set_swap_cached(memInfo.swap_cached / KBToGB);
    memDetail->set_active(memInfo.active / KBToGB);
    memDetail->set_inactive(memInfo.in_active / KBToGB);
    memDetail->set_active_anon(memInfo.active_anon / KBToGB);
    memDetail->set_inactive_anon(memInfo.inactive_anon / KBToGB);
    memDetail->set_active_file(memInfo.active_file / KBToGB);
    memDetail->set_inactive_file(memInfo.inactive_file / KBToGB);
    memDetail->set_dirty(memInfo.dirty / KBToGB);
    memDetail->set_writeback(memInfo.writeback / KBToGB);
    memDetail->set_anon_pages(memInfo.anon_pages / KBToGB);
    memDetail->set_mapped(memInfo.mapped / KBToGB);
    memDetail->set_kreclaimable(memInfo.kReclaimable / KBToGB);
    memDetail->set_sreclaimable(memInfo.sReclaimable / KBToGB);
    memDetail->set_sunreclaim(memInfo.sUnreclaim / KBToGB);

    return;
}
} // namespace monitor