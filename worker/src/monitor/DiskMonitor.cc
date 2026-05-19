#include "DiskMonitor.h"

#include <ctime>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <stdint.h>

namespace monitor {

/**
 * @brief         单次磁盘采样的原始计数器集合
 *
 */
struct DiskSample {
    uint64_t reads, writes, sectors_read, sectors_written;
    uint64_t read_time_ms, write_time_ms, io_in_progress, io_time_ms, weighted_io_time_ms;
};

static std::map<std::string, DiskSample> lastSamples;
static std::map<std::string, double> lastTime;

/**
 * @brief         计算单调递增计数器的差值，计数器回退时返回 0
 *
 * @param         current 当前计数值
 * @param         previous 上一次计数值
 * @return        计数器增量
 */
static double counterDelta(uint64_t current, uint64_t previous) {
    return current >= previous ? static_cast<double>(current - previous) : 0.0;
}

/**
 * @brief         从 /proc/diskstats 采集一次磁盘指标并写入 MonitorInfo
 *
 * @param         monitor_info 监控数据输出对象
 */
void DiskMonitor::updateOnce(monitor::proto::MonitorInfo *monitor_info) {
    std::ifstream ifs("/proc/diskstats");
    std::string line;
    double now = ::time(nullptr);

    while (std::getline(ifs, line)) {
        std::istringstream iss(line);
        int major, minor;
        std::string name;
        DiskSample curr{};
        uint64_t reads_merged = 0;
        uint64_t writes_merged = 0;
        if (!(iss >> major >> minor >> name >> curr.reads >> reads_merged >> curr.sectors_read >> curr.read_time_ms >>
              curr.writes >> writes_merged >> curr.sectors_written >> curr.write_time_ms >> curr.io_in_progress >>
              curr.io_time_ms >> curr.weighted_io_time_ms)) {
            continue;
        }
        if (name.find("loop") == 0 || name.find("ram") == 0) continue; // 跳过虚拟盘

        auto *disk = monitor_info->add_disk_info();
        disk->set_name(name);
        disk->set_reads(curr.reads);
        disk->set_writes(curr.writes);
        disk->set_sectors_read(curr.sectors_read);
        disk->set_sectors_written(curr.sectors_written);
        disk->set_read_time_ms(curr.read_time_ms);
        disk->set_write_time_ms(curr.write_time_ms);
        disk->set_io_in_progress(curr.io_in_progress);
        disk->set_io_time_ms(curr.io_time_ms);
        disk->set_weighted_io_time_ms(curr.weighted_io_time_ms);

        // 速率/变化率计算
        auto it = lastSamples.find(name);
        double dt = now - lastTime[name];
        if (it != lastSamples.end() && dt > 0) {
            const auto &last = it->second;
            double read_ios = counterDelta(curr.reads, last.reads);
            double write_ios = counterDelta(curr.writes, last.writes);
            double read_bytes = counterDelta(curr.sectors_read, last.sectors_read) * 512.0;
            double write_bytes = counterDelta(curr.sectors_written, last.sectors_written) * 512.0;
            double read_time = counterDelta(curr.read_time_ms, last.read_time_ms);
            double write_time = counterDelta(curr.write_time_ms, last.write_time_ms);
            double io_time = counterDelta(curr.io_time_ms, last.io_time_ms);

            disk->set_read_bytes_per_sec(read_bytes / dt);
            disk->set_write_bytes_per_sec(write_bytes / dt);
            disk->set_read_iops(read_ios / dt);
            disk->set_write_iops(write_ios / dt);
            disk->set_avg_read_latency_ms(read_ios > 0 ? read_time / read_ios : 0);
            disk->set_avg_write_latency_ms(write_ios > 0 ? write_time / write_ios : 0);
            disk->set_util_percent(io_time / (dt * 10.0)); // io_time 单位 ms
        } else {
            disk->set_read_bytes_per_sec(0);
            disk->set_write_bytes_per_sec(0);
            disk->set_read_iops(0);
            disk->set_write_iops(0);
            disk->set_avg_read_latency_ms(0);
            disk->set_avg_write_latency_ms(0);
            disk->set_util_percent(0);
        }
        lastSamples[name] = curr;
        lastTime[name] = now;
    }
}

} // namespace monitor
