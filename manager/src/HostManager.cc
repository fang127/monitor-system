#include "HostManager.h"

#include "MysqlConfig.h"
#include "mysql.h"
#include <sys/types.h>
#include <chrono>
#include <mutex>
#include <random>
#include <stdexcept>
#include <thread>

namespace monitor {
#ifdef ENABLE_MYSQL
namespace {
/**
 * @brief         net recv/send bytes/packets rate and error/drop count
 */
struct NetDetailedSample {
    float net_recv_bytes_rate = 0.0f;
    float net_send_bytes_rate = 0.0f;
    float net_recv_packets_rate = 0.0f;
    float net_send_packets_rate = 0.0f;
    uint64_t err_in = 0;
    uint64_t err_out = 0;
    uint64_t drop_in = 0;
    uint64_t drop_out = 0;
};

/**
 * @brief         softirq time percentage in each category, including hi, timer,
 * net_tx, net_rx, block, irq_poll, tasklet, sched, hrtimer and rcu
 *
 */
struct SoftIrqSample {
    float hi = 0, timer = 0, net_tx = 0, net_rx = 0, block = 0;
    float irq_poll = 0, tasklet = 0, sched = 0, hrtimer = 0, rcu = 0;
};

/**
 * @brief         memory usage and details, including total, free, available,
 * buffers, cached, swap cached, active, inactive, active anon, inactive anon,
 * active file, inactive file, dirty, writeback, anon pages, mapped,
 * kreclaimable, sreclaimable and sunreclaim
 *
 */
struct MemDetailSample {
    float total = 0, free = 0, avail = 0, buffers = 0, cached = 0;
    float swap_cached = 0, active = 0, inactive = 0;
    float active_anon = 0, inactive_anon = 0, active_file = 0,
          inactive_file = 0;
    float dirty = 0, writeback = 0, anon_pages = 0, mapped = 0;
    float kreclaimable = 0, sreclaimable = 0, sunreclaim = 0;
};

/**
 * @brief         disk read/write bytes/IOPS rate, average read/write latency
 * and disk utilization percentage
 *
 */
struct DiskDetailSample {
    float read_bytes_per_sec = 0;
    float write_bytes_per_sec = 0;
    float read_iops = 0;
    float write_iops = 0;
    float avg_read_latency_ms = 0;
    float avg_write_latency_ms = 0;
    float util_percent = 0;
};

// store the last samples for each host, used to calculate the rate and
// percentage
std::map<std::string, std::map<std::string, NetDetailedSample>> lastNetSamples;
std::map<std::string, std::map<std::string, SoftIrqSample>> lastSoftirqSamples;
std::map<std::string, MemDetailSample> lastMemSamples;
std::map<std::string, std::map<std::string, DiskDetailSample>> lastDiskSamples;

} // namespace
#endif

/**
 * @brief         network recv/send bytes and packets rate, and error/drop count
 * for each network interface, including eth0, eth1, lo, etc.
 *
 */
struct NetSample {
    double last_in_bytes = 0;
    double last_out_bytes = 0;
    std::chrono::system_clock::time_point last_time;
};
static std::map<std::string, NetSample> netSamples;

/**
 * @brief         Performance metrics for computing the rate
 *
 */
struct PerfSample {
    float cpu_percent = 0, usr_percent = 0, system_percent = 0;
    float nice_percent = 0, idle_percent = 0, io_wait_percent = 0;
    float irq_percent = 0, soft_irq_percent = 0;
    float steal_percent = 0, guest_percent = 0, guest_nice_percent = 0;
    float load_avg_1 = 0, load_avg_3 = 0, load_avg_15 = 0;
    float mem_used_percent = 0, mem_total = 0, mem_free = 0, mem_avail = 0;
    float net_in_rate = 0, net_out_rate = 0;
    float score = 0;
};
static std::map<std::string, PerfSample> lastPerfSamples;

HostManager::HostManager() : running_(false) {}

HostManager::~HostManager() {
    running_.store(false);
    if (thread_ && thread_->joinable()) thread_->join();
    stop();
}

void HostManager::processLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(60));
        auto now = std::chrono::system_clock::now();
        // Remove hosts that haven't been updated for more than 60 seconds
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = hostScores_.begin(); it != hostScores_.end();) {
            auto age = std::chrono::duration_cast<std::chrono::seconds>(
                           now - it->second.timestamp)
                           .count();
            if (age > 60) {
                std::cout << "Removing stale host: " << it->first << std::endl;
                it = hostScores_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

double HostManager::calculateScore(const monitor::proto::MonitorInfo &info) {
    // High coroutine background score weights
    const double cpuWeight = 0.35;
    const double memWeight = 0.30;
    const double loadWeight = 0.15;
    const double diskWeight = 0.15;
    const double netWeight = 0.05;

    const double loadCoefficient = 1.5; // I/O intensive scenario coefficient
    const double maxBandWidth = 125000000.0; // 1 Gbps in bytes per second

    double cpuPercent = 0, loadAvg1 = 0, memPercent = 0;
    double netRecvRate = 0, netSendRate = 0, diskUtil = 0;
    int cpuCores = 1;

    // Calculate CPU usage percentage and number of CPU cores
    if (info.cpu_stat_size() > 0) {
        cpuPercent = info.cpu_stat(0).cpu_percent();
        cpuCores = info.cpu_stat_size() - 1;
        if (cpuCores < 1) cpuCores = 1;
    }

    // Calculate load average
    if (info.has_cpu_load()) loadAvg1 = info.cpu_load().load_avg_1();

    // Calculate memory usage percentage
    if (info.has_mem_info()) memPercent = info.mem_info().used_percent();

    // Calculate network rates
    if (info.net_info_size() > 0) {
        netRecvRate = info.net_info(0).rcv_rate();
        netSendRate = info.net_info(0).send_rate();
    }

    // Calculate disk utilization
    if (info.disk_info_size() > 0) {
        for (int i = 0; i < info.disk_info_size(); ++i) {
            double util = info.disk_info(i).util_percent();
            if (util > diskUtil) diskUtil = util;
        }
    }

    // Calculate the overall score using weighted sum
    auto clamp = [](double value) {
        return value < 0 ? 0 : (value > 1 ? 1 : value);
    };
    double cpuScore = clamp(1.0 - cpuPercent / 100);
    double memScore = clamp(1.0 - memPercent / 100);
    double loadScore = clamp(1.0 - loadAvg1 / (cpuCores * loadCoefficient));
    double diskScore = clamp(1.0 - diskUtil / 100);
    double netRecvScore = clamp(1.0 - netRecvRate / maxBandWidth);
    double netSendScore = clamp(1.0 - netSendRate / maxBandWidth);
    double netScore = (netRecvScore + netSendScore) / 2;
    double score = cpuWeight * cpuScore + memWeight * memScore +
                   loadWeight * loadScore + diskWeight * diskScore +
                   netWeight * netScore;
    score *= 100; // Scale to 0-100
    return score < 0 ? 0 : (score > 100 ? 100 : score);
}

void HostManager::writeToMysql(HostMonitoringData &data) {
#ifdef ENABLE_MYSQL
    const MysqlConfig mysqlConfig = loadMysqlConfigFromEnv();
    MYSQL *conn = mysql_init(nullptr);
    if (!conn) {
        std::cerr << "MySQL initialization failed" << std::endl;
        return;
    }

    if (!mysql_real_connect(
            conn, mysqlConfig.host.c_str(), mysqlConfig.user.c_str(),
            mysqlConfig.password.c_str(), mysqlConfig.database.c_str(),
            mysqlConfig.port, nullptr, 0)) {
        std::cerr << "MySQL connection failed: " << mysql_error(conn)
                  << std::endl;
        mysql_close(conn);
        return;
    }

    // convert timestamp to string
    std::time_t t =
        std::chrono::system_clock::to_time_t(data.host_score.timestamp);
    std::tm tm = *std::localtime(&t);
    char timeStr[32];
    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tm);

    const auto &info = data.host_score.info;
    auto rate = [](float nowVal, float lastVal) {
        if (lastVal == 0) return 0.0f;
        return (nowVal - lastVal) / lastVal; // calculate rate of change
    };

    // Insert data into MySQL main table
    {
        float total = 0, freeMem = 0, avail = 0, sendRate = 0, rcvRate = 0;
        float cpuPercent = 0, usrPercent = 0, systemPercent = 0;
        float nicePercent = 0, idlePercent = 0, ioWaitPercent = 0;
        float irqPercent = 0, softIrqPercent = 0;
        float loadAvg1 = 0, loadAvg3 = 0, loadAvg15 = 0, memUsedPercent = 0;
        float diskUtilPercent = 0;

        if (info.has_mem_info()) {
            total = info.mem_info().total();
            freeMem = info.mem_info().free();
            avail = info.mem_info().avail();
            memUsedPercent = info.mem_info().used_percent();
        }
        if (info.net_info_size() > 0) {
            sendRate = info.net_info(0).send_rate() / 1024.0;
            rcvRate = info.net_info(0).rcv_rate() / 1024.0;
        }
        if (info.cpu_stat_size() > 0) {
            const auto &cpu = info.cpu_stat(0);
            cpuPercent = cpu.cpu_percent();
            usrPercent = cpu.usr_percent();
            systemPercent = cpu.system_percent();
            nicePercent = cpu.nice_percent();
            idlePercent = cpu.idle_percent();
            ioWaitPercent = cpu.io_wait_percent();
            irqPercent = cpu.irq_percent();
            softIrqPercent = cpu.soft_irq_percent();
        }
        if (info.has_cpu_load()) {
            loadAvg1 = info.cpu_load().load_avg_1();
            loadAvg3 = info.cpu_load().load_avg_3();
            loadAvg15 = info.cpu_load().load_avg_15();
        }
        // get disk utilization percentage
        for (int i = 0; i < info.disk_info_size(); ++i) {
            float util = info.disk_info(i).util_percent();
            if (util > diskUtilPercent) diskUtilPercent = util;
        }

        // compute disk utilization percentage rate
        static std::map<std::string, float> lastDiskUtil;
        float diskUtilPercentRate = 0;
        std::string hostName = data.host_name;
        if (lastDiskUtil.count(hostName) && lastDiskUtil[hostName] != 0) {
            diskUtilPercentRate = (diskUtilPercent - lastDiskUtil[hostName]) /
                                  lastDiskUtil[hostName];
        }
        lastDiskUtil[hostName] = diskUtilPercent;

        std::ostringstream oss;
        oss << "INSERT INTO server_performance "
            << "(server_name, cpu_percent, usr_percent, system_percent, "
               "nice_percent, "
            << "idle_percent, io_wait_percent, irq_percent, soft_irq_percent, "
            << "load_avg_1, load_avg_3, load_avg_15, "
            << "mem_used_percent, total, free, avail, "
            << "disk_util_percent, send_rate, rcv_rate, score, "
            << "cpu_percent_rate, usr_percent_rate, system_percent_rate, "
            << "nice_percent_rate, idle_percent_rate, io_wait_percent_rate, "
            << "irq_percent_rate, soft_irq_percent_rate, "
            << "load_avg_1_rate, load_avg_3_rate, load_avg_15_rate, "
            << "mem_used_percent_rate, total_rate, free_rate, avail_rate, "
            << "disk_util_percent_rate, send_rate_rate, rcv_rate_rate, "
               "timestamp) VALUES ('"
            << data.host_name << "'," << cpuPercent << "," << usrPercent << ","
            << systemPercent << "," << nicePercent << "," << idlePercent << ","
            << ioWaitPercent << "," << irqPercent << "," << softIrqPercent
            << "," << loadAvg1 << "," << loadAvg3 << "," << loadAvg15 << ","
            << memUsedPercent << "," << total << "," << freeMem << "," << avail
            << "," << diskUtilPercent << "," << sendRate << "," << rcvRate
            << "," << data.host_score.score << "," << data.cpu_percent_rate
            << "," << data.usr_percent_rate << "," << data.system_percent_rate
            << "," << data.nice_percent_rate << "," << data.idle_percent_rate
            << "," << data.io_wait_percent_rate << "," << data.irq_percent_rate
            << "," << data.soft_irq_percent_rate << "," << data.load_avg_1_rate
            << "," << data.load_avg_3_rate << "," << data.load_avg_15_rate
            << "," << data.mem_used_percent_rate << "," << data.mem_total_rate
            << "," << data.mem_free_rate << "," << data.mem_avail_rate << ","
            << diskUtilPercentRate << "," << data.net_in_rate_rate << ","
            << data.net_out_rate_rate << ",'" << timeStr << "')";
        // execute the query
        mysql_query(conn, oss.str().c_str());
        // check for errors
        if (mysql_errno(conn)) {
            std::cerr << "MySQL insert error: " << mysql_error(conn)
                      << std::endl;
            std::cerr << __func__ << " " << __LINE__ << std::endl;
        }
    }

    // insert net detail data into mysql
    {
        for (int i = 0; i < info.net_info_size(); ++i) {
            const auto &net = info.net_info(i);
            std::string netName = net.name();

            NetDetailedSample curr;
            curr.net_recv_bytes_rate = net.rcv_rate();
            curr.net_recv_packets_rate = net.rcv_packets_rate();
            curr.net_send_bytes_rate = net.send_rate();
            curr.net_send_packets_rate = net.send_packets_rate();
            curr.err_in = net.err_in();
            curr.err_out = net.err_out();
            curr.drop_in = net.drop_in();
            curr.drop_out = net.drop_out();

            NetDetailedSample &last = lastNetSamples[data.host_name][netName];

            // 计算错误/丢弃变化率
            auto rateU64 = [](uint64_t nowVal, uint64_t lastVal) -> float {
                if (lastVal == 0) return 0;
                return static_cast<float>(nowVal - lastVal) /
                       static_cast<float>(lastVal);
            };

            std::ostringstream oss;
            oss << "INSERT INTO server_net_detail "
                << "(server_name, net_name, err_in, err_out, drop_in, "
                   "drop_out, "
                << "rcv_bytes_rate, rcv_packets_rate, snd_bytes_rate, "
                   "snd_packets_rate, "
                << "rcv_bytes_rate_rate, rcv_packets_rate_rate, "
                << "snd_bytes_rate_rate, snd_packets_rate_rate, "
                << "err_in_rate, err_out_rate, drop_in_rate, drop_out_rate, "
                << "timestamp) VALUES ('" << data.host_name << "','" << netName
                << "'," << curr.err_in << "," << curr.err_out << ","
                << curr.drop_in << "," << curr.drop_out << ","
                << curr.net_recv_bytes_rate << "," << curr.net_recv_packets_rate
                << "," << curr.net_send_bytes_rate << ","
                << curr.net_send_packets_rate << ","
                << rate(curr.net_recv_bytes_rate, last.net_recv_bytes_rate)
                << ","
                << rate(curr.net_recv_packets_rate, last.net_recv_packets_rate)
                << ","
                << rate(curr.net_send_bytes_rate, last.net_send_bytes_rate)
                << ","
                << rate(curr.net_send_packets_rate, last.net_send_packets_rate)
                << "," << rateU64(curr.err_in, last.err_in) << ","
                << rateU64(curr.err_out, last.err_out) << ","
                << rateU64(curr.drop_in, last.drop_in) << ","
                << rateU64(curr.drop_out, last.drop_out) << ",'" << timeStr
                << "')";
            mysql_query(conn, oss.str().c_str());
            if (mysql_errno(conn)) {
                std::cerr << "MySQL insert error: " << mysql_error(conn)
                          << std::endl;
                std::cerr << __func__ << " " << __LINE__ << std::endl;
            }
            last = curr;
        }
    }

    // insert softirq data into mysql
    {
        for (int i = 0; i < info.soft_irq_size(); ++i) {
            const auto &sirq = info.soft_irq(i);
            std::string cpuName = sirq.cpu();

            SoftIrqSample curr;
            curr.hi = sirq.hi();
            curr.timer = sirq.timer();
            curr.net_tx = sirq.net_tx();
            curr.net_rx = sirq.net_rx();
            curr.block = sirq.block();
            curr.irq_poll = sirq.irq_poll();
            curr.tasklet = sirq.tasklet();
            curr.sched = sirq.sched();
            curr.hrtimer = sirq.hrtimer();
            curr.rcu = sirq.rcu();

            SoftIrqSample &last = lastSoftirqSamples[data.host_name][cpuName];

            std::ostringstream oss;
            oss << "INSERT INTO server_softirq_detail "
                << "(server_name, cpu_name, hi, timer, net_tx, net_rx, block, "
                << "irq_poll, tasklet, sched, hrtimer, rcu, "
                << "hi_rate, timer_rate, net_tx_rate, net_rx_rate, block_rate, "
                << "irq_poll_rate, tasklet_rate, sched_rate, hrtimer_rate, "
                   "rcu_rate, "
                << "timestamp) VALUES ('" << data.host_name << "','" << cpuName
                << "'," << curr.hi << "," << curr.timer << "," << curr.net_tx
                << "," << curr.net_rx << "," << curr.block << ","
                << curr.irq_poll << "," << curr.tasklet << "," << curr.sched
                << "," << curr.hrtimer << "," << curr.rcu << ","
                << rate(curr.hi, last.hi) << "," << rate(curr.timer, last.timer)
                << "," << rate(curr.net_tx, last.net_tx) << ","
                << rate(curr.net_rx, last.net_rx) << ","
                << rate(curr.block, last.block) << ","
                << rate(curr.irq_poll, last.irq_poll) << ","
                << rate(curr.tasklet, last.tasklet) << ","
                << rate(curr.sched, last.sched) << ","
                << rate(curr.hrtimer, last.hrtimer) << ","
                << rate(curr.rcu, last.rcu) << ",'" << timeStr << "')";
            mysql_query(conn, oss.str().c_str());

            if (mysql_errno(conn)) {
                std::cerr << "MySQL insert error: " << mysql_error(conn)
                          << std::endl;
                std::cerr << __func__ << " " << __LINE__ << std::endl;
            }
            last = curr;
        }
    }

    // insert memory detail data into mysql
    {
        if (info.has_mem_info()) {
            const auto &mem = info.mem_info();

            MemDetailSample curr;
            curr.total = mem.total();
            curr.free = mem.free();
            curr.avail = mem.avail();
            curr.buffers = mem.buffers();
            curr.cached = mem.cached();
            curr.swap_cached = mem.swap_cached();
            curr.active = mem.active();
            curr.inactive = mem.inactive();
            curr.active_anon = mem.active_anon();
            curr.inactive_anon = mem.inactive_anon();
            curr.active_file = mem.active_file();
            curr.inactive_file = mem.inactive_file();
            curr.dirty = mem.dirty();
            curr.writeback = mem.writeback();
            curr.anon_pages = mem.anon_pages();
            curr.mapped = mem.mapped();
            curr.kreclaimable = mem.kreclaimable();
            curr.sreclaimable = mem.sreclaimable();
            curr.sunreclaim = mem.sunreclaim();

            MemDetailSample &last = lastMemSamples[data.host_name];

            std::ostringstream oss;
            oss << "INSERT INTO server_mem_detail "
                << "(server_name, total, free, avail, buffers, cached, "
                   "swap_cached, "
                << "active, inactive, active_anon, inactive_anon, active_file, "
                   "inactive_file, "
                << "dirty, writeback, anon_pages, mapped, kreclaimable, "
                   "sreclaimable, sunreclaim, "
                << "total_rate, free_rate, avail_rate, buffers_rate, "
                   "cached_rate, swap_cached_rate, "
                << "active_rate, inactive_rate, active_anon_rate, "
                   "inactive_anon_rate, "
                << "active_file_rate, inactive_file_rate, dirty_rate, "
                   "writeback_rate, "
                << "anon_pages_rate, mapped_rate, kreclaimable_rate, "
                   "sreclaimable_rate, "
                << "sunreclaim_rate, timestamp) VALUES ('" << data.host_name
                << "'," << curr.total << "," << curr.free << "," << curr.avail
                << "," << curr.buffers << "," << curr.cached << ","
                << curr.swap_cached << "," << curr.active << ","
                << curr.inactive << "," << curr.active_anon << ","
                << curr.inactive_anon << "," << curr.active_file << ","
                << curr.inactive_file << "," << curr.dirty << ","
                << curr.writeback << "," << curr.anon_pages << ","
                << curr.mapped << "," << curr.kreclaimable << ","
                << curr.sreclaimable << "," << curr.sunreclaim << ","
                << rate(curr.total, last.total) << ","
                << rate(curr.free, last.free) << ","
                << rate(curr.avail, last.avail) << ","
                << rate(curr.buffers, last.buffers) << ","
                << rate(curr.cached, last.cached) << ","
                << rate(curr.swap_cached, last.swap_cached) << ","
                << rate(curr.active, last.active) << ","
                << rate(curr.inactive, last.inactive) << ","
                << rate(curr.active_anon, last.active_anon) << ","
                << rate(curr.inactive_anon, last.inactive_anon) << ","
                << rate(curr.active_file, last.active_file) << ","
                << rate(curr.inactive_file, last.inactive_file) << ","
                << rate(curr.dirty, last.dirty) << ","
                << rate(curr.writeback, last.writeback) << ","
                << rate(curr.anon_pages, last.anon_pages) << ","
                << rate(curr.mapped, last.mapped) << ","
                << rate(curr.kreclaimable, last.kreclaimable) << ","
                << rate(curr.sreclaimable, last.sreclaimable) << ","
                << rate(curr.sunreclaim, last.sunreclaim) << ",'" << timeStr
                << "')";
            mysql_query(conn, oss.str().c_str());

            if (mysql_errno(conn)) {
                std::cerr << "MySQL insert error: " << mysql_error(conn)
                          << std::endl;
                std::cerr << __func__ << " " << __LINE__ << std::endl;
            }
            last = curr;
        }
    }

    // insert disk detail data into mysql
    {
        for (int i = 0; i < info.disk_info_size(); ++i) {
            const auto &disk = info.disk_info(i);
            std::string diskName = disk.name();

            DiskDetailSample curr;
            curr.read_bytes_per_sec = disk.read_bytes_per_sec();
            curr.write_bytes_per_sec = disk.write_bytes_per_sec();
            curr.read_iops = disk.read_iops();
            curr.write_iops = disk.write_iops();
            curr.avg_read_latency_ms = disk.avg_read_latency_ms();
            curr.avg_write_latency_ms = disk.avg_write_latency_ms();
            curr.util_percent = disk.util_percent();

            DiskDetailSample &last = lastDiskSamples[data.host_name][diskName];

            std::ostringstream oss;
            oss << "INSERT INTO server_disk_detail "
                << "(server_name, disk_name, reads, writes, sectors_read, "
                   "sectors_written, "
                << "read_time_ms, write_time_ms, io_in_progress, io_time_ms, "
                   "weighted_io_time_ms, "
                << "read_bytes_per_sec, write_bytes_per_sec, read_iops, "
                   "write_iops, "
                << "avg_read_latency_ms, avg_write_latency_ms, util_percent, "
                << "read_bytes_per_sec_rate, write_bytes_per_sec_rate, "
                   "read_iops_rate, write_iops_rate, "
                << "avg_read_latency_ms_rate, avg_write_latency_ms_rate, "
                   "util_percent_rate, "
                << "timestamp) VALUES ('" << data.host_name << "','" << diskName
                << "'," << disk.reads() << "," << disk.writes() << ","
                << disk.sectors_read() << "," << disk.sectors_written() << ","
                << disk.read_time_ms() << "," << disk.write_time_ms() << ","
                << disk.io_in_progress() << "," << disk.io_time_ms() << ","
                << disk.weighted_io_time_ms() << "," << curr.read_bytes_per_sec
                << "," << curr.write_bytes_per_sec << "," << curr.read_iops
                << "," << curr.write_iops << "," << curr.avg_read_latency_ms
                << "," << curr.avg_write_latency_ms << "," << curr.util_percent
                << "," << rate(curr.read_bytes_per_sec, last.read_bytes_per_sec)
                << ","
                << rate(curr.write_bytes_per_sec, last.write_bytes_per_sec)
                << "," << rate(curr.read_iops, last.read_iops) << ","
                << rate(curr.write_iops, last.write_iops) << ","
                << rate(curr.avg_read_latency_ms, last.avg_read_latency_ms)
                << ","
                << rate(curr.avg_write_latency_ms, last.avg_write_latency_ms)
                << "," << rate(curr.util_percent, last.util_percent) << ",'"
                << timeStr << "')";
            mysql_query(conn, oss.str().c_str());

            if (mysql_errno(conn)) {
                std::cerr << "MySQL insert error: " << mysql_error(conn)
                          << std::endl;
                std::cerr << __func__ << " " << __LINE__ << std::endl;
            }
            last = curr;
        }
    }

    mysql_close(conn);
#else
    (void)data; // avoid unused parameter warning
#endif
}

void HostManager::start() {
    running_.store(true);
    thread_ = std::make_unique<std::thread>(&HostManager::processLoop, this);
}

void HostManager::stop() {
    running_.store(false);
    if (thread_ && thread_->joinable()) thread_->join();
}

void HostManager::onDataReceived(const monitor::proto::MonitorInfo &info) {
    // create unique host ID, can be replaced by real hostname in production
    std::string hostID;
    if (info.has_host_info()) {
        const auto &hostInfo = info.host_info();
        std::string hostName = hostInfo.hostname();
        std::string ip = hostInfo.ip_address();
        if (!hostName.empty() && !ip.empty())
            hostID = hostName + "_" + ip;
        else if (!ip.empty())
            hostID = ip;
        else if (!hostName.empty())
            hostID = hostName;
    }

    // Compatible with older versions
    if (hostID.empty()) hostID = info.name();

    // if hostID is still empty, drop this data
    if (hostID.empty()) {
        std::cerr << "Received data with empty server identifier" << std::endl;
        return;
    }

    double score = calculateScore(info);
    auto now = std::chrono::system_clock::now();

    // compute rate of change for network in/out rate
    double netInRate = 0, netOutRate = 0;
    if (info.net_info_size() > 0) {
        netInRate =
            info.net_info(0).rcv_rate() / (1024.0 * 1024.0); // convert to MB/s
        netOutRate =
            info.net_info(0).send_rate() / (1024.0 * 1024.0); // convert to MB/s
    }

    // store the current performance metrics for rate calculation in the next
    // round
    PerfSample curr;
    if (info.cpu_stat_size() > 0) {
        const auto &cpu = info.cpu_stat(0);
        curr.cpu_percent = cpu.cpu_percent();
        curr.usr_percent = cpu.usr_percent();
        curr.system_percent = cpu.system_percent();
        curr.nice_percent = cpu.nice_percent();
        curr.idle_percent = cpu.idle_percent();
        curr.io_wait_percent = cpu.io_wait_percent();
        curr.irq_percent = cpu.irq_percent();
        curr.soft_irq_percent = cpu.soft_irq_percent();
    }

    if (info.has_cpu_load()) {
        curr.load_avg_1 = info.cpu_load().load_avg_1();
        curr.load_avg_3 = info.cpu_load().load_avg_3();
        curr.load_avg_15 = info.cpu_load().load_avg_15();
    }

    if (info.has_mem_info()) {
        curr.mem_used_percent = info.mem_info().used_percent();
        curr.mem_total = info.mem_info().total();
        curr.mem_free = info.mem_info().free();
        curr.mem_avail = info.mem_info().avail();
    }

    curr.net_in_rate = netInRate;
    curr.net_out_rate = netOutRate;
    curr.score = score;

    // compute rate of change for performance metrics
    PerfSample &last = lastPerfSamples[hostID];
    auto rate = [](float nowVal, float lastVal) -> float {
        if (lastVal == 0) return 0;
        return (nowVal - lastVal) / lastVal; // calculate rate of change
    };

    float cpu_percent_rate = rate(curr.cpu_percent, last.cpu_percent);
    float usr_percent_rate = rate(curr.usr_percent, last.usr_percent);
    float system_percent_rate = rate(curr.system_percent, last.system_percent);
    float nice_percent_rate = rate(curr.nice_percent, last.nice_percent);
    float idle_percent_rate = rate(curr.idle_percent, last.idle_percent);
    float io_wait_percent_rate =
        rate(curr.io_wait_percent, last.io_wait_percent);
    float irq_percent_rate = rate(curr.irq_percent, last.irq_percent);
    float soft_irq_percent_rate =
        rate(curr.soft_irq_percent, last.soft_irq_percent);
    float load_avg_1_rate = rate(curr.load_avg_1, last.load_avg_1);
    float load_avg_3_rate = rate(curr.load_avg_3, last.load_avg_3);
    float load_avg_15_rate = rate(curr.load_avg_15, last.load_avg_15);
    float mem_used_percent_rate =
        rate(curr.mem_used_percent, last.mem_used_percent);
    float mem_total_rate = rate(curr.mem_total, last.mem_total);
    float mem_free_rate = rate(curr.mem_free, last.mem_free);
    float mem_avail_rate = rate(curr.mem_avail, last.mem_avail);
    float net_in_rate_rate = rate(curr.net_in_rate, last.net_in_rate);
    float net_out_rate_rate = rate(curr.net_out_rate, last.net_out_rate);
    HostMonitoringData data{hostID,
                            HostScore{info, score, now},
                            curr.net_in_rate,
                            curr.net_out_rate,
                            cpu_percent_rate,
                            usr_percent_rate,
                            system_percent_rate,
                            nice_percent_rate,
                            idle_percent_rate,
                            io_wait_percent_rate,
                            irq_percent_rate,
                            soft_irq_percent_rate,
                            0,
                            0,
                            0,
                            load_avg_1_rate,
                            load_avg_3_rate,
                            load_avg_15_rate,
                            mem_used_percent_rate,
                            mem_total_rate,
                            mem_free_rate,
                            mem_avail_rate,
                            net_in_rate_rate,
                            net_out_rate_rate,
                            0,
                            0};
    lastPerfSamples[hostID] = curr;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        hostScores_[hostID] = HostScore{info, score, now};
    }

    writeToMysql(data);

    std::cout << "\n================== Received Data =================="
              << std::endl;
    std::cout << "Server: " << hostID << ", Score: " << score << std::endl;

    // CPU 详细信息
    std::cout << "\n--- CPU ---" << std::endl;
    std::cout << "  Usage: " << curr.cpu_percent << "%, "
              << "User: " << curr.usr_percent << "%, "
              << "System: " << curr.system_percent << "%" << std::endl;
    std::cout << "  Nice: " << curr.nice_percent << "%, "
              << "Idle: " << curr.idle_percent << "%, "
              << "IOWait: " << curr.io_wait_percent << "%" << std::endl;
    std::cout << "  IRQ: " << curr.irq_percent << "%, "
              << "SoftIRQ: " << curr.soft_irq_percent << "%" << std::endl;
    std::cout << "  Load: " << curr.load_avg_1 << "/" << curr.load_avg_3 << "/"
              << curr.load_avg_15 << std::endl;

    // 内存详细信息
    std::cout << "\n--- Memory ---" << std::endl;
    std::cout << "  Used: " << curr.mem_used_percent << "%, "
              << "Total: " << curr.mem_total << " MB" << std::endl;
    std::cout << "  Free: " << curr.mem_free << " MB, "
              << "Avail: " << curr.mem_avail << " MB" << std::endl;

    // 网络详细信息
    std::cout << "\n--- Network ---" << std::endl;
    std::cout << "  In: " << curr.net_in_rate * 1024 * 1024 << " B/s, "
              << "Out: " << curr.net_out_rate * 1024 * 1024 << " B/s"
              << std::endl;
    for (int i = 0; i < info.net_info_size(); ++i) {
        const auto &net = info.net_info(i);
        std::cout << "  [" << net.name() << "] Recv: " << net.rcv_rate()
                  << " B/s, "
                  << "Send: " << net.send_rate() << " B/s, "
                  << "Drops: " << net.drop_in() << "/" << net.drop_out()
                  << std::endl;
    }

    // 磁盘详细信息
    std::cout << "\n--- Disk ---" << std::endl;
    float max_disk_util = 0;
    for (int i = 0; i < info.disk_info_size(); ++i) {
        const auto &disk = info.disk_info(i);
        std::cout << "  [" << disk.name() << "] "
                  << "Read: " << disk.read_bytes_per_sec() / 1024.0 << " KB/s, "
                  << "Write: " << disk.write_bytes_per_sec() / 1024.0
                  << " KB/s, "
                  << "Util: " << disk.util_percent() << "%" << std::endl;
        if (disk.util_percent() > max_disk_util)
            max_disk_util = disk.util_percent();
    }
    if (info.disk_info_size() == 0) std::cout << "  No disk data" << std::endl;

    // 软中断信息
    std::cout << "\n--- SoftIRQ ---" << std::endl;
    std::cout << "  CPU cores with softirq data: " << info.soft_irq_size()
              << std::endl;

    // 变化率信息
    std::cout << "\n--- Change Rates ---" << std::endl;
    std::cout << "  CPU: " << cpu_percent_rate * 100 << "%, "
              << "Mem: " << mem_used_percent_rate * 100 << "%, "
              << "Load: " << load_avg_1_rate * 100 << "%" << std::endl;
    std::cout << "  NetIn: " << net_in_rate_rate * 100 << "%, "
              << "NetOut: " << net_out_rate_rate * 100 << "%" << std::endl;

    std::cout << "\n--- Database ---" << std::endl;
#ifdef ENABLE_MYSQL
    const MysqlConfig mysqlConfig = loadMysqlConfigFromEnv();
    std::cout << "  Data saved to MySQL (" << mysqlConfig.database << ")"
              << std::endl;
#else
    std::cout << "  MySQL support is disabled" << std::endl;
#endif
    std::cout << "====================================================\n"
              << std::endl;
}

std::unordered_map<std::string, HostScore> HostManager::getAllHostScores() {
    std::lock_guard<std::mutex> lock(mutex_);
    return hostScores_;
}

std::string HostManager::getBestHost() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string bestHost;
    double bestScore = INT_MIN;
    for (const auto &[host, data] : hostScores_) {
        if (data.score > bestScore) {
            bestScore = data.score;
            bestHost = host;
        }
    }
    return bestHost;
}
}; // namespace monitor
