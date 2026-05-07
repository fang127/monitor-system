#pragma once

#include "ManagerConfig.h"
#include "ManagerMetrics.h"

#include <sw/redis++/redis++.h>

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace monitor {

/**
 * @brief         Redis 连接池，支持连接健康检查和自动重连
 *
 */
class RedisConnectionPool {
public:
    /**
     * @brief         连接池的 Guard 类，RAII 风格管理连接的生命周期，自动释放连接回池中，并支持标记连接为不健康
     *
     */
    class Guard {
    public:
        Guard() = default;
        Guard(RedisConnectionPool *pool, std::size_t index, sw::redis::Redis *conn);
        Guard(const Guard &) = delete;
        Guard &operator=(const Guard &) = delete;
        Guard(Guard &&other) noexcept;
        Guard &operator=(Guard &&other) noexcept;
        ~Guard();

        /**
         * @brief         获取 Redis 连接对象，使用前请检查 Guard 是否有效（operator bool()），如果连接不健康请调用
         * markUnhealthy() 标记连接为不健康，Guard 会在析构时自动释放连接回池中
         *
         * @return
         */
        sw::redis::Redis *get() const { return conn_; }

        /**
         * @brief         检查 Guard 是否持有有效的连接对象，如果连接不健康请调用 markUnhealthy()
         * 标记连接为不健康，Guard 会在析构时自动释放连接回池中
         *
         * @return
         * @return
         */
        explicit operator bool() const { return conn_ != nullptr; }

        /**
         * @brief         标记连接为不健康，Guard
         * 会在析构时自动释放连接回池中，并且连接池会在下次获取连接时进行健康检查，必要时关闭连接并创建新连接替代
         *
         */
        void markUnhealthy();

    private:
        /**
         * @brief         释放连接回池中，如果连接不健康会被池子关闭并替换掉，Guard 会在析构时自动调用 release()
         * 释放连接回池中，如果连接不健康会被池子关闭并替换掉
         *
         */
        void release();

        RedisConnectionPool *pool_ = nullptr;
        std::size_t index_ = 0;
        sw::redis::Redis *conn_ = nullptr;
        bool healthy_ = true;
    };

    RedisConnectionPool(std::string uri, int minSize, int maxSize, std::chrono::milliseconds connectTimeout,
                        std::chrono::milliseconds readTimeout, std::chrono::seconds healthCheckInterval,
                        std::chrono::seconds idleTtl, ManagerMetrics *metrics);
    ~RedisConnectionPool();

    /**
     * @brief         启动连接池，创建初始连接并启动健康检查线程
     *
     * @return
     * @return
     */
    bool start();

    /**
     * @brief         停止连接池，关闭所有连接并停止健康检查线程
     *
     */
    void stop();

    /**
     * @brief         获取连接，返回一个 Guard 对象，Guard 会在析构时自动释放连接回池中，如果连接不健康请调用
     * markUnhealthy() 标记连接为不健康，Guard
     * 会在析构时自动释放连接回池中，并且连接池会在下次获取连接时进行健康检查，必要时关闭连接并创建新连接替代
     *
     * @param         timeout
     * @return
     */
    Guard acquire(std::chrono::milliseconds timeout);

    /**
     * @brief         获取当前可用连接数，供监控使用
     *
     * @return
     */
    int availableCount() const;

private:
    /**
     * @brief         连接池中的每个连接条目，包含 Redis
     * 连接对象、使用状态、健康状态、失败次数和上次使用时间等信息，用于连接池管理和健康检查
     *
     */
    struct Entry {
        std::unique_ptr<sw::redis::Redis> conn; // 连接对象，使用 unique_ptr 管理生命周期
        bool inUse = false;                     // 连接是否正在被使用
        bool healthy = false;                   // 连接是否健康，初始为 false，只有成功创建连接后才会设置为 true
        int failures = 0; // 连接失败次数，健康检查失败时递增，成功时重置为 0，用于判断连接是否需要被关闭和替换
        std::chrono::steady_clock::time_point
            lastUsed; // 上次使用时间，用于判断连接是否空闲超过 idleTtl 需要被关闭和替换
    };

    /**
     * @brief         创建新的 Redis 连接对象，连接池内部使用，成功创建连接后会将对应 Entry 标记为健康状态
     *
     * @return
     */
    std::unique_ptr<sw::redis::Redis> connect();

    /**
     * @brief         确保指定索引的连接条目处于健康状态，如果连接不健康会被关闭并替换掉，返回是否成功获取到健康连接
     *
     * @param         index
     * @return
     * @return
     */
    bool ensureHealthy(std::size_t index);

    /**
     * @brief         释放连接回池中，如果连接不健康会被关闭并替换掉，Guard 会在析构时自动调用 release()
     * 释放连接回池中，如果连接不健康会被关闭并替换掉
     *
     * @param         index
     * @param         healthy
     */
    void release(std::size_t index, bool healthy);

    /**
     * @brief
     * 关闭指定索引的连接条目，如果连接不健康会被关闭并替换掉，连接池会在下次获取连接时进行健康检查，必要时关闭连接并创建新连接替代
     *
     * @param         entry
     */
    void closeEntry(Entry &entry);

    /**
     * @brief
     * 连接池的健康检查循环线程函数，定期检查所有连接条目的健康状态和空闲状态，如果连接不健康会被关闭并替换掉，连接池会在下次获取连接时进行健康检查，必要时关闭连接并创建新连接替代
     *
     */
    void healthLoop();

    std::string uri_;                          // Redis 连接 URI，包含主机、端口、认证等信息，用于创建 Redis 连接对象
    int minSize_ = 0;                          // 连接池的最小连接数
    int maxSize_ = 0;                          // 连接池的最大连接数
    std::chrono::milliseconds connectTimeout_; // 连接超时时间，用于创建 Redis 连接对象时的连接超时设置
    std::chrono::milliseconds readTimeout_;    // 读超时时间，用于创建 Redis 连接对象时的读超时设置
    std::chrono::seconds healthCheckInterval_; // 健康检查间隔时间
    std::chrono::seconds idleTtl_;             // 连接空闲过期时间
    ManagerMetrics *metrics_ = nullptr;        // 连接池相关的监控指标对象，用于记录连接池的状态和性能指标
    mutable std::mutex mutex_;                 // 保护连接池状态的互斥锁，确保多线程环境下对连接池状态的安全访问
    std::condition_variable cv_;               // 条件变量，用于在获取连接时等待有可用连接或者连接池状态发生变化
    std::vector<Entry> entries_;               // 连接池中的连接条目列表
    bool running_ = false;                     // 连接池是否正在运行，控制健康检查线程的生命周期
    std::thread healthThread_;                 // 健康检查线程对象
};

/**
 * @brief         Redis 缓存类，封装了 Redis 连接池的使用，提供简单的 set/get
 * 接口，并且在获取连接时自动处理连接的健康状态，如果连接不健康会被标记为不健康并且连接池会在下次获取连接时进行健康检查，必要时关闭连接并创建新连接替代
 *
 */
class RedisCache {
public:
    RedisCache(RedisConnectionPool *pool, std::chrono::seconds ttl, ManagerMetrics *metrics);

    /**
     * @brief         设置 Redis 键值对
     *
     * @param         key
     * @param         value
     * @return
     * @return
     */
    bool set(const std::string &key, const std::string &value);

    /**
     * @brief         获取 Redis 键值对
     *
     * @param         key
     * @return
     */
    std::optional<std::string> get(const std::string &key);

    /**
     * @brief         获取当前连接池中可用连接数
     *
     * @return
     */
    int availableCount() const;

private:
    RedisConnectionPool *pool_ = nullptr; // Redis 连接池对象指针，用于获取 Redis 连接对象，进行 Redis 操作
    std::chrono::seconds ttl_;            // 缓存项的过期时间，设置 Redis 键值对时使用，用于控制缓存项的生命周期
    ManagerMetrics *metrics_ = nullptr;   // 监控指标对象指针
};

} // namespace monitor
