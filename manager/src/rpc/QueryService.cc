#include "QueryService.h"

#include <iostream>
#include <utility>

namespace monitor {

namespace {

// 线程局部变量，标记当前是否在调度器线程中，避免重复调度
thread_local bool g_inside_dispatcher = false;

/**
 * @brief         调度器作用域，在构造时设置标记，析构时清除标记，确保在调度器线程中执行查询逻辑
 *
 */
class DispatcherScope {
public:
    DispatcherScope() { g_inside_dispatcher = true; }
    ~DispatcherScope() { g_inside_dispatcher = false; }
};

/**
 * @brief         通用的查询调度函数，接受请求和响应对象，以及实际处理查询的成员函数指针，在调度器线程中执行查询逻辑
 *
 * @tparam        Request 请求类型
 * @tparam        Response 响应类型
 * @tparam        Method 查询处理函数类型
 * @param         dispatcher 管理器调度器
 * @param         request 查询请求
 * @param         response 查询响应
 * @param         method 实际查询处理函数
 * @return        gRPC 调用状态
 */
template <typename Request, typename Response, typename Method>
grpc::Status dispatchQuery(ManagerDispatcher *dispatcher, const Request *request, Response *response, Method method) {
    // 如果没有调度器或者已经在调度器线程中，直接执行查询逻辑
    if (!dispatcher || g_inside_dispatcher) return grpc::Status::OK;
    // 复制请求对象，确保在调度器线程中使用有效的请求数据
    Request requestCopy = request ? *request : Request();
    // 提交查询任务到调度器线程，使用DispatcherScope确保在调度器线程中执行，并调用实际的查询处理函数
    return dispatcher->submitQueryTask<Response>(
        [requestCopy = std::move(requestCopy), method](Response *out) mutable {
            DispatcherScope scope;
            // 调用实际的查询处理函数，传入请求副本和响应对象
            return method(&requestCopy, out);
        },
        response);
}

/**
 * @brief         将查询错误转换为 gRPC 状态码，区分未初始化和其他内部错误
 *
 * @param         error 错误信息
 * @return        gRPC 错误状态
 */
grpc::Status queryErrorStatus(const std::string &error) {
    if (error.find("not initialized") != std::string::npos ||
        error.find("MySQL support is not enabled") != std::string::npos) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, error);
    }
    return grpc::Status(grpc::StatusCode::INTERNAL, error);
}

} // namespace

QueryServiceImpl::QueryServiceImpl(QueryManager *queryManager, ManagerDispatcher *dispatcher, RedisCache *redisCache)
    : queryManager_(queryManager), dispatcher_(dispatcher), redisCache_(redisCache) {}

/**
 * @brief         将 Protobuf 定义的时间范围转换为内部使用的 TimeRange 结构，方便后续查询逻辑处理
 *
 * @param         range protobuf 时间范围
 * @return        内部时间范围
 */
TimeRange QueryServiceImpl::convertTimeRange(const ::monitor::proto::TimeRange &range) {
    TimeRange timeRange;
    timeRange.start_time = std::chrono::system_clock::from_time_t(range.start_time().seconds());
    timeRange.end_time = std::chrono::system_clock::from_time_t(range.end_time().seconds());
    return timeRange;
}

/**
 * @brief         设置 protobuf 时间戳
 *
 * @param         ts 待写入的 protobuf 时间戳
 * @param         tp 系统时间点
 */
void QueryServiceImpl::setTimestamp(::google::protobuf::Timestamp *ts,
                                    const std::chrono::system_clock::time_point &tp) {
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
    ts->set_seconds(seconds);
    ts->set_nanos(0);
}

::grpc::Status QueryServiceImpl::QueryPerformance(::grpc::ServerContext *context,
                                                  const ::monitor::proto::QueryPerformanceRequest *request,
                                                  ::monitor::proto::QueryPerformanceResponse *response) {
    (void)context;

    // 如果存在调度器且当前不在调度器线程中，使用 dispatchQuery
    // 将查询任务提交到调度器线程执行，确保查询逻辑在正确的线程环境中运行
    if (dispatcher_ && !g_inside_dispatcher) {
        return dispatchQuery(dispatcher_, request, response,
                             [this](const auto *req, auto *resp) { return QueryPerformance(nullptr, req, resp); });
    }

    if (!queryManager_ || !queryManager_->isInitialized()) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Query manager not initialized");
    }

    // 验证时间范围
    TimeRange range = convertTimeRange(request->time_range());
    if (!queryManager_->validateTimeRange(range)) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid time range: start_time > end_time");
    }

    int page = request->pagination().page();
    int pageSize = request->pagination().page_size();
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    int totalCount = 0;
    std::string error;
    auto records =
        queryManager_->queryPerformanceRecords(request->server_name(), range, page, pageSize, &totalCount, &error);
    if (!error.empty()) return queryErrorStatus(error);

    for (const auto &rec : records) {
        auto *protoRec = response->add_records();
        protoRec->set_server_name(rec.server_name);
        setTimestamp(protoRec->mutable_timestamp(), rec.timestamp);
        protoRec->set_cpu_percent(rec.cpu_percent);
        protoRec->set_usr_percent(rec.usr_percent);
        protoRec->set_system_percent(rec.sys_percent);
        protoRec->set_nice_percent(rec.nice_percent);
        protoRec->set_idle_percent(rec.idle_percent);
        protoRec->set_io_wait_percent(rec.io_wait_percent);
        protoRec->set_irq_percent(rec.irq_percent);
        protoRec->set_soft_irq_percent(rec.soft_irq_percent);
        protoRec->set_load_avg_1(rec.load_avg_1);
        protoRec->set_load_avg_3(rec.load_avg_3);
        protoRec->set_load_avg_15(rec.load_avg_15);
        protoRec->set_mem_used_percent(rec.mem_used_percent);
        protoRec->set_mem_total(rec.mem_total);
        protoRec->set_mem_free(rec.mem_free);
        protoRec->set_mem_avail(rec.mem_avail);
        protoRec->set_disk_util_percent(rec.disk_util_percent);
        protoRec->set_send_rate(rec.net_sent_bytes);
        protoRec->set_rcv_rate(rec.net_recv_bytes);
        protoRec->set_score(rec.score);
        protoRec->set_cpu_percent_rate(rec.cpu_percent_rate);
        protoRec->set_mem_used_percent_rate(rec.mem_used_percent_rate);
        protoRec->set_disk_util_percent_rate(rec.disk_util_percent_rate);
        protoRec->set_load_avg_1_rate(rec.load_avg_1_rate);
    }

    response->set_total_count(totalCount);
    response->set_page(page);
    response->set_page_size(pageSize);

    return grpc::Status::OK;
}

::grpc::Status QueryServiceImpl::QueryTrend(::grpc::ServerContext *context,
                                            const ::monitor::proto::QueryTrendRequest *request,
                                            ::monitor::proto::QueryTrendResponse *response) {
    (void)context;

    if (dispatcher_ && !g_inside_dispatcher) {
        return dispatchQuery(dispatcher_, request, response,
                             [this](const auto *req, auto *resp) { return QueryTrend(nullptr, req, resp); });
    }

    if (!queryManager_ || !queryManager_->isInitialized()) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Query manager not initialized");
    }

    TimeRange range = convertTimeRange(request->time_range());
    if (!queryManager_->validateTimeRange(range)) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid time range: start_time > end_time");
    }

    std::string error;
    auto records = queryManager_->queryTrend(request->server_name(), range, request->interval_seconds(), &error);
    if (!error.empty()) return queryErrorStatus(error);

    for (const auto &rec : records) {
        auto *protoRec = response->add_records();
        protoRec->set_server_name(rec.server_name);
        setTimestamp(protoRec->mutable_timestamp(), rec.timestamp);
        protoRec->set_cpu_percent(rec.cpu_percent);
        protoRec->set_usr_percent(rec.usr_percent);
        protoRec->set_system_percent(rec.sys_percent);
        protoRec->set_io_wait_percent(rec.io_wait_percent);
        protoRec->set_load_avg_1(rec.load_avg_1);
        protoRec->set_load_avg_3(rec.load_avg_3);
        protoRec->set_load_avg_15(rec.load_avg_15);
        protoRec->set_mem_used_percent(rec.mem_used_percent);
        protoRec->set_disk_util_percent(rec.disk_util_percent);
        protoRec->set_send_rate(rec.net_sent_bytes);
        protoRec->set_rcv_rate(rec.net_recv_bytes);
        protoRec->set_score(rec.score);
        protoRec->set_cpu_percent_rate(rec.cpu_percent_rate);
        protoRec->set_mem_used_percent_rate(rec.mem_used_percent_rate);
        protoRec->set_disk_util_percent_rate(rec.disk_util_percent_rate);
        protoRec->set_load_avg_1_rate(rec.load_avg_1_rate);
    }

    response->set_interval_seconds(request->interval_seconds());

    return grpc::Status::OK;
}

::grpc::Status QueryServiceImpl::QueryAnomaly(::grpc::ServerContext *context,
                                              const ::monitor::proto::QueryAnomalyRequest *request,
                                              ::monitor::proto::QueryAnomalyResponse *response) {
    (void)context;

    if (dispatcher_ && !g_inside_dispatcher) {
        return dispatchQuery(dispatcher_, request, response,
                             [this](const auto *req, auto *resp) { return QueryAnomaly(nullptr, req, resp); });
    }

    if (!queryManager_ || !queryManager_->isInitialized()) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Query manager not initialized");
    }

    TimeRange range = convertTimeRange(request->time_range());
    if (!queryManager_->validateTimeRange(range)) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid time range: start_time > end_time");
    }

    AnomalyThreshold thresholds;
    thresholds.cpu_threshold = request->cpu_threshold() > 0 ? request->cpu_threshold() : 80.0f;
    thresholds.memory_threshold = request->mem_threshold() > 0 ? request->mem_threshold() : 90.0f;
    thresholds.disk_threshold = request->disk_threshold() > 0 ? request->disk_threshold() : 85.0f;
    thresholds.change_rate_threshold = request->change_rate_threshold() > 0 ? request->change_rate_threshold() : 0.5f;
    thresholds.mysql_connection_threshold =
        request->mysql_connection_threshold() > 0 ? request->mysql_connection_threshold() : 90.0f;
    thresholds.mysql_replication_lag_threshold =
        request->mysql_replication_lag_threshold() > 0 ? request->mysql_replication_lag_threshold() : 30.0f;
    thresholds.mysql_slow_query_rate_threshold =
        request->mysql_slow_query_rate_threshold() > 0 ? request->mysql_slow_query_rate_threshold() : 1.0f;
    thresholds.mysql_lock_wait_rate_threshold =
        request->mysql_lock_wait_rate_threshold() > 0 ? request->mysql_lock_wait_rate_threshold() : 1.0f;
    thresholds.mysql_buffer_pool_hit_threshold =
        request->mysql_buffer_pool_hit_threshold() > 0 ? request->mysql_buffer_pool_hit_threshold() : 95.0f;

    int page = request->pagination().page();
    int pageSize = request->pagination().page_size();
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    int totalCount = 0;
    std::string error;
    auto records = queryManager_->queryAnomalyRecords(request->server_name(), range, thresholds, page, pageSize,
                                                      &totalCount, &error);
    if (!error.empty()) return queryErrorStatus(error);

    for (const auto &rec : records) {
        auto *protoRec = response->add_anomalies();
        protoRec->set_server_name(rec.server_name);
        setTimestamp(protoRec->mutable_timestamp(), rec.timestamp);
        protoRec->set_anomaly_type(rec.anomaly_type);
        protoRec->set_severity(rec.severity);
        protoRec->set_value(rec.value);
        protoRec->set_threshold(rec.threshold);
        protoRec->set_metric_name(rec.metric_name);
    }

    response->set_total_count(totalCount);
    response->set_page(page);
    response->set_page_size(pageSize);

    return grpc::Status::OK;
}

::grpc::Status QueryServiceImpl::QueryScoreRank(::grpc::ServerContext *context,
                                                const ::monitor::proto::QueryScoreRankRequest *request,
                                                ::monitor::proto::QueryScoreRankResponse *response) {
    (void)context;

    if (dispatcher_ && !g_inside_dispatcher) {
        return dispatchQuery(dispatcher_, request, response,
                             [this](const auto *req, auto *resp) { return QueryScoreRank(nullptr, req, resp); });
    }

    if (!queryManager_ || !queryManager_->isInitialized()) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Query manager not initialized");
    }

    SortOrder order = (request->order() == ::monitor::proto::ASC) ? SortOrder::ASC : SortOrder::DESC;

    int page = request->pagination().page();
    int pageSize = request->pagination().page_size();
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    int totalCount = 0;
    std::string error;
    auto records = queryManager_->queryServerScoreRank(order, page, pageSize, &totalCount, &error);
    if (!error.empty()) return queryErrorStatus(error);

    for (const auto &rec : records) {
        auto *protoRec = response->add_servers();
        protoRec->set_server_name(rec.server_name);
        protoRec->set_score(rec.score);
        setTimestamp(protoRec->mutable_last_update(), rec.last_updated);
        protoRec->set_status(rec.status == ServerStatus::ONLINE ? ::monitor::proto::ONLINE : ::monitor::proto::OFFLINE);
        protoRec->set_cpu_percent(rec.cpu_percent);
        protoRec->set_mem_used_percent(rec.mem_used_percent);
        protoRec->set_disk_util_percent(rec.disk_util_percent);
        protoRec->set_load_avg_1(rec.load_avg_1);
    }

    response->set_total_count(totalCount);
    response->set_page(page);
    response->set_page_size(pageSize);

    return grpc::Status::OK;
}

::grpc::Status QueryServiceImpl::QueryLatestScore(::grpc::ServerContext *context,
                                                  const ::monitor::proto::QueryLatestScoreRequest *request,
                                                  ::monitor::proto::QueryLatestScoreResponse *response) {
    (void)context;
    (void)request;

    if (dispatcher_ && !g_inside_dispatcher) {
        return dispatchQuery(dispatcher_, request, response,
                             [this](const auto *req, auto *resp) { return QueryLatestScore(nullptr, req, resp); });
    }

    if (redisCache_) {
        auto cached = redisCache_->get("manager:query:latest_score");
        if (cached && response->ParseFromString(*cached)) return grpc::Status::OK;
    }

    if (!queryManager_ || !queryManager_->isInitialized()) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Query manager not initialized");
    }

    ClusterStats stats;
    std::string error;
    auto records = queryManager_->queryLatestServerScores(&stats, &error);
    if (!error.empty()) return queryErrorStatus(error);

    for (const auto &rec : records) {
        auto *protoRec = response->add_servers();
        protoRec->set_server_name(rec.server_name);
        protoRec->set_score(rec.score);
        setTimestamp(protoRec->mutable_last_update(), rec.last_updated);
        protoRec->set_status(rec.status == ServerStatus::ONLINE ? ::monitor::proto::ONLINE : ::monitor::proto::OFFLINE);
        protoRec->set_cpu_percent(rec.cpu_percent);
        protoRec->set_mem_used_percent(rec.mem_used_percent);
        protoRec->set_disk_util_percent(rec.disk_util_percent);
        protoRec->set_load_avg_1(rec.load_avg_1);
    }

    auto *proto_stats = response->mutable_cluster_stats();
    proto_stats->set_total_servers(stats.total_servers);
    proto_stats->set_online_servers(stats.online_servers);
    proto_stats->set_offline_servers(stats.offline_servers);
    proto_stats->set_avg_score(stats.avg_score);
    proto_stats->set_max_score(stats.max_score);
    proto_stats->set_min_score(stats.min_score);
    proto_stats->set_best_server(stats.best_server);
    proto_stats->set_worst_server(stats.worst_server);

    if (redisCache_) {
        std::string serialized;
        if (response->SerializeToString(&serialized)) redisCache_->set("manager:query:latest_score", serialized);
    }

    return grpc::Status::OK;
}

::grpc::Status QueryServiceImpl::QueryNetDetail(::grpc::ServerContext *context,
                                                const ::monitor::proto::QueryDetailRequest *request,
                                                ::monitor::proto::QueryNetDetailResponse *response) {
    (void)context;

    if (dispatcher_ && !g_inside_dispatcher) {
        return dispatchQuery(dispatcher_, request, response,
                             [this](const auto *req, auto *resp) { return QueryNetDetail(nullptr, req, resp); });
    }

    if (!queryManager_ || !queryManager_->isInitialized()) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Query manager not initialized");
    }

    TimeRange time_range = convertTimeRange(request->time_range());
    if (!queryManager_->validateTimeRange(time_range)) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid time range: start_time > end_time");
    }

    int page = request->pagination().page();
    int pageSize = request->pagination().page_size();
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    int totalCount = 0;
    std::string error;
    auto records =
        queryManager_->queryNetDetailRecords(request->server_name(), time_range, page, pageSize, &totalCount, &error);
    if (!error.empty()) return queryErrorStatus(error);

    for (const auto &rec : records) {
        auto *protoRec = response->add_records();
        protoRec->set_server_name(rec.server_name);
        protoRec->set_net_name(rec.net_name);
        setTimestamp(protoRec->mutable_timestamp(), rec.timestamp);
        protoRec->set_err_in(rec.err_in);
        protoRec->set_err_out(rec.err_out);
        protoRec->set_drop_in(rec.drop_in);
        protoRec->set_drop_out(rec.drop_out);
        protoRec->set_rcv_bytes_rate(rec.recv_bytes_rate);
        protoRec->set_snd_bytes_rate(rec.sent_bytes_rate);
        protoRec->set_rcv_packets_rate(rec.recv_packets_rate);
        protoRec->set_snd_packets_rate(rec.sent_packets_rate);
    }

    response->set_total_count(totalCount);
    response->set_page(page);
    response->set_page_size(pageSize);

    return grpc::Status::OK;
}

::grpc::Status QueryServiceImpl::QueryDiskDetail(::grpc::ServerContext *context,
                                                 const ::monitor::proto::QueryDetailRequest *request,
                                                 ::monitor::proto::QueryDiskDetailResponse *response) {
    (void)context;

    if (dispatcher_ && !g_inside_dispatcher) {
        return dispatchQuery(dispatcher_, request, response,
                             [this](const auto *req, auto *resp) { return QueryDiskDetail(nullptr, req, resp); });
    }

    if (!queryManager_ || !queryManager_->isInitialized()) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Query manager not initialized");
    }

    TimeRange time_range = convertTimeRange(request->time_range());
    if (!queryManager_->validateTimeRange(time_range)) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid time range: start_time > end_time");
    }

    int page = request->pagination().page();
    int pageSize = request->pagination().page_size();
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    int totalCount = 0;
    std::string error;
    auto records =
        queryManager_->queryDiskDetailRecords(request->server_name(), time_range, page, pageSize, &totalCount, &error);
    if (!error.empty()) return queryErrorStatus(error);

    for (const auto &rec : records) {
        auto *protoRec = response->add_records();
        protoRec->set_server_name(rec.server_name);
        protoRec->set_disk_name(rec.disk_name);
        setTimestamp(protoRec->mutable_timestamp(), rec.timestamp);
        protoRec->set_read_bytes_per_sec(rec.read_bytes_per_sec);
        protoRec->set_write_bytes_per_sec(rec.write_bytes_per_sec);
        protoRec->set_read_iops(rec.read_iops);
        protoRec->set_write_iops(rec.write_iops);
        protoRec->set_avg_read_latency_ms(rec.avg_read_latency_ms);
        protoRec->set_avg_write_latency_ms(rec.avg_write_latency_ms);
        protoRec->set_util_percent(rec.util_percent);
    }

    response->set_total_count(totalCount);
    response->set_page(page);
    response->set_page_size(pageSize);

    return grpc::Status::OK;
}

::grpc::Status QueryServiceImpl::QueryMemDetail(::grpc::ServerContext *context,
                                                const ::monitor::proto::QueryDetailRequest *request,
                                                ::monitor::proto::QueryMemDetailResponse *response) {
    (void)context;

    if (dispatcher_ && !g_inside_dispatcher) {
        return dispatchQuery(dispatcher_, request, response,
                             [this](const auto *req, auto *resp) { return QueryMemDetail(nullptr, req, resp); });
    }

    if (!queryManager_ || !queryManager_->isInitialized()) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Query manager not initialized");
    }

    TimeRange time_range = convertTimeRange(request->time_range());
    if (!queryManager_->validateTimeRange(time_range)) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid time range: start_time > end_time");
    }

    int page = request->pagination().page();
    int pageSize = request->pagination().page_size();
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    int totalCount = 0;
    std::string error;
    auto records =
        queryManager_->queryMemDetailRecords(request->server_name(), time_range, page, pageSize, &totalCount, &error);
    if (!error.empty()) return queryErrorStatus(error);

    for (const auto &rec : records) {
        auto *protoRec = response->add_records();
        protoRec->set_server_name(rec.server_name);
        setTimestamp(protoRec->mutable_timestamp(), rec.timestamp);
        protoRec->set_total(rec.mem_total);
        protoRec->set_free(rec.mem_free);
        protoRec->set_avail(rec.mem_avail);
        protoRec->set_buffers(rec.buffers);
        protoRec->set_cached(rec.cached);
        protoRec->set_active(rec.active);
        protoRec->set_inactive(rec.inactive);
        protoRec->set_dirty(rec.dirty);
    }

    response->set_total_count(totalCount);
    response->set_page(page);
    response->set_page_size(pageSize);

    return grpc::Status::OK;
}

::grpc::Status QueryServiceImpl::QuerySoftIrqDetail(::grpc::ServerContext *context,
                                                    const ::monitor::proto::QueryDetailRequest *request,
                                                    ::monitor::proto::QuerySoftIrqDetailResponse *response) {
    (void)context;

    if (dispatcher_ && !g_inside_dispatcher) {
        return dispatchQuery(dispatcher_, request, response,
                             [this](const auto *req, auto *resp) { return QuerySoftIrqDetail(nullptr, req, resp); });
    }

    if (!queryManager_ || !queryManager_->isInitialized()) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Query manager not initialized");
    }

    TimeRange time_range = convertTimeRange(request->time_range());
    if (!queryManager_->validateTimeRange(time_range)) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid time range: start_time > end_time");
    }

    int page = request->pagination().page();
    int pageSize = request->pagination().page_size();
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    int totalCount = 0;
    std::string error;
    auto records = queryManager_->querySoftIrqDetailRecords(request->server_name(), time_range, page, pageSize,
                                                            &totalCount, &error);
    if (!error.empty()) return queryErrorStatus(error);

    for (const auto &rec : records) {
        auto *protoRec = response->add_records();
        protoRec->set_server_name(rec.server_name);
        protoRec->set_cpu_name(rec.cpu_name);
        setTimestamp(protoRec->mutable_timestamp(), rec.timestamp);
        protoRec->set_hi(rec.hi);
        protoRec->set_timer(rec.timer);
        protoRec->set_net_tx(rec.net_tx);
        protoRec->set_net_rx(rec.net_rx);
        protoRec->set_block(rec.block);
        protoRec->set_sched(rec.sched);
    }

    response->set_total_count(totalCount);
    response->set_page(page);
    response->set_page_size(pageSize);

    return grpc::Status::OK;
}

::grpc::Status QueryServiceImpl::QueryMysqlDetail(::grpc::ServerContext *context,
                                                  const ::monitor::proto::QueryDetailRequest *request,
                                                  ::monitor::proto::QueryMysqlDetailResponse *response) {
    (void)context;

    if (dispatcher_ && !g_inside_dispatcher) {
        return dispatchQuery(dispatcher_, request, response,
                             [this](const auto *req, auto *resp) { return QueryMysqlDetail(nullptr, req, resp); });
    }

    if (!queryManager_ || !queryManager_->isInitialized()) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Query manager not initialized");
    }

    TimeRange time_range = convertTimeRange(request->time_range());
    if (!queryManager_->validateTimeRange(time_range)) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid time range: start_time > end_time");
    }

    int page = request->pagination().page();
    int pageSize = request->pagination().page_size();
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    int totalCount = 0;
    std::string error;
    auto records =
        queryManager_->queryMysqlDetailRecords(request->server_name(), time_range, page, pageSize, &totalCount, &error);
    if (!error.empty()) return queryErrorStatus(error);

    for (const auto &rec : records) {
        auto *protoRec = response->add_records();
        protoRec->set_server_name(rec.server_name);
        protoRec->set_instance(rec.instance);
        setTimestamp(protoRec->mutable_timestamp(), rec.timestamp);
        protoRec->set_mysql_host(rec.mysql_host);
        protoRec->set_mysql_port(rec.mysql_port);
        protoRec->set_up(rec.up);
        protoRec->set_version(rec.version);
        protoRec->set_role(rec.role);
        protoRec->set_max_connections(rec.max_connections);
        protoRec->set_threads_connected(rec.threads_connected);
        protoRec->set_threads_running(rec.threads_running);
        protoRec->set_aborted_connects(rec.aborted_connects);
        protoRec->set_questions(rec.questions);
        protoRec->set_com_select(rec.com_select);
        protoRec->set_com_insert(rec.com_insert);
        protoRec->set_com_update(rec.com_update);
        protoRec->set_com_delete(rec.com_delete);
        protoRec->set_com_commit(rec.com_commit);
        protoRec->set_com_rollback(rec.com_rollback);
        protoRec->set_slow_queries(rec.slow_queries);
        protoRec->set_innodb_buffer_pool_read_requests(rec.innodb_buffer_pool_read_requests);
        protoRec->set_innodb_buffer_pool_reads(rec.innodb_buffer_pool_reads);
        protoRec->set_innodb_buffer_pool_hit_percent(rec.innodb_buffer_pool_hit_percent);
        protoRec->set_innodb_row_lock_waits(rec.innodb_row_lock_waits);
        protoRec->set_innodb_row_lock_time_avg_ms(rec.innodb_row_lock_time_avg_ms);
        protoRec->set_replication_configured(rec.replication_configured);
        protoRec->set_replication_running(rec.replication_running);
        protoRec->set_replication_lag_seconds(rec.replication_lag_seconds);
        protoRec->set_connection_used_percent(rec.connection_used_percent);
        protoRec->set_qps(rec.qps);
        protoRec->set_tps(rec.tps);
        protoRec->set_slow_queries_rate(rec.slow_queries_rate);
        protoRec->set_innodb_row_lock_waits_rate(rec.innodb_row_lock_waits_rate);
    }

    response->set_total_count(totalCount);
    response->set_page(page);
    response->set_page_size(pageSize);

    return grpc::Status::OK;
}

} // namespace monitor
