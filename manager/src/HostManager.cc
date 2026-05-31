#include "HostManager.h"

#include "mysql.h"
#include <sys/types.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <map>
#include <mutex>
#include <thread>
#include <sstream>

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
    float active_anon = 0, inactive_anon = 0, active_file = 0, inactive_file = 0;
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

/**
 * @brief         MySQL 实例计数器采样，用于计算派生速率
 *
 */
struct MysqlDetailSample {
    uint64_t questions = 0;
    uint64_t com_commit = 0;
    uint64_t com_rollback = 0;
    uint64_t slow_queries = 0;
    uint64_t innodb_row_lock_waits = 0;
    std::chrono::system_clock::time_point timestamp;
    bool initialized = false;
};

/**
 * @brief         Redis 实例计数器采样，用于计算命令、网络和慢日志派生速率
 *
 */
struct RedisDetailSample {
    uint64_t total_commands_processed = 0;
    uint64_t total_net_input_bytes = 0;
    uint64_t total_net_output_bytes = 0;
    uint64_t slowlog_len = 0; // 慢日志长度
    std::chrono::system_clock::time_point timestamp;
    bool initialized = false;
};

// 保存每台主机的上一次采样，用于计算速率和变化率
std::map<std::string, std::map<std::string, NetDetailedSample>> lastNetSamples;
std::map<std::string, std::map<std::string, SoftIrqSample>> lastSoftirqSamples;
std::map<std::string, MemDetailSample> lastMemSamples;
std::map<std::string, std::map<std::string, DiskDetailSample>> lastDiskSamples;
std::map<std::string, std::map<std::string, MysqlDetailSample>> lastMysqlSamples;
std::map<std::string, std::map<std::string, RedisDetailSample>> lastRedisSamples;

/**
 * @brief         转义字符串并包裹单引号，确保可安全拼接到 MySQL 查询中，避免 SQL 注入
 *
 * @param         conn MySQL 连接
 * @param         value 待转义字符串
 * @return        转义后的 SQL 字符串字面量
 */
std::string quoteSqlString(MYSQL *conn, const std::string &value) {
    std::string escaped(value.size() * 2 + 1,
                        '\0'); // 为转义结果预留足够空间
    unsigned long len = mysql_real_escape_string(conn, escaped.data(), value.data(),
                                                 static_cast<unsigned long>(value.size())); // 执行字符串转义
    escaped.resize(len);
    return "'" + escaped + "'";
}

} // namespace
#endif

namespace {
/**
 * @brief
 * 选择用于计算分数的CPU统计数据，优先选择名为"all"的统计数据，如果没有则选择名为"cpu"的统计数据，如果仍然没有，则选择第一个CPU统计数据。
 *
 * @param         info 监控数据
 * @return        可用于评分的 CPU 统计数据指针，缺失时返回 nullptr
 */
const monitor::proto::CpuStat *selectAggregateCpuStat(const monitor::proto::MonitorInfo &info) {
    const monitor::proto::CpuStat *legacyCpu = nullptr;
    for (int i = 0; i < info.cpu_stat_size(); ++i) {
        const auto &cpu = info.cpu_stat(i);
        if (cpu.cpu_name() == "all") return &cpu;
        if (!legacyCpu && cpu.cpu_name() == "cpu") legacyCpu = &cpu;
    }
    if (legacyCpu) return legacyCpu;
    return info.cpu_stat_size() > 0 ? &info.cpu_stat(0) : nullptr;
}

/**
 * @brief         判断CPU统计数据的名称是否符合每核统计的命名规则，即以"cpu"开头，后面跟数字，例如"cpu0"、"cpu1"等。
 *
 * @param         name CPU 名称
 * @return        是每核 CPU 名称返回 true，否则返回 false
 */
bool isPerCoreCpuName(const std::string &name) {
    if (name.size() <= 3 || name.compare(0, 3, "cpu") != 0) return false;
    return std::all_of(name.begin() + 3, name.end(), [](unsigned char ch) { return std::isdigit(ch); });
}

/**
 * @brief
 * 计算CPU核心数量的方法是遍历所有的CPU统计数据，检查每个统计数据的名称是否符合每核统计的命名规则，如果符合则计数器加1。最后返回计数器的值，如果没有找到任何符合条件的统计数据，则默认返回1，表示至少有一个CPU核心。
 *
 * @param         info 监控数据
 * @return        CPU 核心数量
 */
int countCpuCores(const monitor::proto::MonitorInfo &info) {
    int cores = 0;
    for (int i = 0; i < info.cpu_stat_size(); ++i) {
        if (isPerCoreCpuName(info.cpu_stat(i).cpu_name())) ++cores;
    }
    return cores > 0 ? cores : 1;
}
} // namespace

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
// 保存每个主机的上一次性能指标样本，用于计算变化率
static std::map<std::string, PerfSample> lastPerfSamples;
static std::mutex lastPerfSamplesMutex;

HostManager::HostManager() : running_(false) {}

HostManager::~HostManager() { stop(); }

void HostManager::configure(const ManagerConfig &config, ManagerMetrics *metrics, MysqlConnectionPool *mysqlWritePool,
                            RedisCache *redisCache) {
    config_ = config;
    metrics_ = metrics;
    mysqlWritePool_ = mysqlWritePool;
    redisCache_ = redisCache;
}

void HostManager::processLoop() {
    // 每60秒检查一次，移除超过60秒未更新的主机数据
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(60));
        auto now = std::chrono::system_clock::now();
        // Remove hosts that haven't been updated for more than 60 seconds
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = hostScores_.begin(); it != hostScores_.end();) {
            auto age = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.timestamp).count();
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
    // 高并发后台场景的评分权重
    const double cpuWeight = 0.35;
    const double memWeight = 0.30;
    const double loadWeight = 0.15;
    const double diskWeight = 0.15;
    const double netWeight = 0.05;

    const double loadCoefficient = 1.5;      // I/O 密集场景系数
    const double maxBandWidth = 125000000.0; // 1 Gbps，单位 bytes/s

    double cpuPercent = 0, loadAvg1 = 0, memPercent = 0;
    double netRecvRate = 0, netSendRate = 0, diskUtil = 0;
    int cpuCores = 1;

    // 计算 CPU 使用率和 CPU 核心数量
    if (const auto *cpu = selectAggregateCpuStat(info)) {
        cpuPercent = cpu->cpu_percent();
        cpuCores = countCpuCores(info);
    }

    // 计算平均负载
    if (info.has_cpu_load()) loadAvg1 = info.cpu_load().load_avg_1();

    // 计算内存使用率
    if (info.has_mem_info()) memPercent = info.mem_info().used_percent();

    // 计算网络速率
    if (info.net_info_size() > 0) {
        netRecvRate = info.net_info(0).rcv_rate();
        netSendRate = info.net_info(0).send_rate();
    }

    // 计算磁盘利用率
    if (info.disk_info_size() > 0) {
        for (int i = 0; i < info.disk_info_size(); ++i) {
            double util = info.disk_info(i).util_percent();
            if (util > diskUtil) diskUtil = util;
        }
    }

    // 使用加权求和计算综合评分
    auto clamp = [](double value) { return value < 0 ? 0 : (value > 1 ? 1 : value); };
    double cpuScore = clamp(1.0 - cpuPercent / 100);
    double memScore = clamp(1.0 - memPercent / 100);
    double loadScore = clamp(1.0 - loadAvg1 / (cpuCores * loadCoefficient));
    double diskScore = clamp(1.0 - diskUtil / 100);
    double netRecvScore = clamp(1.0 - netRecvRate / maxBandWidth);
    double netSendScore = clamp(1.0 - netSendRate / maxBandWidth);
    double netScore = (netRecvScore + netSendScore) / 2;
    double score = cpuWeight * cpuScore + memWeight * memScore + loadWeight * loadScore + diskWeight * diskScore +
                   netWeight * netScore;
    score *= 100; // 缩放到 0-100
    return score < 0 ? 0 : (score > 100 ? 100 : score);
}

void HostManager::writeToMysql(HostMonitoringData &data) {
#ifdef ENABLE_MYSQL
    // 检查MySQL写入连接池是否配置
    if (!mysqlWritePool_) {
        std::cerr << "HostManager: MySQL write pool is not configured" << std::endl;
        if (metrics_) metrics_->mysql_errors.fetch_add(1);
        return;
    }
    // 从连接池获取一个MySQL连接，设置超时时间为配置中的mysql_read_timeout
    MysqlConnectionPool::Guard pooledConn = mysqlWritePool_->acquire(config_.mysql_read_timeout);
    if (!pooledConn) {
        if (metrics_) metrics_->pool_timeouts.fetch_add(1);
        return;
    }
    MYSQL *conn = pooledConn.get();

    // 转换时间戳为可读格式并进行SQL转义
    std::time_t t = std::chrono::system_clock::to_time_t(data.host_score.timestamp);
    std::tm tm = *std::localtime(&t);
    char timeStr[32];
    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tm);

    // 检查监控数据是否包含必要的范围信息（租户ID、团队ID、集群ID），如果缺失则记录错误并丢弃该数据
    if (data.scope.tenant_id.empty() || data.scope.team_id.empty() || data.scope.cluster_id.empty()) {
        std::cerr << "HostManager: monitor data has no explicit tenant, team or cluster scope" << std::endl;
        if (metrics_) metrics_->dropped_monitor_samples.fetch_add(1);
        return;
    }
    const std::string tenantIDSql = quoteSqlString(conn, data.scope.tenant_id);
    const std::string teamIDSql = quoteSqlString(conn, data.scope.team_id);
    const std::string clusterIDSql = quoteSqlString(conn, data.scope.cluster_id);
    const std::string serverIDSql = data.scope.server_id == 0 ? "NULL" : std::to_string(data.scope.server_id);
    const std::string hostNameSql = quoteSqlString(conn, data.host_name); // 转义主机名以防 SQL 注入
    const std::string timestampSql = quoteSqlString(conn, timeStr);       // 转义时间戳字符串以防 SQL 注入

    const auto &info = data.host_score.info;
    auto rate = [](float nowVal, float lastVal) {
        if (lastVal == 0) return 0.0f;
        return (nowVal - lastVal) / lastVal; // 计算变化率
    };

    // 向 MySQL 主表插入数据
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
            sendRate = info.net_info(0).send_rate();
            rcvRate = info.net_info(0).rcv_rate();
        }
        if (const auto *cpu = selectAggregateCpuStat(info)) {
            cpuPercent = cpu->cpu_percent();
            usrPercent = cpu->usr_percent();
            systemPercent = cpu->system_percent();
            nicePercent = cpu->nice_percent();
            idlePercent = cpu->idle_percent();
            ioWaitPercent = cpu->io_wait_percent();
            irqPercent = cpu->irq_percent();
            softIrqPercent = cpu->soft_irq_percent();
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
            diskUtilPercentRate = (diskUtilPercent - lastDiskUtil[hostName]) / lastDiskUtil[hostName];
        }
        lastDiskUtil[hostName] = diskUtilPercent;

        std::ostringstream oss;
        oss << "INSERT INTO server_performance "
            << "(tenant_id, team_id, cluster_id, server_id, server_name, cpu_percent, usr_percent, system_percent, "
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
               "timestamp) VALUES ("
            << tenantIDSql << "," << teamIDSql << "," << clusterIDSql << "," << serverIDSql << "," << hostNameSql << ","
            << cpuPercent << "," << usrPercent << "," << systemPercent << "," << nicePercent << "," << idlePercent
            << "," << ioWaitPercent << "," << irqPercent << "," << softIrqPercent << "," << loadAvg1 << "," << loadAvg3
            << "," << loadAvg15 << "," << memUsedPercent << "," << total << "," << freeMem << "," << avail << ","
            << diskUtilPercent << "," << sendRate << "," << rcvRate << "," << data.host_score.score << ","
            << data.cpu_percent_rate << "," << data.usr_percent_rate << "," << data.system_percent_rate << ","
            << data.nice_percent_rate << "," << data.idle_percent_rate << "," << data.io_wait_percent_rate << ","
            << data.irq_percent_rate << "," << data.soft_irq_percent_rate << "," << data.load_avg_1_rate << ","
            << data.load_avg_3_rate << "," << data.load_avg_15_rate << "," << data.mem_used_percent_rate << ","
            << data.mem_total_rate << "," << data.mem_free_rate << "," << data.mem_avail_rate << ","
            << diskUtilPercentRate << "," << data.net_in_rate_rate << "," << data.net_out_rate_rate << ","
            << timestampSql << ")";
        // 执行写入语句
        mysql_query(conn, oss.str().c_str());
        // check for errors
        if (mysql_errno(conn)) {
            std::cerr << "MySQL insert error: " << mysql_error(conn) << std::endl;
            std::cerr << __func__ << " " << __LINE__ << std::endl;
        }
    }

    // insert net detail data into mysql
    {
        for (int i = 0; i < info.net_info_size(); ++i) {
            const auto &net = info.net_info(i);
            std::string netName = net.name();
            std::string netNameSql = quoteSqlString(conn, netName);

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
                return static_cast<float>(nowVal - lastVal) / static_cast<float>(lastVal);
            };

            std::ostringstream oss;
            oss << "INSERT INTO server_net_detail "
                << "(tenant_id, team_id, cluster_id, server_id, server_name, net_name, err_in, err_out, drop_in, "
                   "drop_out, "
                << "rcv_bytes_rate, rcv_packets_rate, snd_bytes_rate, "
                   "snd_packets_rate, "
                << "rcv_bytes_rate_rate, rcv_packets_rate_rate, "
                << "snd_bytes_rate_rate, snd_packets_rate_rate, "
                << "err_in_rate, err_out_rate, drop_in_rate, drop_out_rate, "
                << "timestamp) VALUES (" << tenantIDSql << "," << teamIDSql << "," << clusterIDSql << "," << serverIDSql
                << "," << hostNameSql << "," << netNameSql << "," << curr.err_in << "," << curr.err_out << ","
                << curr.drop_in << "," << curr.drop_out << "," << curr.net_recv_bytes_rate << ","
                << curr.net_recv_packets_rate << "," << curr.net_send_bytes_rate << "," << curr.net_send_packets_rate
                << "," << rate(curr.net_recv_bytes_rate, last.net_recv_bytes_rate) << ","
                << rate(curr.net_recv_packets_rate, last.net_recv_packets_rate) << ","
                << rate(curr.net_send_bytes_rate, last.net_send_bytes_rate) << ","
                << rate(curr.net_send_packets_rate, last.net_send_packets_rate) << ","
                << rateU64(curr.err_in, last.err_in) << "," << rateU64(curr.err_out, last.err_out) << ","
                << rateU64(curr.drop_in, last.drop_in) << "," << rateU64(curr.drop_out, last.drop_out) << ","
                << timestampSql << ")";
            mysql_query(conn, oss.str().c_str());
            if (mysql_errno(conn)) {
                std::cerr << "MySQL insert error: " << mysql_error(conn) << std::endl;
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
            std::string cpuNameSql = quoteSqlString(conn, cpuName);

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
                << "(tenant_id, team_id, cluster_id, server_id, server_name, cpu_name, hi, timer, net_tx, net_rx, "
                   "block, "
                << "irq_poll, tasklet, sched, hrtimer, rcu, "
                << "hi_rate, timer_rate, net_tx_rate, net_rx_rate, block_rate, "
                << "irq_poll_rate, tasklet_rate, sched_rate, hrtimer_rate, "
                   "rcu_rate, "
                << "timestamp) VALUES (" << tenantIDSql << "," << teamIDSql << "," << clusterIDSql << "," << serverIDSql
                << "," << hostNameSql << "," << cpuNameSql << "," << curr.hi << "," << curr.timer << "," << curr.net_tx
                << "," << curr.net_rx << "," << curr.block << "," << curr.irq_poll << "," << curr.tasklet << ","
                << curr.sched << "," << curr.hrtimer << "," << curr.rcu << "," << rate(curr.hi, last.hi) << ","
                << rate(curr.timer, last.timer) << "," << rate(curr.net_tx, last.net_tx) << ","
                << rate(curr.net_rx, last.net_rx) << "," << rate(curr.block, last.block) << ","
                << rate(curr.irq_poll, last.irq_poll) << "," << rate(curr.tasklet, last.tasklet) << ","
                << rate(curr.sched, last.sched) << "," << rate(curr.hrtimer, last.hrtimer) << ","
                << rate(curr.rcu, last.rcu) << "," << timestampSql << ")";
            mysql_query(conn, oss.str().c_str());

            if (mysql_errno(conn)) {
                std::cerr << "MySQL insert error: " << mysql_error(conn) << std::endl;
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
                << "(tenant_id, team_id, cluster_id, server_id, server_name, total, free, avail, buffers, cached, "
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
                << "sunreclaim_rate, timestamp) VALUES (" << tenantIDSql << "," << teamIDSql << "," << clusterIDSql
                << "," << serverIDSql << "," << hostNameSql << "," << curr.total << "," << curr.free << ","
                << curr.avail << "," << curr.buffers << "," << curr.cached << "," << curr.swap_cached << ","
                << curr.active << "," << curr.inactive << "," << curr.active_anon << "," << curr.inactive_anon << ","
                << curr.active_file << "," << curr.inactive_file << "," << curr.dirty << "," << curr.writeback << ","
                << curr.anon_pages << "," << curr.mapped << "," << curr.kreclaimable << "," << curr.sreclaimable << ","
                << curr.sunreclaim << "," << rate(curr.total, last.total) << "," << rate(curr.free, last.free) << ","
                << rate(curr.avail, last.avail) << "," << rate(curr.buffers, last.buffers) << ","
                << rate(curr.cached, last.cached) << "," << rate(curr.swap_cached, last.swap_cached) << ","
                << rate(curr.active, last.active) << "," << rate(curr.inactive, last.inactive) << ","
                << rate(curr.active_anon, last.active_anon) << "," << rate(curr.inactive_anon, last.inactive_anon)
                << "," << rate(curr.active_file, last.active_file) << ","
                << rate(curr.inactive_file, last.inactive_file) << "," << rate(curr.dirty, last.dirty) << ","
                << rate(curr.writeback, last.writeback) << "," << rate(curr.anon_pages, last.anon_pages) << ","
                << rate(curr.mapped, last.mapped) << "," << rate(curr.kreclaimable, last.kreclaimable) << ","
                << rate(curr.sreclaimable, last.sreclaimable) << "," << rate(curr.sunreclaim, last.sunreclaim) << ","
                << timestampSql << ")";
            mysql_query(conn, oss.str().c_str());

            if (mysql_errno(conn)) {
                std::cerr << "MySQL insert error: " << mysql_error(conn) << std::endl;
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
            std::string diskNameSql = quoteSqlString(conn, diskName);

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
                << "(tenant_id, team_id, cluster_id, server_id, server_name, disk_name, read_ops, write_ops, "
                   "sectors_read, "
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
                << "timestamp) VALUES (" << tenantIDSql << "," << teamIDSql << "," << clusterIDSql << "," << serverIDSql
                << "," << hostNameSql << "," << diskNameSql << "," << disk.reads() << "," << disk.writes() << ","
                << disk.sectors_read() << "," << disk.sectors_written() << "," << disk.read_time_ms() << ","
                << disk.write_time_ms() << "," << disk.io_in_progress() << "," << disk.io_time_ms() << ","
                << disk.weighted_io_time_ms() << "," << curr.read_bytes_per_sec << "," << curr.write_bytes_per_sec
                << "," << curr.read_iops << "," << curr.write_iops << "," << curr.avg_read_latency_ms << ","
                << curr.avg_write_latency_ms << "," << curr.util_percent << ","
                << rate(curr.read_bytes_per_sec, last.read_bytes_per_sec) << ","
                << rate(curr.write_bytes_per_sec, last.write_bytes_per_sec) << ","
                << rate(curr.read_iops, last.read_iops) << "," << rate(curr.write_iops, last.write_iops) << ","
                << rate(curr.avg_read_latency_ms, last.avg_read_latency_ms) << ","
                << rate(curr.avg_write_latency_ms, last.avg_write_latency_ms) << ","
                << rate(curr.util_percent, last.util_percent) << "," << timestampSql << ")";
            mysql_query(conn, oss.str().c_str());

            if (mysql_errno(conn)) {
                std::cerr << "MySQL insert error: " << mysql_error(conn) << std::endl;
                std::cerr << __func__ << " " << __LINE__ << std::endl;
            }
            last = curr;
        }
    }

    // 向 MySQL 明细表插入 MySQL 实例指标数据
    {
        auto counterRate = [](uint64_t nowVal, uint64_t lastVal, double seconds) -> float {
            if (seconds <= 0.0 || nowVal < lastVal) return 0.0f;
            return static_cast<float>((nowVal - lastVal) / seconds);
        };

        for (int i = 0; i < info.mysql_info_size(); ++i) {
            const auto &mysql = info.mysql_info(i);
            std::string instance = mysql.instance();
            if (instance.empty()) {
                instance = mysql.host();
                if (mysql.port() > 0) instance += ":" + std::to_string(mysql.port());
            }
            if (instance.empty()) instance = "unknown";

            const std::string instanceSql = quoteSqlString(conn, instance);
            const std::string mysqlHostSql = quoteSqlString(conn, mysql.host());
            const std::string versionSql = quoteSqlString(conn, mysql.version());
            const std::string roleSql = quoteSqlString(conn, mysql.role().empty() ? "unknown" : mysql.role());

            const float connectionUsedPercent = mysql.max_connections() == 0
                                                    ? 0.0f
                                                    : static_cast<float>(mysql.threads_connected()) /
                                                          static_cast<float>(mysql.max_connections()) * 100.0f;

            MysqlDetailSample curr;
            curr.questions = mysql.questions();
            curr.com_commit = mysql.com_commit();
            curr.com_rollback = mysql.com_rollback();
            curr.slow_queries = mysql.slow_queries();
            curr.innodb_row_lock_waits = mysql.innodb_row_lock_waits();
            curr.timestamp = data.host_score.timestamp;
            curr.initialized = true;

            MysqlDetailSample &last = lastMysqlSamples[data.host_name][instance];
            double seconds = 0.0;
            if (last.initialized) {
                seconds = std::chrono::duration<double>(curr.timestamp - last.timestamp).count();
            }

            const uint64_t currTransactions = curr.com_commit + curr.com_rollback;
            const uint64_t lastTransactions = last.com_commit + last.com_rollback;
            const float qps = last.initialized ? counterRate(curr.questions, last.questions, seconds) : 0.0f;
            const float tps = last.initialized ? counterRate(currTransactions, lastTransactions, seconds) : 0.0f;
            const float slowQueriesRate =
                last.initialized ? counterRate(curr.slow_queries, last.slow_queries, seconds) : 0.0f;
            const float rowLockWaitsRate =
                last.initialized ? counterRate(curr.innodb_row_lock_waits, last.innodb_row_lock_waits, seconds) : 0.0f;

            std::ostringstream oss;
            oss << "INSERT INTO server_mysql_detail "
                << "(tenant_id, team_id, cluster_id, server_id, server_name, instance, mysql_host, mysql_port, up, "
                << "version, `role`, "
                << "max_connections, threads_connected, threads_running, aborted_connects, "
                << "questions, com_select, com_insert, com_update, com_delete, com_commit, com_rollback, "
                << "slow_queries, innodb_buffer_pool_read_requests, innodb_buffer_pool_reads, "
                << "innodb_buffer_pool_hit_percent, innodb_row_lock_waits, innodb_row_lock_time_avg_ms, "
                << "replication_configured, replication_running, replication_lag_seconds, "
                << "connection_used_percent, qps, tps, slow_queries_rate, innodb_row_lock_waits_rate, "
                << "timestamp) VALUES (" << tenantIDSql << "," << teamIDSql << "," << clusterIDSql << "," << serverIDSql
                << "," << hostNameSql << "," << instanceSql << "," << mysqlHostSql << "," << mysql.port() << ","
                << (mysql.up() ? 1 : 0) << "," << versionSql << "," << roleSql << "," << mysql.max_connections() << ","
                << mysql.threads_connected() << "," << mysql.threads_running() << "," << mysql.aborted_connects() << ","
                << mysql.questions() << "," << mysql.com_select() << "," << mysql.com_insert() << ","
                << mysql.com_update() << "," << mysql.com_delete() << "," << mysql.com_commit() << ","
                << mysql.com_rollback() << "," << mysql.slow_queries() << ","
                << mysql.innodb_buffer_pool_read_requests() << "," << mysql.innodb_buffer_pool_reads() << ","
                << mysql.innodb_buffer_pool_hit_percent() << "," << mysql.innodb_row_lock_waits() << ","
                << mysql.innodb_row_lock_time_avg_ms() << "," << (mysql.replication_configured() ? 1 : 0) << ","
                << (mysql.replication_running() ? 1 : 0) << "," << mysql.replication_lag_seconds() << ","
                << connectionUsedPercent << "," << qps << "," << tps << "," << slowQueriesRate << ","
                << rowLockWaitsRate << "," << timestampSql << ")";
            mysql_query(conn, oss.str().c_str());

            if (mysql_errno(conn)) {
                std::cerr << "MySQL insert error: " << mysql_error(conn) << std::endl;
                std::cerr << __func__ << " " << __LINE__ << std::endl;
            }
            last = curr;
        }
    }

    // 向 MySQL 明细表插入 Redis 实例指标数据
    {
        auto counterRate = [](uint64_t nowVal, uint64_t lastVal, double seconds) -> float {
            if (seconds <= 0.0 || nowVal < lastVal) return 0.0f;
            return static_cast<float>((nowVal - lastVal) / seconds);
        };
        auto counterDelta = [](uint64_t nowVal, uint64_t lastVal) -> float {
            if (nowVal < lastVal) return 0.0f;
            return static_cast<float>(nowVal - lastVal);
        };

        for (int i = 0; i < info.redis_info_size(); ++i) {
            const auto &redis = info.redis_info(i);
            std::string instance = redis.instance();
            if (instance.empty()) {
                instance = redis.host();
                if (redis.port() > 0) instance += ":" + std::to_string(redis.port());
            }
            if (instance.empty()) instance = "unknown";

            const std::string instanceSql = quoteSqlString(conn, instance);
            const std::string redisHostSql = quoteSqlString(conn, redis.host());
            const std::string versionSql = quoteSqlString(conn, redis.version());
            const std::string roleSql = quoteSqlString(conn, redis.role().empty() ? "unknown" : redis.role());

            const float connectionUsedPercent =
                redis.maxclients() == 0
                    ? 0.0f
                    : static_cast<float>(redis.connected_clients()) / static_cast<float>(redis.maxclients()) * 100.0f;

            RedisDetailSample curr;
            curr.total_commands_processed = redis.total_commands_processed();
            curr.total_net_input_bytes = redis.total_net_input_bytes();
            curr.total_net_output_bytes = redis.total_net_output_bytes();
            curr.slowlog_len = redis.slowlog_len();
            curr.timestamp = data.host_score.timestamp;
            curr.initialized = true;

            RedisDetailSample &last = lastRedisSamples[data.host_name][instance];
            double seconds = 0.0;
            if (last.initialized) seconds = std::chrono::duration<double>(curr.timestamp - last.timestamp).count();

            // Redis 的总量计数器只增不减，重启或回绕时派生速率按 0 处理。
            const float commandsPerSec =
                last.initialized ? counterRate(curr.total_commands_processed, last.total_commands_processed, seconds)
                                 : 0.0f;
            const float netInputBytesPerSec =
                last.initialized ? counterRate(curr.total_net_input_bytes, last.total_net_input_bytes, seconds) : 0.0f;
            const float netOutputBytesPerSec =
                last.initialized ? counterRate(curr.total_net_output_bytes, last.total_net_output_bytes, seconds)
                                 : 0.0f;
            const float slowlogGrowth = last.initialized ? counterDelta(curr.slowlog_len, last.slowlog_len) : 0.0f;

            std::ostringstream oss;
            oss << "INSERT INTO server_redis_detail "
                << "(tenant_id, team_id, cluster_id, server_id, server_name, instance, redis_host, redis_port, up, "
                << "version, `role`, uptime_in_seconds, "
                << "connected_clients, blocked_clients, maxclients, connection_used_percent, "
                << "used_memory, maxmemory, mem_fragmentation_ratio, memory_used_percent, "
                << "total_commands_processed, instantaneous_ops_per_sec, commands_per_sec, "
                << "keyspace_hits, keyspace_misses, keyspace_hit_percent, expired_keys, evicted_keys, "
                << "rejected_connections, total_error_replies, total_net_input_bytes, total_net_output_bytes, "
                << "net_input_bytes_per_sec, net_output_bytes_per_sec, replication_configured, master_link_up, "
                << "connected_slaves, master_last_io_seconds_ago, slowlog_len, slowlog_growth, timestamp) VALUES ("
                << tenantIDSql << "," << teamIDSql << "," << clusterIDSql << "," << serverIDSql << "," << hostNameSql
                << "," << instanceSql << "," << redisHostSql << "," << redis.port() << "," << (redis.up() ? 1 : 0)
                << "," << versionSql << "," << roleSql << "," << redis.uptime_in_seconds() << ","
                << redis.connected_clients() << "," << redis.blocked_clients() << "," << redis.maxclients() << ","
                << connectionUsedPercent << "," << redis.used_memory() << "," << redis.maxmemory() << ","
                << redis.mem_fragmentation_ratio() << "," << redis.memory_used_percent() << ","
                << redis.total_commands_processed() << "," << redis.instantaneous_ops_per_sec() << "," << commandsPerSec
                << "," << redis.keyspace_hits() << "," << redis.keyspace_misses() << "," << redis.keyspace_hit_percent()
                << "," << redis.expired_keys() << "," << redis.evicted_keys() << "," << redis.rejected_connections()
                << "," << redis.total_error_replies() << "," << redis.total_net_input_bytes() << ","
                << redis.total_net_output_bytes() << "," << netInputBytesPerSec << "," << netOutputBytesPerSec << ","
                << (redis.replication_configured() ? 1 : 0) << "," << (redis.master_link_up() ? 1 : 0) << ","
                << redis.connected_slaves() << "," << redis.master_last_io_seconds_ago() << "," << redis.slowlog_len()
                << "," << slowlogGrowth << "," << timestampSql << ")";
            mysql_query(conn, oss.str().c_str());

            if (mysql_errno(conn)) {
                std::cerr << "MySQL insert error: " << mysql_error(conn) << std::endl;
                std::cerr << __func__ << " " << __LINE__ << std::endl;
            }
            last = curr;
        }
    }

#else
    (void)data; // 避免未使用参数警告
#endif
}

void HostManager::start() {
    running_.store(true);
    thread_ = std::make_unique<std::thread>(&HostManager::processLoop, this);
    const int writerThreads = std::max(1, config_.mysql_write_pool_max);
    mysqlWriteThreads_.reserve(static_cast<std::size_t>(writerThreads));
    // 创建mysql写入线程池，线程数根据配置决定，默认为4
    for (int i = 0; i < writerThreads; ++i) mysqlWriteThreads_.emplace_back(&HostManager::mysqlWriteLoop, this);
}

void HostManager::stop() {
    running_.store(false);
    mysqlWriteQueueCv_.notify_all();
    if (thread_ && thread_->joinable()) thread_->join();
    for (auto &worker : mysqlWriteThreads_)
        if (worker.joinable()) worker.join();
    mysqlWriteThreads_.clear();
}

bool HostManager::resolveWorkerScope(const WorkerIdentity &workerIdentity, WorkerScope *scope) {
    if (!scope || workerIdentity.worker_id.empty()) return false;
#ifdef ENABLE_MYSQL
    if (!mysqlWritePool_) return false;
    auto guard = mysqlWritePool_->acquire(config_.mysql_read_timeout);
    MYSQL *conn = guard.get();
    if (!conn) return false;

    // 查询 worker_registrations 表验证 worker_id 和 worker_token，并获取对应的 tenant_id、team_id、cluster_id 和
    // server_id（如果有）。使用 COALESCE(server_id, 0) 确保即使 server_id 为空也能正确处理。
    // TODO: 可以增加缓存来减少对数据库的查询频率，尤其是在 worker 数量较多的情况下。
    const std::string workerIDSql = quoteSqlString(conn, workerIdentity.worker_id);
    std::ostringstream oss;
    oss << "SELECT tenant_id, team_id, cluster_id, COALESCE(server_id, 0), token_hash "
        << "FROM worker_registrations WHERE worker_id=" << workerIDSql << " AND status='active' LIMIT 1";
    if (mysql_query(conn, oss.str().c_str()) != 0) {
        std::cerr << "Worker registry query error: " << mysql_error(conn) << std::endl;
        return false;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res) return false;
    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) {
        mysql_free_result(res);
        return false;
    }
    const std::string expectedToken = row[4] ? row[4] : "";
    if (!expectedToken.empty() && expectedToken != workerIdentity.worker_token) {
        mysql_free_result(res);
        return false;
    }
    scope->tenant_id = row[0] ? row[0] : "";
    scope->team_id = row[1] ? row[1] : "";
    scope->cluster_id = row[2] ? row[2] : "";
    scope->server_id = row[3] ? static_cast<std::uint64_t>(std::strtoull(row[3], nullptr, 10)) : 0;
    mysql_free_result(res);
    return !scope->tenant_id.empty() && !scope->team_id.empty() && !scope->cluster_id.empty();
#else
    (void)workerIdentity;
    return false;
#endif
}

void HostManager::onDataReceived(const monitor::proto::MonitorInfo &info, const WorkerIdentity &workerIdentity) {
    WorkerScope scope;
    if (!resolveWorkerScope(workerIdentity, &scope)) {
        std::cerr << "Drop monitor data from unregistered or unauthorized worker: " << workerIdentity.worker_id
                  << std::endl;
        if (metrics_) metrics_->dropped_monitor_samples.fetch_add(1);
        return;
    }

    // 创建唯一主机 ID，生产环境可替换为真实主机名
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
        netInRate = info.net_info(0).rcv_rate() / (1024.0 * 1024.0);   // convert to MB/s
        netOutRate = info.net_info(0).send_rate() / (1024.0 * 1024.0); // convert to MB/s
    }

    // store the current performance metrics for rate calculation in the next
    // round
    PerfSample curr;
    if (const auto *cpu = selectAggregateCpuStat(info)) {
        curr.cpu_percent = cpu->cpu_percent();
        curr.usr_percent = cpu->usr_percent();
        curr.system_percent = cpu->system_percent();
        curr.nice_percent = cpu->nice_percent();
        curr.idle_percent = cpu->idle_percent();
        curr.io_wait_percent = cpu->io_wait_percent();
        curr.irq_percent = cpu->irq_percent();
        curr.soft_irq_percent = cpu->soft_irq_percent();
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

    float cpu_percent_rate = 0;
    float usr_percent_rate = 0;
    float system_percent_rate = 0;
    float nice_percent_rate = 0;
    float idle_percent_rate = 0;
    float io_wait_percent_rate = 0;
    float irq_percent_rate = 0;
    float soft_irq_percent_rate = 0;
    float load_avg_1_rate = 0;
    float load_avg_3_rate = 0;
    float load_avg_15_rate = 0;
    float mem_used_percent_rate = 0;
    float mem_total_rate = 0;
    float mem_free_rate = 0;
    float mem_avail_rate = 0;
    float net_in_rate_rate = 0;
    float net_out_rate_rate = 0;
    {
        std::lock_guard<std::mutex> lock(lastPerfSamplesMutex);
        PerfSample &last = lastPerfSamples[hostID];
        auto rate = [](float nowVal, float lastVal) -> float {
            if (lastVal == 0) return 0;
            return (nowVal - lastVal) / lastVal;
        };

        cpu_percent_rate = rate(curr.cpu_percent, last.cpu_percent);
        usr_percent_rate = rate(curr.usr_percent, last.usr_percent);
        system_percent_rate = rate(curr.system_percent, last.system_percent);
        nice_percent_rate = rate(curr.nice_percent, last.nice_percent);
        idle_percent_rate = rate(curr.idle_percent, last.idle_percent);
        io_wait_percent_rate = rate(curr.io_wait_percent, last.io_wait_percent);
        irq_percent_rate = rate(curr.irq_percent, last.irq_percent);
        soft_irq_percent_rate = rate(curr.soft_irq_percent, last.soft_irq_percent);
        load_avg_1_rate = rate(curr.load_avg_1, last.load_avg_1);
        load_avg_3_rate = rate(curr.load_avg_3, last.load_avg_3);
        load_avg_15_rate = rate(curr.load_avg_15, last.load_avg_15);
        mem_used_percent_rate = rate(curr.mem_used_percent, last.mem_used_percent);
        mem_total_rate = rate(curr.mem_total, last.mem_total);
        mem_free_rate = rate(curr.mem_free, last.mem_free);
        mem_avail_rate = rate(curr.mem_avail, last.mem_avail);
        net_in_rate_rate = rate(curr.net_in_rate, last.net_in_rate);
        net_out_rate_rate = rate(curr.net_out_rate, last.net_out_rate);
        lastPerfSamples[hostID] = curr;
    }
    HostMonitoringData data{hostID,
                            scope,
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
    {
        std::lock_guard<std::mutex> lock(mutex_);
        hostScores_[hostID] = HostScore{info, score, now};
    }

    // 存储最新的分数到Redis缓存，键名格式为"manager:latest_score:{hostID}"，值为JSON字符串包含服务器名称和分数，例如{"server_name":"{hostID}","score":85.5}
    if (redisCache_) {
        std::ostringstream cached;
        cached << "{\"server_name\":\"" << hostID << "\",\"score\":" << score << "}";
        redisCache_->set("manager:latest_score:" + hostID, cached.str());
    }

    // 将数据加入MySQL写入队列，由写入线程异步处理，避免阻塞数据接收线程
    enqueueMysqlWrite(std::move(data));

    // 日志输出，只有在verbose_log配置项为true时才输出详细日志，默认值为false
    if (!config_.verbose_log) return;

    std::cout << "\n================== Received Data ==================" << std::endl;
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
    std::cout << "  Load: " << curr.load_avg_1 << "/" << curr.load_avg_3 << "/" << curr.load_avg_15 << std::endl;

    // 内存详细信息
    std::cout << "\n--- Memory ---" << std::endl;
    std::cout << "  Used: " << curr.mem_used_percent << "%, "
              << "Total: " << curr.mem_total << " MB" << std::endl;
    std::cout << "  Free: " << curr.mem_free << " MB, "
              << "Avail: " << curr.mem_avail << " MB" << std::endl;

    // 网络详细信息
    std::cout << "\n--- Network ---" << std::endl;
    std::cout << "  In: " << curr.net_in_rate * 1024 * 1024 << " B/s, "
              << "Out: " << curr.net_out_rate * 1024 * 1024 << " B/s" << std::endl;
    for (int i = 0; i < info.net_info_size(); ++i) {
        const auto &net = info.net_info(i);
        std::cout << "  [" << net.name() << "] Recv: " << net.rcv_rate() << " B/s, "
                  << "Send: " << net.send_rate() << " B/s, "
                  << "Drops: " << net.drop_in() << "/" << net.drop_out() << std::endl;
    }

    // 磁盘详细信息
    std::cout << "\n--- Disk ---" << std::endl;
    float max_disk_util = 0;
    for (int i = 0; i < info.disk_info_size(); ++i) {
        const auto &disk = info.disk_info(i);
        std::cout << "  [" << disk.name() << "] "
                  << "Read: " << disk.read_bytes_per_sec() / 1024.0 << " KB/s, "
                  << "Write: " << disk.write_bytes_per_sec() / 1024.0 << " KB/s, "
                  << "Util: " << disk.util_percent() << "%" << std::endl;
        if (disk.util_percent() > max_disk_util) max_disk_util = disk.util_percent();
    }
    if (info.disk_info_size() == 0) std::cout << "  No disk data" << std::endl;

    // Redis 信息
    if (info.redis_info_size() > 0) {
        std::cout << "\n--- Redis ---" << std::endl;
        for (int i = 0; i < info.redis_info_size(); ++i) {
            const auto &redis = info.redis_info(i);
            std::cout << "  [" << redis.instance() << "] Up: " << (redis.up() ? "yes" : "no")
                      << ", Role: " << redis.role() << ", Clients: " << redis.connected_clients()
                      << ", Memory: " << redis.memory_used_percent() << "%, Hit: " << redis.keyspace_hit_percent()
                      << "%" << std::endl;
        }
    }

    // 软中断信息
    std::cout << "\n--- SoftIRQ ---" << std::endl;
    std::cout << "  CPU cores with softirq data: " << info.soft_irq_size() << std::endl;

    // 变化率信息
    std::cout << "\n--- Change Rates ---" << std::endl;
    std::cout << "  CPU: " << cpu_percent_rate * 100 << "%, "
              << "Mem: " << mem_used_percent_rate * 100 << "%, "
              << "Load: " << load_avg_1_rate * 100 << "%" << std::endl;
    std::cout << "  NetIn: " << net_in_rate_rate * 100 << "%, "
              << "NetOut: " << net_out_rate_rate * 100 << "%" << std::endl;

    std::cout << "\n--- Database ---" << std::endl;
#ifdef ENABLE_MYSQL
    std::cout << "  Data enqueued for MySQL writer pool" << std::endl;
#else
    std::cout << "  MySQL support is disabled" << std::endl;
#endif
    std::cout << "====================================================\n" << std::endl;
}

void HostManager::enqueueMysqlWrite(HostMonitoringData data) {
#ifdef ENABLE_MYSQL
    std::lock_guard<std::mutex> lock(mysqlWriteQueueMutex_);
    const std::size_t capacity = std::max<std::size_t>(1, config_.task_queue_capacity);
    // 如果队列已满，丢弃最旧的数据以腾出空间，并记录丢弃的样本数
    if (mysqlWriteQueue_.size() >= capacity) {
        mysqlWriteQueue_.pop_front();
        if (metrics_) metrics_->dropped_monitor_samples.fetch_add(1);
    }
    mysqlWriteQueue_.push_back(std::move(data));
    mysqlWriteQueueCv_.notify_one();
#else
    (void)data;
#endif
}

void HostManager::mysqlWriteLoop() {
    while (true) {
        HostMonitoringData data;
        {
            std::unique_lock<std::mutex> lock(mysqlWriteQueueMutex_);
            mysqlWriteQueueCv_.wait_for(lock, std::chrono::milliseconds(100),
                                        [this] { return !running_.load() || !mysqlWriteQueue_.empty(); });
            if (!running_.load() && mysqlWriteQueue_.empty()) return;
            // 在等待时可能会被唤醒多次，或者在停止时被唤醒，此时需要再次检查队列是否有数据
            if (mysqlWriteQueue_.empty()) continue;
            data = std::move(mysqlWriteQueue_.front());
            mysqlWriteQueue_.pop_front();
        }

        // writeToMysql 仍要避免长时间持有
        // mysqlWriteMutex_，因为它可能会涉及网络IO等耗时操作，所以在外层只锁定队列操作，写入数据库的部分不锁定，以提高吞吐量
        std::lock_guard<std::mutex> writeLock(mysqlWriteMutex_);
        writeToMysql(data);
    }
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
