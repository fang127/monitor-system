#pragma once

#include <chrono>
#include <cstddef>
#include <string>

namespace monitor {

/**
 * @brief
 * manager的配置项，包含gRPC线程池、任务队列、业务线程池、MySQL连接池、Redis连接池等相关配置。
 *
 */
struct ManagerConfig {
    int grpc_num_cqs = 4;      // gRPC完成队列数量，通常设置为CPU核心数的倍数
    int grpc_min_pollers = 8;  // gRPC最小线程数，建议设置为CPU核心数的2倍
    int grpc_max_pollers = 64; // gRPC最大线程数，建议设置为CPU核心数的8倍

    // 任务队列相关配置，控制任务的入队和出队行为，防止过载
    std::size_t task_queue_capacity = 10000;      // 任务队列的最大容量，超过该值后新任务将被拒绝
    std::size_t task_queue_high_watermark = 8000; // 任务队列的高水位线，当队列长度超过该值时，可能会触发流控或报警
    std::size_t task_queue_low_watermark = 3000;  // 任务队列的低水位线，当队列长度降到该值以下时，流控或报警将被解除
    std::chrono::milliseconds task_timeout{3000}; // 任务的超时时间，超过该时间未完成的任务将被取消或重试

    // 业务线程池相关配置，控制业务线程的数量和空闲线程的回收
    int business_threads_min = 4;                  // 业务线程池的最小线程数，建议设置为CPU核心数的2倍
    int business_threads_max = 16;                 // 业务线程池的最大线程数，建议设置为CPU核心数的8倍
    std::chrono::seconds business_idle_shrink{30}; // 业务线程池的空闲线程回收时间，当线程空闲超过该时间时将被回收

    // MySQL连接池相关配置，控制MySQL连接的数量、超时时间和健康检查
    int mysql_write_pool_min = 2;                          // MySQL写连接池的最小连接数，建议设置为CPU核心数的1倍
    int mysql_write_pool_max = 4;                          // MySQL写连接池的最大连接数，建议设置为CPU核心数的4倍
    int mysql_query_pool_min = 4;                          // MySQL读连接池的最小连接数，建议设置为CPU核心数的2倍
    int mysql_query_pool_max = 8;                          // MySQL读连接池的最大连接数，建议设置为CPU核心数的8倍
    std::chrono::milliseconds mysql_connect_timeout{1000}; // MySQL连接的超时时间，超过该时间未连接成功的连接将被放弃
    std::chrono::milliseconds mysql_read_timeout{2000};    // MySQL读取的超时时间，超过该时间未读取完成的连接将被放弃
    std::chrono::seconds mysql_health_check{
        10}; // MySQL连接的健康检查时间间隔，每隔该时间检查一次连接的健康状态，发现异常连接将被重置
    std::chrono::seconds mysql_idle_ttl{60}; // MySQL连接的空闲TTL，当连接空闲超过该时间时将被关闭以释放资源

    // Redis连接池相关配置，控制Redis连接的数量、超时时间和健康检查
    bool redis_enabled = true; // 是否启用Redis连接池，默认为true，如果不需要使用Redis可以设置为false以节省资源
    std::string redis_uri = "tcp://127.0.0.1:6379";       // Redis连接的URI，格式为tcp://host:port，支持IPv4和IPv6地址
    int redis_pool_min = 2;                               // Redis连接池的最小连接数，建议设置为CPU核心数的1倍
    int redis_pool_max = 8;                               // Redis连接池的最大连接数，建议设置为CPU核心数的8倍
    std::chrono::milliseconds redis_connect_timeout{500}; // Redis连接的超时时间，超过该时间未连接成功的连接将被放弃
    std::chrono::milliseconds redis_read_timeout{1000};   // Redis读取的超时时间，超过该时间未读取完成的连接将被放弃
    std::chrono::seconds redis_health_check{
        10}; // Redis连接的健康检查时间间隔，每隔该时间检查一次连接的健康状态，发现异常连接将被重置
    std::chrono::seconds redis_idle_ttl{60}; // Redis连接的空闲TTL，当连接空闲超过该时间时将被关闭以释放资源
    std::chrono::seconds redis_cache_ttl{
        5}; // Redis缓存的TTL，当数据写入Redis后，该数据在Redis中存活的时间，超过该时间后将被自动删除

    // 日志相关配置，控制日志的输出级别和频率
    bool verbose_log = false; // 是否启用详细日志，默认为false，如果需要调试或监控可以设置为true以输出更多的日志信息
    std::chrono::seconds metrics_log_interval{
        10}; // 监控指标日志的输出间隔，每隔该时间输出一次监控指标日志，包含系统资源使用情况、任务队列长度、连接池状态等信息
};

ManagerConfig loadManagerConfigFromEnv();

} // namespace monitor
