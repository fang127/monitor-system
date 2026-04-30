#pragma once

#include <string>

#include <mysql.h>
#include <chrono>
#include <string>
#include <mutex>
#include <vector>

namespace monitor {
/**
 * @brief         sort order
 *
 */
enum class SortOrder {
    DESC = 0,
    ASC = 1,
};

/**
 * @brief         service status
 *
 */
enum class ServerStatus {
    ONLINE = 0,
    OFFLINE = 1,
};

/**
 * @brief         anomaly detection thresholds
 *
 */
struct AnomalyThreshold {
    float cpu_threshold = 80.0f;
    float memory_threshold = 90.0f;
    float disk_threshold = 85.0f;
    float change_rate_threshold = 0.5f; // 50% change rate
};

/**
 * @brief         time range for querying metrics
 *
 */
struct TimeRange {
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
};

/**
 * @brief         performance record for a server at a specific timestamp
 *
 */
struct PerformanceRecord {
    std::string server_name;                         // name of the server
    std::chrono::system_clock::time_point timestamp; // timestamp of the record
    // cpu usage metrics
    float cpu_percent = 0.0f;      // CPU usage percentage
    float usr_percent = 0.0f;      // user CPU usage percentage
    float sys_percent = 0.0f;      // system CPU usage percentage
    float nice_percent = 0.0f;     // nice CPU usage percentage
    float idle_percent = 0.0f;     // idle CPU usage percentage
    float io_wait_percent = 0.0f;  // I/O wait CPU usage percentage
    float irq_percent = 0.0f;      // IRQ CPU usage percentage
    float soft_irq_percent = 0.0f; // soft IRQ CPU usage percentage
    // cpu load
    float load_avg_1 = 0.0f;  // 1-minute load average
    float load_avg_3 = 0.0f;  // 5-minute load average
    float load_avg_15 = 0.0f; // 15-minute load average
    // memory usage metrics
    float mem_used_percent = 0.0f; // memory usage percentage
    float mem_total = 0.0f;        // total memory in bytes
    float mem_free = 0.0f;         // free memory in bytes
    float mem_avail = 0.0f;        // available memory in bytes
    // disk usage metrics
    float disk_util_percent = 0.0f; // disk utilization percentage
    // network usage metrics
    float net_recv_bytes = 0.0f; // network received bytes per second
    float net_sent_bytes = 0.0f; // network sent bytes per second
    // score
    float score = 0.0f; // overall performance score
    // rate of change
    float cpu_percent_rate = 0.0f; // rate of change of CPU usage percentage
    float mem_used_percent_rate =
        0.0f; // rate of change of memory usage percentage
    float disk_util_percent_rate =
        0.0f; // rate of change of disk utilization percentage
    float load_avg_1_rate = 0.0f; // rate of change of 1-minute load average
    float net_recv_bytes_rate =
        0.0f; // rate of change of network received bytes per second
    float net_sent_bytes_rate =
        0.0f; // rate of change of network sent bytes per second
};

/**
 * @brief         anomaly record for a server at a specific timestamp
 *
 */
struct AnomalyRecord {
    std::string server_name;                         // name of the server
    std::chrono::system_clock::time_point timestamp; // timestamp of the anomaly
    std::string anomaly_type; // type of anomaly (e.g., "CPU", "Memory", "Disk")
    std::string
        severity;       // severity of the anomaly (e.g., "Critical", "Warning")
    float value = 0.0f; // value that triggered the anomaly
    float threshold = 0.0f;  // threshold that was exceeded
    std::string metric_name; // name of the metric that triggered the anomaly
};

/**
 * @brief         summary of server performance and status
 *
 */
struct ServerScoreSummary {
    std::string server_name; // name of the server
    float score = 0.0f;      // overall performance score
    std::chrono::system_clock::time_point
        last_updated;                           // timestamp of the last update
    ServerStatus status = ServerStatus::ONLINE; // current status of the server
    float cpu_percent = 0.0f;                   // current CPU usage percentage
    float mem_used_percent = 0.0f;  // current memory usage percentage
    float disk_util_percent = 0.0f; // current disk utilization percentage
    float load_avg_1 = 0.0f;        // current 1-minute load average
};

/**
 * @brief         summary of cluster performance and status
 *
 */
struct ClusterStats {
    int total_servers = 0;    // total number of servers in the cluster
    int online_servers = 0;   // number of online servers
    int offline_servers = 0;  // number of offline servers
    float avg_score = 0.0f;   // average performance score across the cluster
    float max_score = 0.0f;   // maximum performance score in the cluster
    float min_score = 0.0f;   // minimum performance score in the cluster
    std::string best_server;  // name of the server with the highest score
    std::string worst_server; // name of the server with the lowest score
};

/**
 * @brief         detailed network performance record for a server at a specific
 * timestamp
 *
 */
struct NetDetailRecord {
    std::string server_name; // name of the server
    std::string net_name;    // name of the network interface
    std::chrono::system_clock::time_point timestamp; // timestamp of the record
    uint64_t err_in = 0;   // number of input errors per second
    uint64_t err_out = 0;  // number of output errors per second
    uint64_t drop_in = 0;  // number of input packets dropped per second
    uint64_t drop_out = 0; // number of output packets dropped per second
    float recv_bytes_rate =
        0.0f; // rate of change of network received bytes per second
    float sent_bytes_rate =
        0.0f; // rate of change of network sent bytes per second
    float recv_packets_rate =
        0.0f; // rate of change of network received packets per second
    float sent_packets_rate =
        0.0f; // rate of change of network sent packets per second
};

/**
 * @brief         detailed disk performance record for a server at a specific
 * timestamp
 *
 */
struct DiskDetailRecord {
    std::string server_name;
    std::string disk_name;
    std::chrono::system_clock::time_point timestamp;
    float read_bytes_per_sec = 0;
    float write_bytes_per_sec = 0;
    float read_iops = 0;
    float write_iops = 0;
    float avg_read_latency_ms = 0;
    float avg_write_latency_ms = 0;
    float util_percent = 0;
};

/**
 * @brief         detailed memory performance record for a server at a specific
 * timestamp
 *
 */
struct MemDetailRecord {
    std::string server_name;
    std::chrono::system_clock::time_point timestamp;
    float mem_total = 0.0f; // total memory in bytes
    float mem_free = 0.0f;  // free memory in bytes
    float mem_avail = 0.0f; // available memory in bytes
    float buffers = 0.0f;   // memory used for buffers in bytes
    float cached = 0.0f;    // memory used for cache in bytes
    float active = 0.0f;    // active memory in bytes
    float inactive = 0.0f;  // inactive memory in bytes
    float dirty = 0.0f;     // dirty memory in bytes
};

/**
 * @brief         detailed soft IRQ performance record for a server at a
 * specific timestamp
 *
 */
struct SoftIrqDetailRecord {
    std::string server_name;
    std::string
        cpu_name; // e.g., "all" for total, or "cpu0", "cpu1", etc. for per-CPU
    std::chrono::system_clock::time_point timestamp;
    int64_t hi = 0;     // high priority softirqs per second
    int64_t timer = 0;  // timer softirqs per second
    int64_t net_tx = 0; // network transmit softirqs per second
    int64_t net_rx = 0; // network receive softirqs per second
    int64_t block = 0;  // block softirqs per second
    int64_t sched = 0;  // scheduler softirqs per second
};

class QueryManager {
public:
    QueryManager() = default;
    ~QueryManager() { close(); }

    /**
     * @brief         initialize the QueryManager with database connection
     * parameters
     *
     * @param         host database host address
     * @param         port database port
     * @param         user database username
     * @param         password database password
     * @param         db database name
     * @return        true if initialization is successful, false otherwise
     */
    bool init(const std::string &host, unsigned int port,
              const std::string &user, const std::string &password,
              const std::string &db);

    /**
     * @brief         close the database connection and clean up resources
     *
     */
    void close();

    /**
     * @brief         validate the given time range for querying metrics
     *
     * @param         range time range to validate
     * @return        true if the time range is valid, false otherwise
     */
    bool validateTimeRange(const TimeRange &range) const;

    /**
     * @brief         query performance records for a specific server within a
     * given time
     *
     * @param         serverName server name
     * @param         range time range for querying
     * @param         page mysql page number
     * @param         pageSize number of records per page
     * @param         totalCount pointer to store total count of records
     * @return        std::vector<PerformanceRecord>
     */
    std::vector<PerformanceRecord> queryPerformanceRecords(
        const std::string &serverName, const TimeRange &range, int page,
        int pageSize, int *totalCount);

    /**
     * @brief         query trend data for a specific server over a given time
     * range
     *
     * @param         serverName
     * @param         range
     * @param         intervalSeconds
     * @return        trend data as a vector of PerformanceRecord
     */
    std::vector<PerformanceRecord> queryTrend(const std::string &serverName,
                                              const TimeRange &range,
                                              int intervalSeconds);

    /**
     * @brief         query anomaly records for a specific server within a given
     * time
     *
     * @param         serverName
     * @param         range
     * @param         threshold
     * @param         page
     * @param         pageSize
     * @param         totalCount
     * @return
     */
    std::vector<AnomalyRecord> queryAnomalyRecords(
        const std::string &serverName, const TimeRange &range,
        const AnomalyThreshold &threshold, int page, int pageSize,
        int *totalCount);

    /**
     * @brief         query server score summaries with pagination and sorting
     *
     * @param         order
     * @param         page
     * @param         pageSize
     * @param         totalCount
     * @return
     */
    std::vector<ServerScoreSummary> queryServerScoreRank(SortOrder order,
                                                         int page, int pageSize,
                                                         int *totalCount);

    /**
     * @brief         query latest server scores and cluster statistics
     *
     * @param         clusterStats
     * @return
     */
    std::vector<ServerScoreSummary> queryLatestServerScores(
        ClusterStats *clusterStats);

    /**
     * @brief         query net statistics
     *
     * @param         serverName
     * @param         range
     * @param         page
     * @param         pageSize
     * @param         totalCount
     * @return
     */
    std::vector<NetDetailRecord> queryNetDetailRecords(
        const std::string &serverName, const TimeRange &range, int page,
        int pageSize, int *totalCount);

    /**
     * @brief         query disk statistics
     *
     * @param         serverName
     * @param         range
     * @param         page
     * @param         pageSize
     * @param         totalCount
     * @return
     */
    std::vector<DiskDetailRecord> queryDiskDetailRecords(
        const std::string &serverName, const TimeRange &range, int page,
        int pageSize, int *totalCount);

    /**
     * @brief         query memory statistics
     *
     * @param         serverName
     * @param         range
     * @param         page
     * @param         pageSize
     * @param         totalCount
     * @return
     */
    std::vector<MemDetailRecord> queryMemDetailRecords(
        const std::string &serverName, const TimeRange &range, int page,
        int pageSize, int *totalCount);

    /**
     * @brief         query soft IRQ statistics
     *
     * @param         serverName
     * @param         range
     * @param         page
     * @param         pageSize
     * @param         totalCount
     * @return
     */
    std::vector<SoftIrqDetailRecord> querySoftIrqDetailRecords(
        const std::string &serverName, const TimeRange &range, int page,
        int pageSize, int *totalCount);

private:
    /**
     * @brief         format a time_point to a string suitable for SQL queries
     *
     * @param         tp
     * @return
     */
    std::string formatTimePoint(
        const std::chrono::system_clock::time_point &tp) const;

    /**
     * @brief         parse a time string from SQL query results to a time_point
     *
     * @param         timeStr
     * @return
     */
    std::chrono::system_clock::time_point parseTimeString(
        const std::string &timeStr) const;

#ifdef ENABLE_MYSQL
    MYSQL *conn_; // MySQL connection handle
#endif
    std::mutex mutex_;
    bool initialized_ = false;
};

} // namespace monitor
