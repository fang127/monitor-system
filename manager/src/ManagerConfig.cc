#include "ManagerConfig.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <thread>

namespace monitor {
namespace {

/**
 * @brief         从环境变量中读取整数值，支持指定默认值和最小值。
 *
 * @param         name
 * @param         fallback
 * @param         minValue
 * @return
 */
int envInt(const char *name, int fallback, int minValue = 0) {
    const char *value = std::getenv(name);
    if (!value) return fallback;
    char *end = nullptr;
    // 使用 strtol 解析整数，支持错误检查
    // end 将指向解析结束的位置，如果没有解析出任何数字，end 将等于 value
    long parsed = std::strtol(value, &end, 10);
    if (end == value || *end != '\0') return fallback;
    return std::max(static_cast<int>(parsed), minValue);
}

/**
 * @brief         从环境变量中读取大小值（以字节为单位），支持指定默认值和最小值。
 *
 * @param         name
 * @param         fallback
 * @param         minValue
 * @return
 */
std::size_t envSize(const char *name, std::size_t fallback, std::size_t minValue = 0) {
    return static_cast<std::size_t>(envInt(name, static_cast<int>(fallback), static_cast<int>(minValue)));
}

/**
 * @brief         从环境变量中读取布尔值，支持指定默认值。解析时忽略大小写，接受 "1", "true", "yes", "on" 作为 true。
 *
 * @param         name
 * @param         fallback
 * @return
 * @return
 */
bool envBool(const char *name, bool fallback) {
    const char *value = std::getenv(name);
    if (!value) return fallback;
    std::string text(value);
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::tolower(c); });
    return text == "1" || text == "true" || text == "yes" || text == "on";
}

/**
 * @brief         从环境变量中读取字符串值，支持指定默认值。
 *
 * @param         name
 * @param         fallback
 * @return
 */
std::string envString(const char *name, const std::string &fallback) {
    const char *value = std::getenv(name);
    return value ? std::string(value) : fallback;
}

} // namespace

/**
 * @brief         从环境变量中加载 ManagerConfig
 * 配置，支持指定默认值和合理的最小值。对于线程数和连接池大小等参数，默认值会根据系统 CPU
 * 核心数进行调整，以适应不同的运行环境。
 *
 * @return
 */
ManagerConfig loadManagerConfigFromEnv() {
    ManagerConfig cfg;
    const int cpu = std::max(1, static_cast<int>(std::thread::hardware_concurrency()));

    cfg.grpc_num_cqs = envInt("MANAGER_GRPC_NUM_CQS", cfg.grpc_num_cqs, 1);
    cfg.grpc_min_pollers = envInt("MANAGER_GRPC_MIN_POLLERS", cfg.grpc_min_pollers, 1);
    cfg.grpc_max_pollers = envInt("MANAGER_GRPC_MAX_POLLERS", cfg.grpc_max_pollers, cfg.grpc_min_pollers);

    cfg.task_queue_capacity = envSize("MANAGER_TASK_QUEUE_CAPACITY", cfg.task_queue_capacity, 1);
    cfg.task_queue_high_watermark = envSize("MANAGER_TASK_QUEUE_HIGH_WATERMARK", cfg.task_queue_capacity * 8 / 10, 1);
    cfg.task_queue_low_watermark = envSize("MANAGER_TASK_QUEUE_LOW_WATERMARK", cfg.task_queue_capacity * 3 / 10, 0);
    /* 如果高水位线超过了队列容量，调整为容量的 80%；如果低水位线超过了高水位线，调整为高水位线的一半。 */
    if (cfg.task_queue_high_watermark > cfg.task_queue_capacity)
        cfg.task_queue_high_watermark = cfg.task_queue_capacity * 0.8;
    if (cfg.task_queue_low_watermark > cfg.task_queue_high_watermark)
        cfg.task_queue_low_watermark = cfg.task_queue_high_watermark / 2;
    cfg.task_timeout =
        std::chrono::milliseconds(envInt("MANAGER_TASK_TIMEOUT_MS", static_cast<int>(cfg.task_timeout.count()), 1));

    cfg.business_threads_min = envInt("MANAGER_BUSINESS_THREADS_MIN", std::max(4, cpu), 1);
    cfg.business_threads_max =
        envInt("MANAGER_BUSINESS_THREADS_MAX", std::max(cfg.business_threads_min, cpu * 2), cfg.business_threads_min);
    cfg.business_idle_shrink = std::chrono::seconds(
        envInt("MANAGER_BUSINESS_IDLE_SHRINK_SECONDS", static_cast<int>(cfg.business_idle_shrink.count()), 1));

    cfg.mysql_write_pool_min = envInt("MANAGER_MYSQL_WRITE_POOL_MIN", cfg.mysql_write_pool_min, 0);
    cfg.mysql_write_pool_max =
        envInt("MANAGER_MYSQL_WRITE_POOL_MAX", cfg.mysql_write_pool_max, cfg.mysql_write_pool_min);
    cfg.mysql_query_pool_min = envInt("MANAGER_MYSQL_QUERY_POOL_MIN", cfg.mysql_query_pool_min, 0);
    cfg.mysql_query_pool_max =
        envInt("MANAGER_MYSQL_QUERY_POOL_MAX", cfg.mysql_query_pool_max, cfg.mysql_query_pool_min);
    cfg.mysql_connect_timeout = std::chrono::milliseconds(
        envInt("MANAGER_MYSQL_CONNECT_TIMEOUT_MS", static_cast<int>(cfg.mysql_connect_timeout.count()), 1));
    cfg.mysql_read_timeout = std::chrono::milliseconds(
        envInt("MANAGER_MYSQL_READ_TIMEOUT_MS", static_cast<int>(cfg.mysql_read_timeout.count()), 1));
    cfg.mysql_health_check = std::chrono::seconds(
        envInt("MANAGER_MYSQL_HEALTH_CHECK_SECONDS", static_cast<int>(cfg.mysql_health_check.count()), 1));
    cfg.mysql_idle_ttl =
        std::chrono::seconds(envInt("MANAGER_MYSQL_IDLE_TTL_SECONDS", static_cast<int>(cfg.mysql_idle_ttl.count()), 1));

    cfg.redis_enabled = envBool("MANAGER_REDIS_ENABLED", cfg.redis_enabled);
    cfg.redis_uri = envString("MANAGER_REDIS_URI", cfg.redis_uri);
    cfg.redis_pool_min = envInt("MANAGER_REDIS_POOL_MIN", cfg.redis_pool_min, 0);
    cfg.redis_pool_max = envInt("MANAGER_REDIS_POOL_MAX", cfg.redis_pool_max, cfg.redis_pool_min);
    cfg.redis_connect_timeout = std::chrono::milliseconds(
        envInt("MANAGER_REDIS_CONNECT_TIMEOUT_MS", static_cast<int>(cfg.redis_connect_timeout.count()), 1));
    cfg.redis_read_timeout = std::chrono::milliseconds(
        envInt("MANAGER_REDIS_READ_TIMEOUT_MS", static_cast<int>(cfg.redis_read_timeout.count()), 1));
    cfg.redis_health_check = std::chrono::seconds(
        envInt("MANAGER_REDIS_HEALTH_CHECK_SECONDS", static_cast<int>(cfg.redis_health_check.count()), 1));
    cfg.redis_idle_ttl =
        std::chrono::seconds(envInt("MANAGER_REDIS_IDLE_TTL_SECONDS", static_cast<int>(cfg.redis_idle_ttl.count()), 1));
    cfg.redis_cache_ttl = std::chrono::seconds(
        envInt("MANAGER_REDIS_CACHE_TTL_SECONDS", static_cast<int>(cfg.redis_cache_ttl.count()), 1));

    cfg.verbose_log = envBool("MANAGER_VERBOSE_LOG", cfg.verbose_log);
    cfg.metrics_log_interval = std::chrono::seconds(
        envInt("MANAGER_METRICS_LOG_INTERVAL_SECONDS", static_cast<int>(cfg.metrics_log_interval.count()), 1));
    return cfg;
}

} // namespace monitor
