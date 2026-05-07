#pragma once

#include <atomic>
#include <cstdint>
#include <iostream>

namespace monitor {

/**
 * @brief         ManagerMetrics
 * 结构体用于跟踪和记录管理器的各种性能指标和错误统计信息。这些指标包括工作请求数量、查询请求数量、队列拒绝次数、监控样本丢失次数、任务超时次数、任务错误次数、MySQL错误次数、Redis错误次数、连接池超时次数和连接池重连次数。通过这些指标，管理器可以监控其运行状态并进行性能分析和故障排除。
 *
 */
struct ManagerMetrics {
    std::atomic<uint64_t> worker_requests{0};         // 工作请求数量
    std::atomic<uint64_t> query_requests{0};          // 查询请求数量
    std::atomic<uint64_t> queue_rejected{0};          // 队列拒绝次数
    std::atomic<uint64_t> dropped_monitor_samples{0}; // 监控样本丢失次数
    std::atomic<uint64_t> task_timeouts{0};           // 任务超时次数
    std::atomic<uint64_t> task_errors{0};             // 任务错误次数
    std::atomic<uint64_t> mysql_errors{0};            // MySQL错误次数
    std::atomic<uint64_t> redis_errors{0};            // Redis错误次数
    std::atomic<uint64_t> pool_timeouts{0};           // 连接池超时次数
    std::atomic<uint64_t> pool_reconnects{0};         // 连接池重连次数

    /**
     * @brief         print
     * 打印当前的管理器性能指标和错误统计信息。这些信息包括工作请求数量、查询请求数量、队列大小、业务线程数量、MySQL写操作可用性、MySQL查询可用性、Redis可用性、队列拒绝次数、监控样本丢失次数、任务超时次数、任务错误次数、MySQL错误次数、Redis错误次数、连接池超时次数和连接池重连次数。通过这些信息，用户可以了解管理器的当前状态和性能表现。
     *
     * @param         queueSize
     * @param         businessThreads
     * @param         mysqlWriteAvailable
     * @param         mysqlQueryAvailable
     * @param         redisAvailable
     */
    void print(std::size_t queueSize, int businessThreads, int mysqlWriteAvailable, int mysqlQueryAvailable,
               int redisAvailable) const {
        std::cout << "[manager.metrics] worker_requests=" << worker_requests.load()
                  << " query_requests=" << query_requests.load() << " queue_size=" << queueSize
                  << " business_threads=" << businessThreads << " mysql_write_available=" << mysqlWriteAvailable
                  << " mysql_query_available=" << mysqlQueryAvailable << " redis_available=" << redisAvailable
                  << " queue_rejected=" << queue_rejected.load()
                  << " dropped_monitor_samples=" << dropped_monitor_samples.load()
                  << " task_timeouts=" << task_timeouts.load() << " task_errors=" << task_errors.load()
                  << " mysql_errors=" << mysql_errors.load() << " redis_errors=" << redis_errors.load()
                  << " pool_timeouts=" << pool_timeouts.load() << " pool_reconnects=" << pool_reconnects.load()
                  << std::endl;
    }
};

} // namespace monitor
