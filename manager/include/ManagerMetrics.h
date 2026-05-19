#pragma once

#include <atomic>
#include <cstdint>
#include <iostream>

namespace monitor {

/**
 * @brief         管理器指标结构体，用于跟踪和记录管理器的性能指标和错误统计信息
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
     * @brief         打印当前的管理器性能指标和错误统计信息
     *
     * @param         queueSize 当前队列长度
     * @param         businessThreads 当前业务线程数量
     * @param         mysqlWriteAvailable MySQL 写连接池可用连接数
     * @param         mysqlQueryAvailable MySQL 查询连接池可用连接数
     * @param         redisAvailable Redis 连接池可用连接数
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
