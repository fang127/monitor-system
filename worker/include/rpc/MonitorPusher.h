/**
 * @brief         MonitorPusher class definition
 * @file          MonitorPusher.h
 * @author        harry
 * @date          2026-02-11
 */
#pragma once

#include "MetricCollector.h"
#include "monitor_info.grpc.pb.h"

#include <string>
#include <memory>
#include <atomic>
#include <thread>

namespace monitor {
class MonitorPusher {
public:
    /**
     * @brief         Construct a new Monitor Pusher object
     *
     * @param         managerAddress The gRPC address of the manager to which
     * metrics will be pushed
     * @param         intervalSeconds The interval in seconds at which metrics
     * will be pushed to the manager (default is 10 seconds)
     */
    explicit MonitorPusher(const std::string &managerAddress,
                           int intervalSeconds = 10);
    ~MonitorPusher();

    /**
     * @brief         Start the monitor pusher thread to periodically push
     * metrics to the manager
     *
     */
    void start();

    /**
     * @brief         Stop the monitor pusher thread
     *
     */
    void stop();

    /**
     * @brief         Get the Manager Address object
     *
     * @return        const std::string&
     */
    const std::string &getManagerAddress() const { return managerAddress_; }

private:
    /**
     * @brief         The main loop for pushing metrics to the manager. This
     * function will be run in a separate thread and will continuously push
     * metrics at the specified interval until the pusher is stopped. It will
     * call the pushOnce function to perform the actual metric collection and
     * pushing to the manager.
     *
     */
    void pushLoop();

    /**
     * @brief         Push metrics to the manager once. This function will
     * collect metrics and send them to the manager using gRPC. It will be
     * called periodically by the pushLoop function based on the specified
     * interval.
     *
     * @return        bool True if the metrics were successfully pushed to the
     * manager, false otherwise.
     */
    bool pushOnce();

    std::string managerAddress_; // gRPC manager address
    int intervalSeconds_;        // Interval in seconds for pushing metrics
    std::atomic<bool>
        running_; // Flag to control the running state of the pusher
    std::unique_ptr<std::thread> thread_;        // Thread for pushing metrics
    std::unique_ptr<MetricCollector> collector_; // Metric collector instance
    std::unique_ptr<monitor::proto::GrpcManager::Stub>
        stub_; // gRPC stub for communication with the manager
};
} // namespace monitor