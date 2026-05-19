#pragma once

#include "ManagerConfig.h"
#include "ManagerMetrics.h"
#include "folly/MPMCQueue.h"
#include <grpcpp/support/status.h>

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace monitor {

/**
 * @brief         任务类型枚举，区分监控推送任务和查询任务
 *
 */
enum class ManagerTaskKind {
    MonitorPush,
    Query,
    Shutdown,
};

/**
 * @brief         管理器任务结构体，包含任务类型、分区键和执行函数
 *
 */
struct ManagerTask {
    ManagerTaskKind kind = ManagerTaskKind::Query; // 默认值为查询任务
    std::string partition_key;                     // 分区键，仅对监控推送任务有效
    std::function<void()> run;                     // 任务执行函数，接受一个无参数的函数对象，返回void
};

class ManagerDispatcher {
public:
    ManagerDispatcher(const ManagerConfig &config, ManagerMetrics *metrics);
    ~ManagerDispatcher();

    /**
     * @brief         启动调度器，创建工作线程并开始处理任务
     *
     */
    void start();

    /**
     * @brief         停止调度器，发送关闭任务并等待工作线程退出
     *
     */
    void stop();

    /**
     * @brief 提交监控推送任务，根据分区键将任务分发到对应的分片队列中，保证同一分区的任务由同一线程处理，避免锁竞争
     *
     * @param         partitionKey 分区键
     * @param         task 待执行任务
     * @return        入队成功返回 true，否则返回 false
     */
    bool submitMonitorTask(const std::string &partitionKey, std::function<void()> task);

    /**
     * @brief
     * 提交监控推送任务的重载版本，不指定分区键，默认使用空字符串作为分区键，所有不指定分区键的任务将被分发到同一分片队列中处理
     *
     * @param         task 待执行任务
     * @return        入队成功返回 true，否则返回 false
     */
    bool submitMonitorTask(std::function<void()> task) { return submitMonitorTask(std::string(), std::move(task)); }

    /**
     * @brief         提交查询任务，将任务放入查询队列中，由查询工作线程处理，支持任务超时和结果返回
     *
     * @tparam        Response 查询响应类型
     * @param         task 查询任务
     * @param         response 查询响应输出对象
     * @return        gRPC 调用状态
     */
    template <typename Response>
    grpc::Status submitQueryTask(std::function<grpc::Status(Response *)> task, Response *response) {
        if (!response) return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "null response");

        if (queryQueueSize() >= config_.query_queue_high_watermark) {
            maybeScaleUpQuery();
            if (queryQueueSize() >= config_.query_queue_capacity) {
                if (metrics_) metrics_->queue_rejected.fetch_add(1);
                return grpc::Status(grpc::StatusCode::RESOURCE_EXHAUSTED, "manager query queue is full");
            }
        }

        auto result = std::make_shared<Response>();
        auto promise = std::make_shared<std::promise<grpc::Status>>();
        auto future = promise->get_future();

        ManagerTask managerTask;
        managerTask.kind = ManagerTaskKind::Query;
        managerTask.run = [result, promise, task = std::move(task)]() mutable {
            try {
                promise->set_value(task(result.get()));
            } catch (const std::exception &ex) {
                promise->set_value(
                    grpc::Status(grpc::StatusCode::INTERNAL, std::string("manager query task failed: ") + ex.what()));
            } catch (...) {
                promise->set_value(
                    grpc::Status(grpc::StatusCode::INTERNAL, "manager query task failed with unknown error"));
            }
        };

        if (!enqueueQuery(std::move(managerTask))) {
            if (metrics_) metrics_->queue_rejected.fetch_add(1);
            return grpc::Status(grpc::StatusCode::RESOURCE_EXHAUSTED, "manager query queue is full");
        }

        if (metrics_) metrics_->query_requests.fetch_add(1);
        if (future.wait_for(config_.task_timeout) != std::future_status::ready) {
            if (metrics_) metrics_->task_timeouts.fetch_add(1);
            return grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED, "manager query task timed out");
        }

        grpc::Status status = future.get();
        if (status.ok()) *response = std::move(*result);
        return status;
    }

    /**
     * @brief         获取当前队列长度，包括监控推送任务队列和查询任务队列，用于监控和调度决策
     *
     * @return        总队列长度
     */
    std::size_t queueSize() const;

    /**
     * @brief         获取监控推送任务队列长度，返回所有分片队列的总长度，用于监控和调度决策
     *
     * @return        监控推送任务队列总长度
     */
    std::size_t ingestQueueSize() const;

    /**
     * @brief         获取查询任务队列长度，返回查询队列的长度，用于监控和调度决策
     *
     * @return        查询队列长度
     */
    std::size_t queryQueueSize() const;

    /**
     * @brief         获取当前查询工作线程数量，用于监控和调度决策
     *
     * @return        查询工作线程数量
     */
    int workerCount() const;

private:
    /**
     * @brief         监控推送任务分片结构体，每个分片包含一个MPMC队列和一个工作线程
     *
     */
    struct IngestShard {
        explicit IngestShard(std::size_t capacity) : queue(capacity) {}
        folly::MPMCQueue<ManagerTask> queue; // 监控推送任务队列，使用folly的MPMC队列实现高性能的多生产者多消费者队列
        std::thread worker;                  // 工作线程，负责从队列中取出任务并执行
    };

    /**
     * @brief 将监控推送任务放入对应分片的队列中，根据分区键计算分片索引，保证同一分区的任务由同一线程处理，避免锁竞争
     *
     * @param         partitionKey 分区键
     * @param         task 待入队任务
     * @return        入队成功返回 true，否则返回 false
     */
    bool enqueueIngest(const std::string &partitionKey, ManagerTask task);

    /**
     * @brief         将查询任务放入查询队列中，由查询工作线程处理，支持任务超时和结果返回
     *
     * @param         task 待入队任务
     * @return        入队成功返回 true，否则返回 false
     */
    bool enqueueQuery(ManagerTask task);

    /**
     * @brief
     * 根据分区键计算分片索引，使用std::hash对分区键进行哈希计算，然后取模分片数量，保证同一分区的任务由同一线程处理，避免锁竞争
     *
     * @param         partitionKey 分区键
     * @return        分片索引
     */
    std::size_t shardIndexFor(const std::string &partitionKey) const;

    /**
     * @brief 监控推送任务工作循环，每个分片一个线程，负责从对应分片的队列中取出任务并执行，处理任务异常和线程退出逻辑
     *
     * @param         shardIndex 分片索引
     */
    void ingestWorkerLoop(std::size_t shardIndex);

    /**
     * @brief         查询任务工作循环，每个查询工作线程一个，负责从查询队列中取出任务并执行，处理任务异常和线程退出逻辑
     *
     * @param         workerId 查询工作线程 ID
     */
    void queryWorkerLoop(int workerId);

    /**
     * @brief
     * 根据当前查询队列长度和工作线程数量，判断是否需要增加查询工作线程，避免查询任务积压过多导致响应变慢，同时控制最大线程数量避免资源过度消耗
     *
     */
    void maybeScaleUpQuery();

    /**
     * @brief
     * 启动一个新的查询工作线程，增加查询处理能力，线程会执行queryWorkerLoop函数，负责从查询队列中取出任务并执行，处理任务异常和线程退出逻辑
     *
     */
    void spawnQueryWorker();

    ManagerConfig config_;              // 管理器配置，包含队列容量、分区数量、任务超时时间等参数
    ManagerMetrics *metrics_ = nullptr; // 管理器指标指针，用于记录队列长度、请求数量、超时数量等指标
    std::vector<std::unique_ptr<IngestShard>> ingestShards_; // 监控推送任务分片，每个分片包含一个MPMC队列和一个工作线程
    folly::MPMCQueue<ManagerTask> queryQueue_; // 查询任务队列，使用folly的MPMC队列实现高性能的多生产者多消费者队列
    std::atomic<bool> running_{false};         // 运行状态标志，表示调度器是否正在运行
    std::atomic<int> queryWorkerCount_{0};     // 查询工作线程数量，使用原子变量保证线程安全
    std::mutex queryWorkersMutex_;             // 查询工作线程列表的互斥锁，保护queryWorkers_的访问
    std::vector<std::thread> queryWorkers_;    // 查询工作线程列表，负责从查询任务队列中取出任务并执行
};

} // namespace monitor
