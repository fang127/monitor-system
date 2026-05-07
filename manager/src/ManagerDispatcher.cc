#include "ManagerDispatcher.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>

namespace monitor {

ManagerDispatcher::ManagerDispatcher(const ManagerConfig &config,
                                     ManagerMetrics *metrics)
    : config_(config),
      metrics_(metrics),
      queryQueue_(std::max<std::size_t>(1, config.query_queue_capacity)) {
    const std::size_t shardCount =
        std::max<std::size_t>(1, config_.ingest_shard_count);
    ingestShards_.reserve(shardCount);
    for (std::size_t i = 0; i < shardCount; ++i) {
        ingestShards_.push_back(std::make_unique<IngestShard>(
            std::max<std::size_t>(1,
                                  config_.ingest_queue_capacity_per_shard)));
    }
}

ManagerDispatcher::~ManagerDispatcher() { stop(); }

void ManagerDispatcher::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) return;

    for (std::size_t i = 0; i < ingestShards_.size(); ++i) {
        ingestShards_[i]->worker =
            std::thread(&ManagerDispatcher::ingestWorkerLoop, this, i);
    }

    const int initialQueryThreads =
        std::max(1, std::min(config_.query_threads_min,
                             config_.query_threads_max));
    for (int i = 0; i < initialQueryThreads; ++i) spawnQueryWorker();
}

void ManagerDispatcher::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) return;

    for (auto &shard : ingestShards_) {
        if (shard->worker.joinable()) shard->worker.join();
    }

    std::vector<std::thread> queryWorkers;
    {
        std::lock_guard<std::mutex> lock(queryWorkersMutex_);
        queryWorkers.swap(queryWorkers_);
    }
    for (auto &worker : queryWorkers) {
        if (worker.joinable()) worker.join();
    }
}

bool ManagerDispatcher::submitMonitorTask(const std::string &partitionKey,
                                          std::function<void()> task) {
    ManagerTask managerTask;
    managerTask.kind = ManagerTaskKind::MonitorPush;
    managerTask.partition_key = partitionKey;
    managerTask.run = std::move(task);

    if (!enqueueIngest(partitionKey, std::move(managerTask))) {
        if (metrics_) {
            metrics_->queue_rejected.fetch_add(1);
            metrics_->dropped_monitor_samples.fetch_add(1);
        }
        return false;
    }

    if (metrics_) metrics_->worker_requests.fetch_add(1);
    return true;
}

bool ManagerDispatcher::enqueueIngest(const std::string &partitionKey,
                                      ManagerTask task) {
    if (!running_.load() || ingestShards_.empty()) return false;
    return ingestShards_[shardIndexFor(partitionKey)]->queue.write(
        std::move(task));
}

bool ManagerDispatcher::enqueueQuery(ManagerTask task) {
    if (!running_.load()) return false;
    return queryQueue_.write(std::move(task));
}

std::size_t ManagerDispatcher::shardIndexFor(
    const std::string &partitionKey) const {
    if (ingestShards_.empty()) return 0;
    return std::hash<std::string>{}(partitionKey) % ingestShards_.size();
}

void ManagerDispatcher::ingestWorkerLoop(std::size_t shardIndex) {
    auto &queue = ingestShards_[shardIndex]->queue;
    while (running_.load() || queue.size() > 0) {
        ManagerTask task;
        if (!queue.read(task)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        try {
            if (task.run) task.run();
        } catch (const std::exception &ex) {
            if (metrics_) metrics_->task_errors.fetch_add(1);
            std::cerr << "manager ingest task failed: " << ex.what()
                      << std::endl;
        } catch (...) {
            if (metrics_) metrics_->task_errors.fetch_add(1);
            std::cerr << "manager ingest task failed with unknown error"
                      << std::endl;
        }
    }
}

void ManagerDispatcher::queryWorkerLoop(int workerId) {
    (void)workerId;
    auto idleSince = std::chrono::steady_clock::now();

    while (running_.load() || queryQueueSize() > 0) {
        ManagerTask task;
        if (!queryQueue_.read(task)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            const auto now = std::chrono::steady_clock::now();
            if (queryWorkerCount_.load() > config_.query_threads_min &&
                now - idleSince >= config_.query_idle_shrink) {
                queryWorkerCount_.fetch_sub(1);
                return;
            }
            continue;
        }

        idleSince = std::chrono::steady_clock::now();
        try {
            if (task.run) task.run();
        } catch (const std::exception &ex) {
            if (metrics_) metrics_->task_errors.fetch_add(1);
            std::cerr << "manager query task failed: " << ex.what()
                      << std::endl;
        } catch (...) {
            if (metrics_) metrics_->task_errors.fetch_add(1);
            std::cerr << "manager query task failed with unknown error"
                      << std::endl;
        }
    }

    queryWorkerCount_.fetch_sub(1);
}

void ManagerDispatcher::maybeScaleUpQuery() {
    if (queryQueueSize() < config_.query_queue_high_watermark) return;
    if (queryWorkerCount_.load() >= config_.query_threads_max) return;
    spawnQueryWorker();
}

void ManagerDispatcher::spawnQueryWorker() {
    const int current = queryWorkerCount_.load();
    if (current >= config_.query_threads_max) return;

    queryWorkerCount_.fetch_add(1);
    std::lock_guard<std::mutex> lock(queryWorkersMutex_);
    queryWorkers_.emplace_back(&ManagerDispatcher::queryWorkerLoop, this,
                               current + 1);
}

std::size_t ManagerDispatcher::ingestQueueSize() const {
    std::size_t total = 0;
    for (const auto &shard : ingestShards_) total += shard->queue.size();
    return total;
}

std::size_t ManagerDispatcher::queryQueueSize() const {
    return queryQueue_.size();
}

std::size_t ManagerDispatcher::queueSize() const {
    return ingestQueueSize() + queryQueueSize();
}

int ManagerDispatcher::workerCount() const {
    return static_cast<int>(ingestShards_.size()) +
           queryWorkerCount_.load();
}

} // namespace monitor
