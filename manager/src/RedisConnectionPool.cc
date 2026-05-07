#include "RedisConnectionPool.h"

#include <algorithm>
#include <iostream>

namespace monitor {

RedisConnectionPool::Guard::Guard(RedisConnectionPool *pool, std::size_t index, sw::redis::Redis *conn)
    : pool_(pool), index_(index), conn_(conn) {}

RedisConnectionPool::Guard::Guard(Guard &&other) noexcept {
    pool_ = other.pool_;
    index_ = other.index_;
    conn_ = other.conn_;
    healthy_ = other.healthy_;
    other.pool_ = nullptr;
    other.conn_ = nullptr;
}

RedisConnectionPool::Guard &RedisConnectionPool::Guard::operator=(Guard &&other) noexcept {
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

RedisConnectionPool::Guard::~Guard() { release(); }

void RedisConnectionPool::Guard::markUnhealthy() { healthy_ = false; }

void RedisConnectionPool::Guard::release() {
    if (pool_ && conn_) pool_->release(index_, healthy_);
    pool_ = nullptr;
    conn_ = nullptr;
}

RedisConnectionPool::RedisConnectionPool(std::string uri, int minSize, int maxSize,
                                         std::chrono::milliseconds connectTimeout,
                                         std::chrono::milliseconds readTimeout,
                                         std::chrono::seconds healthCheckInterval, std::chrono::seconds idleTtl,
                                         ManagerMetrics *metrics)
    : uri_(std::move(uri)),
      minSize_(std::max(0, minSize)),
      maxSize_(std::max(minSize_, maxSize)),
      connectTimeout_(connectTimeout),
      readTimeout_(readTimeout),
      healthCheckInterval_(healthCheckInterval),
      idleTtl_(idleTtl),
      metrics_(metrics) {}

RedisConnectionPool::~RedisConnectionPool() { stop(); }

bool RedisConnectionPool::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) return true;
    running_ = true;
    for (int i = 0; i < minSize_; ++i) {
        Entry entry;
        entry.conn = connect();
        entry.healthy = entry.conn != nullptr;
        entry.lastUsed = std::chrono::steady_clock::now();
        entries_.push_back(std::move(entry));
    }
    healthThread_ = std::thread(&RedisConnectionPool::healthLoop, this);
    return true;
}

void RedisConnectionPool::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) return;
        running_ = false;
        cv_.notify_all();
    }
    if (healthThread_.joinable()) healthThread_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
}

RedisConnectionPool::Guard RedisConnectionPool::acquire(std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    std::unique_lock<std::mutex> lock(mutex_);
    while (running_) {
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            auto &entry = entries_[i];
            if (entry.inUse) continue;
            if (!entry.healthy && !ensureHealthy(i)) continue;
            entry.inUse = true;
            return Guard(this, i, entry.conn.get());
        }
        // 如果没有可用连接且当前连接数未达上限，尝试创建新连接
        if (static_cast<int>(entries_.size()) < maxSize_) {
            Entry entry;
            entry.conn = connect();
            entry.healthy = entry.conn != nullptr;
            entry.inUse = entry.healthy;
            entry.lastUsed = std::chrono::steady_clock::now();
            entries_.push_back(std::move(entry));
            const std::size_t index = entries_.size() - 1;
            if (entries_[index].healthy) return Guard(this, index, entries_[index].conn.get());
        }

        if (cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
            if (metrics_) metrics_->pool_timeouts.fetch_add(1);
            return Guard();
        }
    }
    return Guard();
}

int RedisConnectionPool::availableCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    int count = 0;
    for (const auto &entry : entries_)
        if (!entry.inUse && entry.healthy && entry.conn) ++count;
    return count;
}

std::unique_ptr<sw::redis::Redis> RedisConnectionPool::connect() {
    try {
        // uri为redis://host:port?param=value格式，添加连接和读写超时参数
        std::string uri = uri_;
        uri += (uri.find('?') == std::string::npos) ? "?" : "&";
        uri += "connect_timeout=" + std::to_string(connectTimeout_.count()) +
               "ms&socket_timeout=" + std::to_string(readTimeout_.count()) + "ms";

        // 使用sw::redis库创建连接，并执行ping命令验证连接是否成功
        auto conn = std::make_unique<sw::redis::Redis>(uri);
        conn->ping();
        if (metrics_) metrics_->pool_reconnects.fetch_add(1);
        return conn;
    } catch (const std::exception &ex) {
        if (metrics_) metrics_->redis_errors.fetch_add(1);
        std::cerr << "Redis pool connect failed: " << ex.what() << std::endl;
        return nullptr;
    }
}

bool RedisConnectionPool::ensureHealthy(std::size_t index) {
    auto &entry = entries_[index];
    if (!entry.conn) {
        entry.conn = connect();
        entry.healthy = entry.conn != nullptr;
        return entry.healthy;
    }

    try {
        entry.conn->ping();
        entry.healthy = true;
        entry.failures = 0;
        return true;
    } catch (const std::exception &) {
        if (metrics_) metrics_->redis_errors.fetch_add(1);
        closeEntry(entry);
        entry.conn = connect();
        entry.healthy = entry.conn != nullptr;
        return entry.healthy;
    }
}

void RedisConnectionPool::release(std::size_t index, bool healthy) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= entries_.size()) return;
    auto &entry = entries_[index];
    entry.inUse = false;
    entry.lastUsed = std::chrono::steady_clock::now();
    if (!healthy) {
        entry.healthy = false; // 标记连接不健康，等待healthLoop修复
        ++entry.failures;
        if (metrics_) metrics_->redis_errors.fetch_add(1);
    }
    cv_.notify_one();
}

void RedisConnectionPool::closeEntry(Entry &entry) {
    entry.conn.reset();
    entry.healthy = false;
    entry.inUse = false;
}

void RedisConnectionPool::healthLoop() {
    while (true) {
        std::this_thread::sleep_for(healthCheckInterval_);
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) return;

        const auto now = std::chrono::steady_clock::now();
        for (auto &entry : entries_) {
            if (entry.inUse) continue;
            if (!entry.conn) {
                entry.conn = connect();
                entry.healthy = entry.conn != nullptr;
                entry.lastUsed = now;
                continue;
            }
            // 对于release时标记为不健康的连接，healthLoop会尝试ping修复，如果失败则关闭连接等待下一次重试
            try {
                entry.conn->ping();
                entry.healthy = true;
            } catch (const std::exception &) {
                closeEntry(entry);
                if (metrics_) metrics_->redis_errors.fetch_add(1);
            }
        }
        // 关闭空闲时间超过idleTtl_且当前连接数大于minSize_的连接
        int openConnections = 0;
        for (const auto &entry : entries_)
            if (entry.conn) ++openConnections;
        for (auto &entry : entries_) {
            if (openConnections <= minSize_) break;
            if (!entry.inUse && entry.conn && now - entry.lastUsed > idleTtl_) {
                closeEntry(entry);
                --openConnections;
            }
        }
        cv_.notify_all();
    }
}

RedisCache::RedisCache(RedisConnectionPool *pool, std::chrono::seconds ttl, ManagerMetrics *metrics)
    : pool_(pool), ttl_(ttl), metrics_(metrics) {}

bool RedisCache::set(const std::string &key, const std::string &value) {
    if (!pool_) return false;
    auto guard = pool_->acquire(std::chrono::milliseconds(50));
    if (!guard) return false;
    try {
        guard.get()->setex(key, ttl_, value);
        return true;
    } catch (const std::exception &) {
        guard.markUnhealthy();
        if (metrics_) metrics_->redis_errors.fetch_add(1);
        return false;
    }
}

std::optional<std::string> RedisCache::get(const std::string &key) {
    if (!pool_) return std::nullopt;
    auto guard = pool_->acquire(std::chrono::milliseconds(50));
    if (!guard) return std::nullopt;
    try {
        return guard.get()->get(key);
    } catch (const std::exception &) {
        guard.markUnhealthy();
        if (metrics_) metrics_->redis_errors.fetch_add(1);
        return std::nullopt;
    }
}

int RedisCache::availableCount() const { return pool_ ? pool_->availableCount() : 0; }

} // namespace monitor
