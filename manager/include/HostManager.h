#pragma once
#include "ManagerConfig.h"
#include "ManagerMetrics.h"
#include "MysqlConnectionPool.h"
#include "RedisConnectionPool.h"
#include "monitor_info.pb.h"

#include <condition_variable>
#include <deque>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>
#include <vector>

namespace monitor {
/**
 * @brief         HostScore is a structure that encapsulates the monitoring
 * information of a host along with its computed score and the last update time.
 * It is used to evaluate and compare the performance of different hosts based
 * on their monitoring data.
 */
struct HostScore {
    monitor::proto::MonitorInfo info;
    double score;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief         HostMonitoringData is a structure that encapsulates the
 * monitoring data of a host along with its computed score. It is used to store
 * the monitoring data of a host for further analysis and persistence in a MySQL
 * database.
 *
 */
struct HostMonitoringData {
    std::string host_name;
    HostScore host_score;
    double net_in_rate;
    double net_out_rate;
    float cpu_percent_rate;
    float usr_percent_rate;
    float system_percent_rate;
    float nice_percent_rate;
    float idle_percent_rate;
    float io_wait_percent_rate;
    float irq_percent_rate;
    float soft_irq_percent_rate;
    float steal_percent_rate;
    float guest_percent_rate;
    float guest_nice_percent_rate;
    float load_avg_1_rate;
    float load_avg_3_rate;
    float load_avg_15_rate;
    float mem_used_percent_rate;
    float mem_total_rate;
    float mem_free_rate;
    float mem_avail_rate;
    float net_in_rate_rate;
    float net_out_rate_rate;
    float net_in_drop_rate_rate;
    float net_out_drop_rate_rate;
};

class HostManager {
public:
    HostManager();
    ~HostManager();

    /**
     * @brief         configure HostManager with necessary parameters and dependencies
     *
     * @param         config
     * @param         metrics
     * @param         mysqlWritePool
     * @param         redisCache
     */
    void configure(const ManagerConfig &config, ManagerMetrics *metrics, MysqlConnectionPool *mysqlWritePool,
                   RedisCache *redisCache);

    /**
     * @brief         start HostManager thread
     *
     */
    void start();

    /**
     * @brief         stop HostManager thread
     *
     */
    void stop();

    /**
     * @brief         receive worker push monitoring data
     *
     * @param         info
     */
    void onDataReceived(const monitor::proto::MonitorInfo &info);

    /**
     * @brief         Get the All Host Scores object
     *
     * @return
     */
    std::unordered_map<std::string, HostScore> getAllHostScores();

    /**
     * @brief         Get the Best Host object
     *
     * @return
     */
    std::string getBestHost();

private:
    /**
     * @brief         running in a loop to process host scores
     *
     */
    void processLoop();

    /**
     * @brief         Calculate the score of a host based on its monitoring
     * information.
     *
     * @param         info
     * @return
     */
    double calculateScore(const monitor::proto::MonitorInfo &info);

    /**
     * @brief         Write the host score and monitoring data to MySQL database
     * for persistence and further analysis.
     *
     */
    void writeToMysql(HostMonitoringData &data);

    /**
     * @brief         Enqueue the host monitoring data to the MySQL write queue for asynchronous processing.
     *
     * @param         data
     */
    void enqueueMysqlWrite(HostMonitoringData data);

    /**
     * @brief         Running in a loop to process the MySQL write queue and persist the host monitoring data to the
     * MySQL database.
     *
     */
    void mysqlWriteLoop();

    std::unordered_map<std::string, HostScore> hostScores_; // key: host_name, value: HostScore
    std::mutex mutex_;                                      // protects hostScores_
    std::atomic<bool> running_;                             // flag to control the running state of the processing loop
    std::unique_ptr<std::thread> thread_;                   // thread for processing host scores
    ManagerConfig config_;                                  // configuration parameters for HostManager
    ManagerMetrics *metrics_ = nullptr;                     // pointer to ManagerMetrics for recording metrics
    MysqlConnectionPool *mysqlWritePool_ =
        nullptr; // pointer to MysqlConnectionPool for writing host monitoring data to MySQL database
    RedisCache *redisCache_ = nullptr; // pointer to RedisCache for caching host scores and related information

    std::deque<HostMonitoringData>
        mysqlWriteQueue_;             // queue for storing host monitoring data to be written to MySQL database
    std::mutex mysqlWriteQueueMutex_; // mutex for protecting access to the MySQL write queue
    std::condition_variable
        mysqlWriteQueueCv_; // condition variable for signaling the MySQL write thread when new data is enqueued
    std::vector<std::thread> mysqlWriteThreads_; // threads for processing the MySQL write queue
    std::mutex mysqlWriteMutex_;                 // mutex for protecting access to the MySQL write threads
};

}; // namespace monitor
