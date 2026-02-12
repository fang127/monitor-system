#pragma once
#include "monitor_info.pb.h"

#include <chrono>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>

namespace monitor
{
/**
 * @brief         HostScore is a structure that encapsulates the monitoring
 * information of a host along with its computed score and the last update time.
 * It is used to evaluate and compare the performance of different hosts based
 * on their monitoring data.
 */
struct HostScore
{
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
struct HostMonitoringData
{
    const std::string &host_name;
    const HostScore &host_score;
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

class HostManager
{
public:
    HostManager();
    ~HostManager();

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

    std::unordered_map<std::string, HostScore> hostScores_;
    std::mutex mutex_;
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> thread_;
};

}; // namespace monitor