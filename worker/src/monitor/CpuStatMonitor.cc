#include "CpuStatMonitor.h"
#include "MonitorStructs.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include "monitorInfo.grpc.pb.h"
#include "monitorInfo.pb.h"

namespace monitor
{
void CpuStatMonitor::updateOnce(monitor::proto::MonitorInfo *monitorInfo)
{
    int fd = open("/dev/cpu_stat_monitor", O_RDONLY);
    if (fd < 0) return;

    size_t statCount = 128; // 假设最多128个CPU
    size_t statSize = sizeof(struct cpu_stat) * statCount;
    void *addr = mmap(nullptr, statSize, PROT_READ, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
    {
        close(fd);
        return;
    }

    struct cpu_stat *stats = static_cast<struct cpu_stat *>(addr);
    for (size_t i = 0; i < statCount; ++i)
    {
        if (stats[i].cpu_name[0] == '\0') break;
        auto it = cpuStatMap_.find(stats[i].cpu_name);
        if (it != cpuStatMap_.end())
        {
            struct CpuStat old = it->second;
            auto cpu_stat_msg = monitorInfo->add_cpu_stat();
            float newCpuTotalTime = stats[i].user + stats[i].system +
                                    stats[i].idle + stats[i].nice +
                                    stats[i].iowait + stats[i].irq +
                                    stats[i].softirq + stats[i].steal;
            float oldCpuTotalTime = old.user + old.system + old.idle +
                                    old.nice + old.io_wait + old.irq +
                                    old.soft_irq + old.steal;
            float newCpuBusyTime = stats[i].user + stats[i].system +
                                   stats[i].nice + stats[i].irq +
                                   stats[i].softirq + stats[i].steal;
            float oldCpuBusyTime = old.user + old.system + old.nice + old.irq +
                                   old.soft_irq + old.steal;

            float cpuPercent = (newCpuBusyTime - oldCpuBusyTime) /
                               (newCpuTotalTime - oldCpuTotalTime) * 100.00;
            float cpuUserPercent = (stats[i].user - old.user) /
                                   (newCpuTotalTime - oldCpuTotalTime) * 100.00;
            float cpuSystemPercent = (stats[i].system - old.system) /
                                     (newCpuTotalTime - oldCpuTotalTime) *
                                     100.00;
            float cpuNicePercent = (stats[i].nice - old.nice) /
                                   (newCpuTotalTime - oldCpuTotalTime) * 100.00;
            float cpuIdlePercent = (stats[i].idle - old.idle) /
                                   (newCpuTotalTime - oldCpuTotalTime) * 100.00;
            float cpuIOWaitPercent = (stats[i].iowait - old.io_wait) /
                                     (newCpuTotalTime - oldCpuTotalTime) *
                                     100.00;
            float cpuIrqPercent = (stats[i].irq - old.irq) /
                                  (newCpuTotalTime - oldCpuTotalTime) * 100.00;
            float cpuSoftIrqPercent = (stats[i].softirq - old.soft_irq) /
                                      (newCpuTotalTime - oldCpuTotalTime) *
                                      100.00;
            cpu_stat_msg->set_cpu_name(stats[i].cpu_name);
            cpu_stat_msg->set_cpu_percent(cpuPercent);
            cpu_stat_msg->set_usr_percent(cpuUserPercent);
            cpu_stat_msg->set_system_percent(cpuSystemPercent);
            cpu_stat_msg->set_nice_percent(cpuNicePercent);
            cpu_stat_msg->set_idle_percent(cpuIdlePercent);
            cpu_stat_msg->set_io_wait_percent(cpuIOWaitPercent);
            cpu_stat_msg->set_irq_percent(cpuIrqPercent);
            cpu_stat_msg->set_soft_irq_percent(cpuSoftIrqPercent);
        }
        // 将内核结构体数据转换为内部 CpuStat 结构体
        CpuStat &cached = cpuStatMap_[stats[i].cpu_name];
        cached.cpu_name = stats[i].cpu_name;
        cached.user = stats[i].user;
        cached.nice = stats[i].nice;
        cached.system = stats[i].system;
        cached.idle = stats[i].idle;
        cached.io_wait = stats[i].iowait;
        cached.irq = stats[i].irq;
        cached.soft_irq = stats[i].softirq;
        cached.steal = stats[i].steal;
        cached.guest = stats[i].guest;
        cached.guest_nice = stats[i].guest_nice;
    }

    munmap(addr, statSize);
    close(fd);
    return;
}
} // namespace monitor