#pragma once
#include "ManagerConfig.h"
#include "ManagerMetrics.h"
#include "MysqlConnectionPool.h"
#include "RedisConnectionPool.h"
#include "WorkerIdentity.h"
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
 * @brief         主机评分结构体，保存主机监控数据、计算出的综合评分和最后更新时间
 */
struct HostScore {
    monitor::proto::MonitorInfo info;
    double score;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief         主机监控落库数据结构体，保存主机指标、评分和各类变化率，供 MySQL 持久化和后续分析使用
 *
 */
struct HostMonitoringData {
    std::string host_name;
    WorkerScope scope;
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
     * @brief         配置 HostManager 所需参数和依赖组件
     *
     * @param         config 管理器配置
     * @param         metrics 监控指标
     * @param         mysqlWritePool MySQL 写连接池
     * @param         redisCache Redis 缓存
     */
    void configure(const ManagerConfig &config, ManagerMetrics *metrics, MysqlConnectionPool *mysqlWritePool,
                   RedisCache *redisCache);

    /**
     * @brief         启动 HostManager 后台处理线程
     *
     */
    void start();

    /**
     * @brief         停止 HostManager 后台处理线程
     *
     */
    void stop();

    /**
     * @brief         接收 worker 推送的监控数据并更新主机评分
     *
     * @param         info 监控数据
     */
    void onDataReceived(const monitor::proto::MonitorInfo &info, const WorkerIdentity &workerIdentity);

    /**
     * @brief         获取所有主机评分快照
     *
     * @return        主机名到评分数据的映射
     */
    std::unordered_map<std::string, HostScore> getAllHostScores();

    /**
     * @brief         获取当前评分最高的主机名
     *
     * @return        最优主机名
     */
    std::string getBestHost();

private:
    /**
     * @brief         后台循环处理主机评分和过期数据
     *
     */
    void processLoop();

    /**
     * @brief         根据主机监控数据计算综合评分
     *
     * @param         info 主机监控数据
     * @return        综合评分
     */
    double calculateScore(const monitor::proto::MonitorInfo &info);

    /**
     * @brief         将主机评分和监控数据写入 MySQL，供持久化和后续分析使用
     *
     */
    void writeToMysql(HostMonitoringData &data);

    /**
     * @brief         根据 worker 注册信息解析可信租户、团队、集群和服务器归属
     *
     * @param         workerIdentity worker 推送携带的身份和凭证
     * @param         scope 输出的可信作用域
     * @return        解析成功返回 true，否则返回 false
     */
    bool resolveWorkerScope(const WorkerIdentity &workerIdentity, WorkerScope *scope);

    /**
     * @brief         将主机监控数据加入 MySQL 写队列，等待后台线程异步写入
     *
     * @param         data 主机监控落库数据
     */
    void enqueueMysqlWrite(HostMonitoringData data);

    /**
     * @brief         后台循环处理 MySQL 写队列并持久化主机监控数据
     *
     */
    void mysqlWriteLoop();

    std::unordered_map<std::string, HostScore> hostScores_; // 键为主机名，值为主机评分数据
    std::mutex mutex_;                                      // 保护 hostScores_ 的互斥锁
    std::atomic<bool> running_;                             // 控制后台处理循环运行状态的标记
    std::unique_ptr<std::thread> thread_;                   // 主机评分处理线程
    ManagerConfig config_;                                  // HostManager 配置参数
    ManagerMetrics *metrics_ = nullptr;                     // 监控指标对象指针
    MysqlConnectionPool *mysqlWritePool_ = nullptr;         // MySQL 写连接池指针，用于写入主机监控数据
    RedisCache *redisCache_ = nullptr;                      // Redis 缓存指针，用于缓存主机评分和相关信息

    std::deque<HostMonitoringData> mysqlWriteQueue_; // 待写入 MySQL 的主机监控数据队列
    std::mutex mysqlWriteQueueMutex_;                // 保护 MySQL 写队列的互斥锁
    std::condition_variable mysqlWriteQueueCv_;      // 通知 MySQL 写线程有新数据入队的条件变量
    std::vector<std::thread> mysqlWriteThreads_;     // 处理 MySQL 写队列的线程列表
    std::mutex mysqlWriteMutex_;                     // 保护 MySQL 写线程列表的互斥锁
};

}; // namespace monitor
