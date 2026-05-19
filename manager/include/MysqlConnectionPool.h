#pragma once

#include "ManagerConfig.h"
#include "ManagerMetrics.h"
#include "MysqlConfig.h"

#include <mysql.h>

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace monitor {

/**
 * @brief         mysql 连接池
 *
 */
class MysqlConnectionPool {
public:
    /**
     * @brief         连接池的守护类，负责自动释放连接回池中
     *
     */
    class Guard {
    public:
        Guard() = default;
        Guard(MysqlConnectionPool *pool, std::size_t index, MYSQL *conn);
        Guard(const Guard &) = delete;
        Guard &operator=(const Guard &) = delete;
        Guard(Guard &&other) noexcept;
        Guard &operator=(Guard &&other) noexcept;
        ~Guard();

        /**
         * @brief         获取 mysql 连接对象
         *
         * @return        MySQL 连接指针
         */
        MYSQL *get() const { return conn_; }

        /**
         * @brief         判断连接是否有效（即是否成功获取到连接）
         *
         * @return        连接有效返回 true，否则返回 false
         */
        explicit operator bool() const { return conn_ != nullptr; }

        /**
         * @brief         标记当前连接为不健康，连接池将会在释放连接时关闭该连接而不是重用它
         *
         */
        void markUnhealthy();

    private:
        /**
         * @brief         释放连接回连接池，如果连接被标记为不健康，则连接池会关闭该连接而不是重用它
         *
         */
        void release();

        MysqlConnectionPool *pool_ = nullptr;
        std::size_t index_ = 0;
        MYSQL *conn_ = nullptr;
        bool healthy_ = true;
    };

    MysqlConnectionPool(MysqlConfig mysqlConfig, int minSize, int maxSize, std::chrono::milliseconds connectTimeout,
                        std::chrono::milliseconds readTimeout, std::chrono::seconds healthCheckInterval,
                        std::chrono::seconds idleTtl, ManagerMetrics *metrics);
    ~MysqlConnectionPool();

    /**
     * @brief         启动连接池，预先创建最小数量的连接，并启动健康检查线程
     *
     * @return        启动成功返回 true，否则返回 false
     */
    bool start();

    /**
     * @brief         停止连接池，关闭所有连接，并停止健康检查线程
     *
     */
    void stop();

    /**
     * @brief         获取一个可用的 mysql 连接，如果没有可用连接，则等待直到有连接可用或超时
     *
     * @param         timeout 获取连接的等待超时时间
     * @return        连接守护对象
     */
    Guard acquire(std::chrono::milliseconds timeout);

    /**
     * @brief         获取当前连接池中可用连接的数量
     *
     * @return        可用连接数量
     */
    int availableCount() const;

private:
    /**
     * @brief         连接池中的每个条目，包含一个 mysql 连接和相关状态信息
     *
     */
    struct Entry {
        MYSQL *conn = nullptr;                          // mysql 连接对象
        bool inUse = false;                             // 连接是否正在被使用
        bool healthy = false;                           // 连接是否健康
        int failures = 0;                               // 连接失败次数，用于健康检查
        std::chrono::steady_clock::time_point lastUsed; // 上次使用时间，用于空闲连接的存活时间检查
    };

    /**
     * @brief         创建一个新的 mysql 连接，并将其添加到连接池中
     *
     * @return        新建的 MySQL 连接指针，失败时返回 nullptr
     */
    MYSQL *connect();

    /**
     * @brief         确保指定索引的连接是健康的，如果连接不可用或不健康，则尝试重新连接
     *
     * @param         index 连接条目索引
     * @return        连接健康或重连成功返回 true，否则返回 false
     */
    bool ensureHealthy(std::size_t index);

    /**
     * @brief         释放指定索引的连接，如果连接健康则将其标记为可用，否则关闭连接并从池中移除
     *
     * @param         index
     * @param         healthy
     */
    void release(std::size_t index, bool healthy);

    /**
     * @brief         关闭指定条目的连接，并从池中移除该条目
     *
     * @param         entry
     */
    void closeEntry(Entry &entry);

    /**
     * @brief         连接池的健康检查循环，定期检查所有连接的健康状态，并关闭不健康或空闲时间过长的连接
     *
     */
    void healthLoop();

    MysqlConfig mysqlConfig_;                  // mysql 连接配置
    int minSize_ = 0;                          // 连接池最小连接数
    int maxSize_ = 0;                          // 连接池最大连接数
    std::chrono::milliseconds connectTimeout_; // 连接超时时间
    std::chrono::milliseconds readTimeout_;    // 读超时时间
    std::chrono::seconds healthCheckInterval_; // 健康检查间隔
    std::chrono::seconds idleTtl_;             // 空闲连接的存活时间
    ManagerMetrics *metrics_ = nullptr;        // 监控指标
    mutable std::mutex mutex_;                 // 保护连接池状态的互斥锁
    std::condition_variable cv_;               // 用于等待可用连接的条件变量
    std::vector<Entry> entries_;               // 连接池中的连接条目
    bool running_ = false;                     // 连接池是否正在运行
    std::thread healthThread_;                 // 健康检查线程
};

} // namespace monitor
