#include "MysqlConnectionPool.h"

#include <algorithm>
#include <iostream>

namespace monitor {
namespace {

/**
 * @brief         转换毫秒为秒，向上取整，最小为1秒
 *
 * @param         timeout
 * @return
 */
unsigned int toSeconds(std::chrono::milliseconds timeout) {
    return static_cast<unsigned int>(std::max<long long>(1, (timeout.count() + 999) / 1000));
}

} // namespace

MysqlConnectionPool::Guard::Guard(MysqlConnectionPool *pool, std::size_t index, MYSQL *conn)
    : pool_(pool), index_(index), conn_(conn) {}

MysqlConnectionPool::Guard::Guard(Guard &&other) noexcept {
    pool_ = other.pool_;
    index_ = other.index_;
    conn_ = other.conn_;
    healthy_ = other.healthy_;
    other.pool_ = nullptr;
    other.conn_ = nullptr;
}

MysqlConnectionPool::Guard &MysqlConnectionPool::Guard::operator=(Guard &&other) noexcept {
    if (this == &other) return *this;
    release();
    pool_ = other.pool_;
    index_ = other.index_;
    conn_ = other.conn_;
    healthy_ = other.healthy_;
    other.pool_ = nullptr;
    other.conn_ = nullptr;
    return *this;
}

MysqlConnectionPool::Guard::~Guard() { release(); }

void MysqlConnectionPool::Guard::markUnhealthy() { healthy_ = false; }

void MysqlConnectionPool::Guard::release() {
    if (pool_ && conn_) pool_->release(index_, healthy_);
    pool_ = nullptr;
    conn_ = nullptr;
}

MysqlConnectionPool::MysqlConnectionPool(MysqlConfig mysqlConfig, int minSize, int maxSize,
                                         std::chrono::milliseconds connectTimeout,
                                         std::chrono::milliseconds readTimeout,
                                         std::chrono::seconds healthCheckInterval, std::chrono::seconds idleTtl,
                                         ManagerMetrics *metrics)
    : mysqlConfig_(std::move(mysqlConfig)),
      minSize_(std::max(0, minSize)),
      maxSize_(std::max(minSize_, maxSize)),
      connectTimeout_(connectTimeout),
      readTimeout_(readTimeout),
      healthCheckInterval_(healthCheckInterval),
      idleTtl_(idleTtl),
      metrics_(metrics) {}

MysqlConnectionPool::~MysqlConnectionPool() { stop(); }

bool MysqlConnectionPool::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) return true;
    running_ = true;

    // 预先创建最小连接数的连接
    for (int i = 0; i < minSize_; ++i) {
        Entry entry;
        entry.conn = connect();
        entry.healthy = entry.conn != nullptr;
        entry.lastUsed = std::chrono::steady_clock::now();
        entries_.push_back(entry);
    }

    // 启动健康检查线程
    healthThread_ = std::thread(&MysqlConnectionPool::healthLoop, this);
    return true;
}

void MysqlConnectionPool::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) return;
        running_ = false;
        cv_.notify_all();
    }

    if (healthThread_.joinable()) healthThread_.join();

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto &entry : entries_) closeEntry(entry);
    entries_.clear();
}

MysqlConnectionPool::Guard MysqlConnectionPool::acquire(std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    std::unique_lock<std::mutex> lock(mutex_);

    while (running_) {
        // 优先尝试获取空闲且健康的连接，如果连接不健康则尝试修复，如果修复失败则继续寻找其他连接
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            auto &entry = entries_[i];
            if (entry.inUse) continue;
            if (!entry.healthy && !ensureHealthy(i)) continue;
            entry.inUse = true;
            return Guard(this, i, entry.conn);
        }

        // 如果没有可用连接且当前连接数未达到最大值，则尝试创建新连接，如果创建成功则直接返回，否则继续等待
        if (static_cast<int>(entries_.size()) < maxSize_) {
            Entry entry;
            entry.conn = connect();
            entry.healthy = entry.conn != nullptr;
            entry.inUse = entry.healthy;
            entry.lastUsed = std::chrono::steady_clock::now();
            entries_.push_back(entry);
            const std::size_t index = entries_.size() - 1;
            if (entry.healthy) return Guard(this, index, entries_[index].conn);
        }

        // 等待连接被释放或健康状态改变，直到超时
        if (cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
            if (metrics_) metrics_->pool_timeouts.fetch_add(1);
            return Guard();
        }
    }

    return Guard();
}

int MysqlConnectionPool::availableCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    int count = 0;
    for (const auto &entry : entries_)
        if (!entry.inUse && entry.healthy && entry.conn) ++count;
    return count;
}

MYSQL *MysqlConnectionPool::connect() {
    MYSQL *conn = mysql_init(nullptr);
    if (!conn) return nullptr;

    unsigned int connectTimeout = toSeconds(connectTimeout_);
    unsigned int readTimeout = toSeconds(readTimeout_);
    mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &connectTimeout); // 连接超时设置
    mysql_options(
        conn, MYSQL_OPT_READ_TIMEOUT,
        &readTimeout); // 读超时设置，写超时设置在某些MySQL版本中可能不支持，因此这里统一使用读超时设置来控制查询的超时行为
    mysql_options(conn, MYSQL_OPT_WRITE_TIMEOUT, &readTimeout); // 写超时设置

    // 连接时使用TCP协议，避免域套接字可能带来的权限问题和路径限制
    if (!mysql_real_connect(conn, mysqlConfig_.host.c_str(), mysqlConfig_.user.c_str(), mysqlConfig_.password.c_str(),
                            mysqlConfig_.database.c_str(), mysqlConfig_.port, nullptr, 0)) {
        std::cerr << "MySQL pool connect failed: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        if (metrics_) metrics_->mysql_errors.fetch_add(1);
        return nullptr;
    }

    mysql_set_character_set(conn, "utf8mb4");
    if (metrics_) metrics_->pool_reconnects.fetch_add(1);
    return conn;
}

bool MysqlConnectionPool::ensureHealthy(std::size_t index) {
    auto &entry = entries_[index];
    // 如果连接不存在，直接尝试连接并返回结果
    if (!entry.conn) {
        entry.conn = connect();
        entry.healthy = entry.conn != nullptr;
        return entry.healthy;
    }

    // 如果连接存在但不健康，尝试ping检查，如果失败则关闭连接并重试连接
    if (mysql_ping(entry.conn) == 0) {
        entry.healthy = true;
        entry.failures = 0;
        return true;
    }

    // 连接存在但ping失败，可能是网络问题或服务器重启，先关闭连接再尝试重连
    if (metrics_) metrics_->mysql_errors.fetch_add(1);
    closeEntry(entry);
    entry.conn = connect();
    entry.healthy = entry.conn != nullptr;
    return entry.healthy;
}

void MysqlConnectionPool::release(std::size_t index, bool healthy) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= entries_.size()) return;

    auto &entry = entries_[index];
    entry.inUse = false;
    entry.lastUsed = std::chrono::steady_clock::now();
    // 如果调用者标记连接不健康，或者连接本身有错误，则将其标记为不健康并增加失败计数
    // 由于调用者可能在使用过程中遇到错误，因此这里不直接关闭连接，而是标记为不健康，等待健康检查线程处理
    if (!healthy || mysql_errno(entry.conn) != 0) {
        entry.healthy = false;
        ++entry.failures;
        if (metrics_) metrics_->mysql_errors.fetch_add(1);
    }
    cv_.notify_one();
}

void MysqlConnectionPool::closeEntry(Entry &entry) {
    if (entry.conn) {
        mysql_close(entry.conn);
        entry.conn = nullptr;
    }
    entry.healthy = false;
    entry.inUse = false;
}

void MysqlConnectionPool::healthLoop() {
    while (true) {
        std::this_thread::sleep_for(healthCheckInterval_);
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) return;

        const auto now = std::chrono::steady_clock::now();
        for (auto &entry : entries_) {
            // 只检查空闲连接，正在使用的连接由调用者负责检测和标记健康状态
            if (entry.inUse) continue;
            // 如果连接不健康或不存在，尝试重连
            if (!entry.healthy || !entry.conn) {
                entry.conn = connect();
                entry.healthy = entry.conn != nullptr;
                entry.lastUsed = now;
                continue;
            }
            // 对于空闲但健康的连接，执行ping检查，如果失败则关闭连接
            if (mysql_ping(entry.conn) != 0) {
                closeEntry(entry);
                if (metrics_) metrics_->mysql_errors.fetch_add(1);
            }
        }

        // 清理空闲时间过长的连接，保持至少minSize_个连接
        int openConnections = 0;
        // 先统计当前打开的连接数，避免在遍历过程中修改状态导致计数不准确
        for (const auto &entry : entries_)
            if (entry.conn) ++openConnections;
        // 然后再遍历一次，关闭空闲时间过长的连接，直到只剩下minSize_个连接
        for (auto &entry : entries_) {
            if (openConnections <= minSize_) break;
            if (!entry.inUse && entry.conn && now - entry.lastUsed > idleTtl_) {
                closeEntry(entry);
                --openConnections;
            }
        }
        // 触发等待线程重新检查连接状态
        cv_.notify_all();
    }
}

} // namespace monitor
