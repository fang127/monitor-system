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

    // 获取内核提供的 CPU 统计数据
    struct cpu_stat *stats = static_cast<struct cpu_stat *>(addr);
    // 定义一个结构体来累加 CPU 时间总和
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

    // 定义一个 lambda 函数来累加内核提供的 CPU 统计数据到 CpuTotals 结构体
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

    // 定义一个 lambda 函数来累加之前缓存的 CPU 统计数据到 CpuTotals 结构体
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

    // 定义一个 lambda 函数来计算总时间和忙碌时间，并设置 CPU 使用率百分比
    auto totalTime = [](const CpuTotals &total) {
        return total.user + total.nice + total.system + total.idle + total.iowait + total.irq + total.softirq +
               total.steal;
    };

    auto busyTime = [](const CpuTotals &total) {
        return total.user + total.nice + total.system + total.irq + total.softirq + total.steal;
    };

    // 定义一个 lambda 函数来计算 CPU 使用率百分比，并设置到消息中
    auto setCpuStatPercentages = [&](monitor::proto::CpuStat *cpu_stat_msg, const CpuTotals &curr,
                                     const CpuTotals &old) {
        const double currTotal = static_cast<double>(totalTime(curr));
        const double oldTotal = static_cast<double>(totalTime(old));
        const double totalDelta = currTotal - oldTotal; // 计算总时间的增量
        if (totalDelta <= 0) return false;

        // 计算各个时间的增量，并转换为百分比
        // 当前忙碌时间增量除以总时间增量，得到 CPU 使用率百分比
        cpu_stat_msg->set_cpu_percent(static_cast<float>(
            (static_cast<double>(busyTime(curr)) - static_cast<double>(busyTime(old))) / totalDelta * 100.0));
        cpu_stat_msg->set_usr_percent(
            static_cast<float>((static_cast<double>(curr.user) - static_cast<double>(old.user)) / totalDelta * 100.0));
        cpu_stat_msg->set_system_percent(static_cast<float>(
            (static_cast<double>(curr.system) - static_cast<double>(old.system)) / totalDelta * 100.0));
        cpu_stat_msg->set_nice_percent(
            static_cast<float>((static_cast<double>(curr.nice) - static_cast<double>(old.nice)) / totalDelta * 100.0));
        cpu_stat_msg->set_idle_percent(
            static_cast<float>((static_cast<double>(curr.idle) - static_cast<double>(old.idle)) / totalDelta * 100.0));
        cpu_stat_msg->set_io_wait_percent(static_cast<float>(
            (static_cast<double>(curr.iowait) - static_cast<double>(old.iowait)) / totalDelta * 100.0));
        cpu_stat_msg->set_irq_percent(
            static_cast<float>((static_cast<double>(curr.irq) - static_cast<double>(old.irq)) / totalDelta * 100.0));
        cpu_stat_msg->set_soft_irq_percent(static_cast<float>(
            (static_cast<double>(curr.softirq) - static_cast<double>(old.softirq)) / totalDelta * 100.0));
        return true;
    };

    CpuTotals allCurr;
    CpuTotals allOld;
    bool hasAggregateBase = false; // 标记是否有有效的基准数据用于计算 "all" 的百分比

    // 遍历内核提供的 CPU 统计数据，计算每个 CPU 的使用率，并累加到 "all" 的总和中
    for (size_t i = 0; i < statCount; ++i) {
        if (stats[i].cpu_name[0] == '\0') break;
        auto it = cpuStatMap_.find(stats[i].cpu_name);
        if (it != cpuStatMap_.end()) {
            struct CpuStat old = it->second;
            CpuTotals curr;
            CpuTotals prev;
            addKernelTotals(curr, stats[i]); // 累加当前内核数据到 curr
            addCachedTotals(prev, old);      // 累加之前缓存数据到 prev

            // 将当前 CPU 的使用率百分比设置到消息中
            auto cpu_stat_msg = monitorInfo->add_cpu_stat();
            cpu_stat_msg->set_cpu_name(stats[i].cpu_name);
            // 如果无法计算百分比（例如总时间增量为0），则从消息中移除该 CPU 的统计信息
            if (!setCpuStatPercentages(cpu_stat_msg, curr, prev)) {
                monitorInfo->mutable_cpu_stat()->RemoveLast();
            }

            // 累加到 "all" 的总和中
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

    // 如果有有效的基准数据，计算 "all" 的使用率百分比，并设置到消息中
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
