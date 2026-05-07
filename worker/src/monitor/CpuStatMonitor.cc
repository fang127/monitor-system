#include "CpuStatMonitor.h"
#include "MonitorStructs.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdint>
#include <cstring>
#include "monitor_info.grpc.pb.h"
#include "monitor_info.pb.h"

namespace monitor {
void CpuStatMonitor::updateOnce(monitor::proto::MonitorInfo *monitorInfo) {
    int fd = open("/dev/CpuStatCollector", O_RDONLY);
    if (fd < 0) return;

    size_t statCount = 128; // 假设最多128个CPU
    size_t statSize = sizeof(struct cpu_stat) * statCount;
    void *addr = mmap(nullptr, statSize, PROT_READ, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        close(fd);
        return;
    }

    struct cpu_stat *stats = static_cast<struct cpu_stat *>(addr);
    struct CpuTotals {
        uint64_t user = 0;
        uint64_t nice = 0;
        uint64_t system = 0;
        uint64_t idle = 0;
        uint64_t iowait = 0;
        uint64_t irq = 0;
        uint64_t softirq = 0;
        uint64_t steal = 0;
    };

    auto addKernelTotals = [](CpuTotals &total, const struct cpu_stat &stat) {
        total.user += stat.user;
        total.nice += stat.nice;
        total.system += stat.system;
        total.idle += stat.idle;
        total.iowait += stat.iowait;
        total.irq += stat.irq;
        total.softirq += stat.softirq;
        total.steal += stat.steal;
    };

    auto addCachedTotals = [](CpuTotals &total, const CpuStat &stat) {
        total.user += stat.user;
        total.nice += stat.nice;
        total.system += stat.system;
        total.idle += stat.idle;
        total.iowait += stat.io_wait;
        total.irq += stat.irq;
        total.softirq += stat.soft_irq;
        total.steal += stat.steal;
    };

    auto totalTime = [](const CpuTotals &total) {
        return total.user + total.nice + total.system + total.idle +
               total.iowait + total.irq + total.softirq + total.steal;
    };

    auto busyTime = [](const CpuTotals &total) {
        return total.user + total.nice + total.system + total.irq +
               total.softirq + total.steal;
    };

    auto setCpuStatPercentages = [&](monitor::proto::CpuStat *cpu_stat_msg,
                                     const CpuTotals &curr,
                                     const CpuTotals &old) {
        const double currTotal = static_cast<double>(totalTime(curr));
        const double oldTotal = static_cast<double>(totalTime(old));
        const double totalDelta = currTotal - oldTotal;
        if (totalDelta <= 0) return false;

        cpu_stat_msg->set_cpu_percent(
            static_cast<float>((static_cast<double>(busyTime(curr)) -
                                static_cast<double>(busyTime(old))) /
                               totalDelta * 100.0));
        cpu_stat_msg->set_usr_percent(
            static_cast<float>((static_cast<double>(curr.user) -
                                static_cast<double>(old.user)) /
                               totalDelta * 100.0));
        cpu_stat_msg->set_system_percent(
            static_cast<float>((static_cast<double>(curr.system) -
                                static_cast<double>(old.system)) /
                               totalDelta * 100.0));
        cpu_stat_msg->set_nice_percent(
            static_cast<float>((static_cast<double>(curr.nice) -
                                static_cast<double>(old.nice)) /
                               totalDelta * 100.0));
        cpu_stat_msg->set_idle_percent(
            static_cast<float>((static_cast<double>(curr.idle) -
                                static_cast<double>(old.idle)) /
                               totalDelta * 100.0));
        cpu_stat_msg->set_io_wait_percent(
            static_cast<float>((static_cast<double>(curr.iowait) -
                                static_cast<double>(old.iowait)) /
                               totalDelta * 100.0));
        cpu_stat_msg->set_irq_percent(
            static_cast<float>((static_cast<double>(curr.irq) -
                                static_cast<double>(old.irq)) /
                               totalDelta * 100.0));
        cpu_stat_msg->set_soft_irq_percent(
            static_cast<float>((static_cast<double>(curr.softirq) -
                                static_cast<double>(old.softirq)) /
                               totalDelta * 100.0));
        return true;
    };

    CpuTotals allCurr;
    CpuTotals allOld;
    bool hasAggregateBase = false;

    for (size_t i = 0; i < statCount; ++i) {
        if (stats[i].cpu_name[0] == '\0') break;
        auto it = cpuStatMap_.find(stats[i].cpu_name);
        if (it != cpuStatMap_.end()) {
            struct CpuStat old = it->second;
            CpuTotals curr;
            CpuTotals prev;
            addKernelTotals(curr, stats[i]);
            addCachedTotals(prev, old);

            auto cpu_stat_msg = monitorInfo->add_cpu_stat();
            cpu_stat_msg->set_cpu_name(stats[i].cpu_name);
            if (!setCpuStatPercentages(cpu_stat_msg, curr, prev)) {
                monitorInfo->mutable_cpu_stat()->RemoveLast();
            }

            addKernelTotals(allCurr, stats[i]);
            addCachedTotals(allOld, old);
            hasAggregateBase = true;
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

    if (hasAggregateBase) {
        auto *all_cpu_stat_msg = monitorInfo->add_cpu_stat();
        all_cpu_stat_msg->set_cpu_name("all");
        if (!setCpuStatPercentages(all_cpu_stat_msg, allCurr, allOld)) {
            monitorInfo->mutable_cpu_stat()->RemoveLast();
        }
    }

    munmap(addr, statSize);
    close(fd);
    return;
}
} // namespace monitor
