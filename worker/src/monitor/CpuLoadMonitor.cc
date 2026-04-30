/*!
 * @file          CpuLoadMonitor.h
 * @author        harry
 * @date          2026-02-09
 */
#include "CpuLoadMonitor.h"
#include "MonitorStructs.h"

#include <cstdio>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace monitor {
/**
 * @brief 从/proc/loadavg文件读取CPU负载信息
 * @details
 * 打开系统文件/proc/loadavg，解析并读取1分钟、3分钟、15分钟的CPU平均负载值，
 *          结果通过输出参数返回，读取成功返回true，失败返回false。
 * @param[out] load1 输出参数：存储一分钟平均负载的变量指针
 * @param[out] load3 输出参数：存储三分钟平均负载的变量指针
 * @param[out] load15 输出参数：存储十五分钟平均负载的变量指针
 * @return bool类型，读取成功且解析到3个负载值返回true，否则返回false
 */
static bool readLoadFromProc(float *load1, float *load3, float *load15) {
    FILE *fp = fopen("/proc/loadavg", "r");
    if (!fp) return false;

    int ret = fscanf(fp, "%f %f %f", load1, load3, load15);
    fclose(fp);
    return ret == 3;
}

void CpuLoadMonitor::updateOnce(monitor::proto::MonitorInfo *monitorInfo) {
    // 从/dev/cpu_load_monitor设备读取CPU负载信息
    int fd = open("/dev/CpuStatCollector", O_RDONLY);
    if (fd >= 0) {
        size_t loadSize = sizeof(struct cpu_load);
        void *addr = mmap(nullptr, loadSize, PROT_READ, MAP_SHARED, fd, 0);
        if (addr != MAP_FAILED) {
            struct cpu_load *info = new cpu_load();
            memcpy(static_cast<void *>(info), addr, loadSize);
            auto cpuLoadMsg = monitorInfo->mutable_cpu_load();
            cpuLoadMsg->set_load_avg_1(info->load_avg_1);
            cpuLoadMsg->set_load_avg_3(info->load_avg_3);
            cpuLoadMsg->set_load_avg_15(info->load_avg_15);

            munmap(addr, loadSize);
            close(fd);
            return;
        }
        close(fd);
    }

    // 如果读取失败，从/proc/loadavg文件读取CPU负载信息
    float load1, load3, load15;
    if (readLoadFromProc(&load1, &load3, &load15)) {
        auto cpuLoadMsg = monitorInfo->mutable_cpu_load();
        cpuLoadMsg->set_load_avg_1(load1);
        cpuLoadMsg->set_load_avg_3(load3);
        cpuLoadMsg->set_load_avg_15(load15);
    }
}
} // namespace monitor