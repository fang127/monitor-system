#pragma once

#include <string>

#include "ManagerConfig.h"
#include "ManagerMetrics.h"
#include "MysqlConnectionPool.h"

#include <mysql.h>
#include <chrono>
#include <string>
#include <mutex>
#include <vector>

namespace monitor {
/**
 * @brief         排序方向
 *
 */
enum class SortOrder {
    DESC = 0,
    ASC = 1,
};

/**
 * @brief         服务状态
 *
 */
enum class ServerStatus {
    ONLINE = 0,
    OFFLINE = 1,
};

/**
 * @brief         异常检测阈值配置
 *
 */
struct AnomalyThreshold {
    float cpu_threshold = 80.0f;
    float memory_threshold = 90.0f;
    float disk_threshold = 85.0f;
    float change_rate_threshold = 0.5f; // 50% 变化率
    float mysql_connection_threshold = 90.0f;
    float mysql_replication_lag_threshold = 30.0f;
    float mysql_slow_query_rate_threshold = 1.0f;
    float mysql_lock_wait_rate_threshold = 1.0f;
    float mysql_buffer_pool_hit_threshold = 95.0f;
    float redis_connection_threshold = 90.0f;
    float redis_memory_threshold = 90.0f;
    float redis_hit_rate_threshold = 80.0f;
    float redis_replication_lag_threshold = 30.0f;
    float redis_slowlog_growth_threshold = 1.0f;
};

/**
 * @brief         指标查询时间范围
 *
 */
struct TimeRange {
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
};

/**
 * @brief         查询认证作用域，来自 api_gateway 写入的访问令牌上下文
 *
 */
struct QueryScope {
    std::string tenant_id;
    std::string team_id;
    std::string cluster_id;
};

/**
 * @brief         指定时间点的服务器性能记录
 *
 */
struct PerformanceRecord {
    std::string server_name;                         // 服务器名称
    std::chrono::system_clock::time_point timestamp; // 记录时间戳
    // CPU 使用率指标
    float cpu_percent = 0.0f;      // CPU 使用率百分比
    float usr_percent = 0.0f;      // 用户态 CPU 使用率百分比
    float sys_percent = 0.0f;      // 系统态 CPU 使用率百分比
    float nice_percent = 0.0f;     // nice CPU 使用率百分比
    float idle_percent = 0.0f;     // 空闲 CPU 百分比
    float io_wait_percent = 0.0f;  // I/O 等待 CPU 百分比
    float irq_percent = 0.0f;      // 硬中断 CPU 百分比
    float soft_irq_percent = 0.0f; // 软中断 CPU 百分比
    // CPU 负载
    float load_avg_1 = 0.0f;  // 1 分钟平均负载
    float load_avg_3 = 0.0f;  // 3 分钟平均负载
    float load_avg_15 = 0.0f; // 15 分钟平均负载
    // 内存使用指标
    float mem_used_percent = 0.0f; // 内存使用率百分比
    float mem_total = 0.0f;        // 内存总量，单位字节
    float mem_free = 0.0f;         // 空闲内存，单位字节
    float mem_avail = 0.0f;        // 可用内存，单位字节
    // 磁盘使用指标
    float disk_util_percent = 0.0f; // 磁盘利用率百分比
    // 网络使用指标
    float net_recv_bytes = 0.0f; // 网络接收字节速率
    float net_sent_bytes = 0.0f; // 网络发送字节速率
    // 综合评分
    float score = 0.0f; // 综合性能评分
    // 变化率
    float cpu_percent_rate = 0.0f;       // CPU 使用率变化率
    float mem_used_percent_rate = 0.0f;  // 内存使用率变化率
    float disk_util_percent_rate = 0.0f; // 磁盘利用率变化率
    float load_avg_1_rate = 0.0f;        // 1 分钟平均负载变化率
    float net_recv_bytes_rate = 0.0f;    // 网络接收字节速率变化率
    float net_sent_bytes_rate = 0.0f;    // 网络发送字节速率变化率
};

/**
 * @brief         指定时间点的服务器异常记录
 *
 */
struct AnomalyRecord {
    std::string server_name;                         // 服务器名称
    std::chrono::system_clock::time_point timestamp; // 异常时间戳
    std::string anomaly_type;                        // 异常类型，例如 "CPU"、"Memory"、"Disk"
    std::string severity;                            // 异常级别，例如 "Critical"、"Warning"
    float value = 0.0f;                              // 触发异常的指标值
    float threshold = 0.0f;                          // 被超过的阈值
    std::string metric_name;                         // 触发异常的指标名称
};

/**
 * @brief         服务器性能和状态摘要
 *
 */
struct ServerScoreSummary {
    std::string server_name;                            // 服务器名称
    float score = 0.0f;                                 // 综合性能评分
    std::chrono::system_clock::time_point last_updated; // 最近更新时间戳
    ServerStatus status = ServerStatus::ONLINE;         // 当前服务器状态
    float cpu_percent = 0.0f;                           // 当前 CPU 使用率百分比
    float mem_used_percent = 0.0f;                      // 当前内存使用率百分比
    float disk_util_percent = 0.0f;                     // 当前磁盘利用率百分比
    float load_avg_1 = 0.0f;                            // 当前 1 分钟平均负载
};

/**
 * @brief         集群性能和状态摘要
 *
 */
struct ClusterStats {
    int total_servers = 0;    // 集群服务器总数
    int online_servers = 0;   // 在线服务器数量
    int offline_servers = 0;  // 离线服务器数量
    float avg_score = 0.0f;   // 集群平均性能评分
    float max_score = 0.0f;   // 集群最高性能评分
    float min_score = 0.0f;   // 集群最低性能评分
    std::string best_server;  // 评分最高的服务器名称
    std::string worst_server; // 评分最低的服务器名称
};

/**
 * @brief         指定时间点的服务器网络性能明细记录
 *
 */
struct NetDetailRecord {
    std::string server_name;                         // 服务器名称
    std::string net_name;                            // 网络接口名称
    std::chrono::system_clock::time_point timestamp; // 记录时间戳
    uint64_t err_in = 0;                             // 每秒接收错误数
    uint64_t err_out = 0;                            // 每秒发送错误数
    uint64_t drop_in = 0;                            // 每秒接收丢包数
    uint64_t drop_out = 0;                           // 每秒发送丢包数
    float recv_bytes_rate = 0.0f;                    // 网络接收字节速率变化率
    float sent_bytes_rate = 0.0f;                    // 网络发送字节速率变化率
    float recv_packets_rate = 0.0f;                  // 网络接收包速率变化率
    float sent_packets_rate = 0.0f;                  // 网络发送包速率变化率
};

/**
 * @brief         指定时间点的服务器磁盘性能明细记录
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
 * @brief         指定时间点的服务器内存性能明细记录
 *
 */
struct MemDetailRecord {
    std::string server_name;
    std::chrono::system_clock::time_point timestamp;
    float mem_total = 0.0f; // 内存总量，单位字节
    float mem_free = 0.0f;  // 空闲内存，单位字节
    float mem_avail = 0.0f; // 可用内存，单位字节
    float buffers = 0.0f;   // buffer 占用内存，单位字节
    float cached = 0.0f;    // cache 占用内存，单位字节
    float active = 0.0f;    // 活跃内存，单位字节
    float inactive = 0.0f;  // 非活跃内存，单位字节
    float dirty = 0.0f;     // 脏页内存，单位字节
};

/**
 * @brief         指定时间点的服务器软中断性能明细记录
 *
 */
struct SoftIrqDetailRecord {
    std::string server_name;
    std::string cpu_name; // CPU 名称，"all" 表示汇总，"cpu0"、"cpu1" 等表示单核
    std::chrono::system_clock::time_point timestamp;
    int64_t hi = 0;     // 每秒高优先级软中断数
    int64_t timer = 0;  // 每秒定时器软中断数
    int64_t net_tx = 0; // 每秒网络发送软中断数
    int64_t net_rx = 0; // 每秒网络接收软中断数
    int64_t block = 0;  // 每秒块设备软中断数
    int64_t sched = 0;  // 每秒调度软中断数
};

/**
 * @brief         指定时间点的服务器 MySQL 性能明细记录
 *
 */
struct MysqlDetailRecord {
    std::string server_name;
    std::string instance;
    std::chrono::system_clock::time_point timestamp;
    std::string mysql_host;
    int mysql_port = 0;
    bool up = false;
    std::string version;
    std::string role;
    uint64_t max_connections = 0;
    uint64_t threads_connected = 0;
    uint64_t threads_running = 0;
    uint64_t aborted_connects = 0;
    uint64_t questions = 0;
    uint64_t com_select = 0;
    uint64_t com_insert = 0;
    uint64_t com_update = 0;
    uint64_t com_delete = 0;
    uint64_t com_commit = 0;
    uint64_t com_rollback = 0;
    uint64_t slow_queries = 0;
    uint64_t innodb_buffer_pool_read_requests = 0;
    uint64_t innodb_buffer_pool_reads = 0;
    float innodb_buffer_pool_hit_percent = 0.0f;
    uint64_t innodb_row_lock_waits = 0;
    float innodb_row_lock_time_avg_ms = 0.0f;
    bool replication_configured = false;
    bool replication_running = false;
    float replication_lag_seconds = 0.0f;
    float connection_used_percent = 0.0f;
    float qps = 0.0f;
    float tps = 0.0f;
    float slow_queries_rate = 0.0f;
    float innodb_row_lock_waits_rate = 0.0f;
};

/**
 * @brief         指定时间点的服务器 Redis 性能明细记录
 *
 */
struct RedisDetailRecord {
    std::string server_name;
    std::string instance;
    std::chrono::system_clock::time_point timestamp;
    std::string redis_host;
    int redis_port = 0;
    bool up = false;
    std::string version;
    std::string role;
    uint64_t uptime_in_seconds = 0;
    uint64_t connected_clients = 0;
    uint64_t blocked_clients = 0;
    uint64_t maxclients = 0;
    float connection_used_percent = 0.0f;
    uint64_t used_memory = 0;
    uint64_t maxmemory = 0;
    float mem_fragmentation_ratio = 0.0f;
    float memory_used_percent = 0.0f;
    uint64_t total_commands_processed = 0;
    float instantaneous_ops_per_sec = 0.0f;
    float commands_per_sec = 0.0f;
    uint64_t keyspace_hits = 0;
    uint64_t keyspace_misses = 0;
    float keyspace_hit_percent = 0.0f;
    uint64_t expired_keys = 0;
    uint64_t evicted_keys = 0;
    uint64_t rejected_connections = 0;
    uint64_t total_error_replies = 0;
    uint64_t total_net_input_bytes = 0;
    uint64_t total_net_output_bytes = 0;
    float net_input_bytes_per_sec = 0.0f;
    float net_output_bytes_per_sec = 0.0f;
    bool replication_configured = false;
    bool master_link_up = false;
    uint64_t connected_slaves = 0;
    float master_last_io_seconds_ago = 0.0f;
    uint64_t slowlog_len = 0;
    float slowlog_growth = 0.0f;
};

class QueryManager {
public:
    QueryManager() = default;
    ~QueryManager() { close(); }

    /**
     * @brief         使用 MySQL 连接池、配置和指标对象初始化 QueryManager
     *
     * @param         queryPool MySQL 查询连接池
     * @param         config 管理器配置
     * @param         metrics 监控指标
     * @return        初始化成功返回 true，否则返回 false
     */
    bool init(MysqlConnectionPool *queryPool = nullptr, const ManagerConfig *config = nullptr,
              ManagerMetrics *metrics = nullptr);

    /**
     * @brief         关闭数据库连接并清理资源
     *
     */
    void close();

    /**
     * @brief         检查数据库查询组件是否已经完成初始化
     *
     * @return        已初始化返回 true，否则返回 false
     */
    bool isInitialized() const;

    /**
     * @brief         校验指标查询时间范围是否合法
     *
     * @param         range 待校验的时间范围
     * @return        时间范围合法返回 true，否则返回 false
     */
    bool validateTimeRange(const TimeRange &range) const;

    /**
     * @brief         查询指定服务器在给定时间范围内的性能记录
     *
     * @param         serverName 服务器名称
     * @param         range 查询时间范围
     * @param         page 页码
     * @param         pageSize 每页记录数
     * @param         totalCount 输出参数，保存总记录数
     * @param         error 输出参数，保存错误信息
     * @return        性能记录列表
     */
    std::vector<PerformanceRecord> queryPerformanceRecords(const QueryScope &scope, const std::string &serverName,
                                                           const TimeRange &range, int page, int pageSize, int *totalCount,
                                                           std::string *error = nullptr);

    /**
     * @brief         查询指定服务器在给定时间范围内的趋势数据
     *
     * @param         serverName 服务器名称
     * @param         range 查询时间范围
     * @param         intervalSeconds 聚合间隔，单位秒
     * @param         error 输出参数，保存错误信息
     * @return        趋势性能记录列表
     */
    std::vector<PerformanceRecord> queryTrend(const QueryScope &scope, const std::string &serverName, const TimeRange &range,
                                              int intervalSeconds, std::string *error = nullptr);

    /**
     * @brief         查询指定服务器在给定时间范围内的异常记录
     *
     * @param         serverName 服务器名称
     * @param         range 查询时间范围
     * @param         threshold 异常阈值
     * @param         page 页码
     * @param         pageSize 每页记录数
     * @param         totalCount 输出参数，保存总记录数
     * @param         error 输出参数，保存错误信息
     * @return        异常记录列表
     */
    std::vector<AnomalyRecord> queryAnomalyRecords(const QueryScope &scope, const std::string &serverName,
                                                   const TimeRange &range, const AnomalyThreshold &threshold, int page, int pageSize,
                                                   int *totalCount, std::string *error = nullptr);

    /**
     * @brief         按分页和排序条件查询服务器评分摘要
     *
     * @param         order 排序方向
     * @param         page 页码
     * @param         pageSize 每页记录数
     * @param         totalCount 输出参数，保存总记录数
     * @param         error 输出参数，保存错误信息
     * @return        服务器评分摘要列表
     */
    std::vector<ServerScoreSummary> queryServerScoreRank(const QueryScope &scope, SortOrder order, int page, int pageSize, int *totalCount,
                                                         std::string *error = nullptr);

    /**
     * @brief         查询最新服务器评分和集群统计信息
     *
     * @param         clusterStats 输出参数，保存集群统计信息
     * @param         error 输出参数，保存错误信息
     * @return        最新服务器评分摘要列表
     */
    std::vector<ServerScoreSummary> queryLatestServerScores(const QueryScope &scope, ClusterStats *clusterStats,
                                                            std::string *error = nullptr);

    /**
     * @brief         查询网络明细统计
     *
     * @param         serverName 服务器名称
     * @param         range 查询时间范围
     * @param         page 页码
     * @param         pageSize 每页记录数
     * @param         totalCount 输出参数，保存总记录数
     * @param         error 输出参数，保存错误信息
     * @return        网络明细记录列表
     */
    std::vector<NetDetailRecord> queryNetDetailRecords(const QueryScope &scope, const std::string &serverName, const TimeRange &range, int page,
                                                       int pageSize, int *totalCount, std::string *error = nullptr);

    /**
     * @brief         查询磁盘明细统计
     *
     * @param         serverName 服务器名称
     * @param         range 查询时间范围
     * @param         page 页码
     * @param         pageSize 每页记录数
     * @param         totalCount 输出参数，保存总记录数
     * @param         error 输出参数，保存错误信息
     * @return        磁盘明细记录列表
     */
    std::vector<DiskDetailRecord> queryDiskDetailRecords(const QueryScope &scope, const std::string &serverName, const TimeRange &range,
                                                         int page, int pageSize, int *totalCount,
                                                         std::string *error = nullptr);

    /**
     * @brief         查询内存明细统计
     *
     * @param         serverName 服务器名称
     * @param         range 查询时间范围
     * @param         page 页码
     * @param         pageSize 每页记录数
     * @param         totalCount 输出参数，保存总记录数
     * @param         error 输出参数，保存错误信息
     * @return        内存明细记录列表
     */
    std::vector<MemDetailRecord> queryMemDetailRecords(const QueryScope &scope, const std::string &serverName, const TimeRange &range, int page,
                                                       int pageSize, int *totalCount, std::string *error = nullptr);

    /**
     * @brief         查询软中断明细统计
     *
     * @param         serverName 服务器名称
     * @param         range 查询时间范围
     * @param         page 页码
     * @param         pageSize 每页记录数
     * @param         totalCount 输出参数，保存总记录数
     * @param         error 输出参数，保存错误信息
     * @return        软中断明细记录列表
     */
    std::vector<SoftIrqDetailRecord> querySoftIrqDetailRecords(const QueryScope &scope, const std::string &serverName,
                                                               const TimeRange &range, int page, int pageSize, int *totalCount,
                                                               std::string *error = nullptr);

    /**
     * @brief         查询 MySQL 明细统计
     *
     * @param         serverName 服务器名称
     * @param         range 查询时间范围
     * @param         page 页码
     * @param         pageSize 每页记录数
     * @param         totalCount 输出参数，保存总记录数
     * @param         error 输出参数，保存错误信息
     * @return        MySQL 明细记录列表
     */
    std::vector<MysqlDetailRecord> queryMysqlDetailRecords(const QueryScope &scope, const std::string &serverName, const TimeRange &range,
                                                           int page, int pageSize, int *totalCount,
                                                           std::string *error = nullptr);

    /**
     * @brief         查询 Redis 明细统计
     *
     * @param         serverName 服务器名称
     * @param         range 查询时间范围
     * @param         page 页码
     * @param         pageSize 每页记录数
     * @param         totalCount 输出参数，保存总记录数
     * @param         error 输出参数，保存错误信息
     * @return        Redis 明细记录列表
     */
    std::vector<RedisDetailRecord> queryRedisDetailRecords(const QueryScope &scope, const std::string &serverName, const TimeRange &range,
                                                           int page, int pageSize, int *totalCount,
                                                           std::string *error = nullptr);

private:
#ifdef ENABLE_MYSQL
    /**
     * @brief      MySQL 连接池租约的 RAII 包装，离开作用域时自动释放连接
     *
     */
    struct MysqlConnectionLease {
        MysqlConnectionPool::Guard guard;
        MYSQL *conn = nullptr;
    };

    /**
     * @brief         从连接池获取一个 MySQL 查询连接
     *
     * @param         error 输出参数，保存错误信息
     * @return        MySQL 连接租约
     */
    MysqlConnectionLease acquireConnection(std::string *error);
#endif

    /**
     * @brief         将 time_point 格式化为适合 SQL 查询的时间字符串
     *
     * @param         tp 时间点
     * @return        SQL 时间字符串
     */
    std::string formatTimePoint(const std::chrono::system_clock::time_point &tp) const;

    /**
     * @brief         将 SQL 查询结果中的时间字符串解析为 time_point
     *
     * @param         timeStr SQL 时间字符串
     * @return        系统时间点
     */
    std::chrono::system_clock::time_point parseTimeString(const std::string &timeStr) const;

#ifdef ENABLE_MYSQL
    MysqlConnectionPool *queryPool_ = nullptr; // 用于执行查询的 MySQL 连接池
#endif
    ManagerConfig config_;              // QueryManager 配置参数
    ManagerMetrics *metrics_ = nullptr; // 记录查询性能指标的 ManagerMetrics 指针
    std::mutex mutex_;                  // 同步共享资源访问的互斥锁
    bool initialized_ = false;          // 标记 QueryManager 是否已经初始化
};

} // namespace monitor
