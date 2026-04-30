#include "QueryService.h"

#include <iostream>

namespace monitor {

QueryServiceImpl::QueryServiceImpl(QueryManager *queryManager)
    : queryManager_(queryManager) {}

TimeRange QueryServiceImpl::convertTimeRange(
    const ::monitor::proto::TimeRange &range) {
    TimeRange timeRange;
    timeRange.start_time =
        std::chrono::system_clock::from_time_t(range.start_time().seconds());
    timeRange.end_time =
        std::chrono::system_clock::from_time_t(range.end_time().seconds());
    return timeRange;
}

void QueryServiceImpl::setTimestamp(
    ::google::protobuf::Timestamp *ts,
    const std::chrono::system_clock::time_point &tp) {
    auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch())
            .count();
    ts->set_seconds(seconds);
    ts->set_nanos(0);
}

::grpc::Status QueryServiceImpl::QueryPerformance(
    ::grpc::ServerContext *context,
    const ::monitor::proto::QueryPerformanceRequest *request,
    ::monitor::proto::QueryPerformanceResponse *response) {
    (void)context;

    if (!queryManager_) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                            "Query manager not initialized");
    }

    // 验证时间范围
    TimeRange range = convertTimeRange(request->time_range());
    if (!queryManager_->validateTimeRange(range)) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                            "Invalid time range: start_time > end_time");
    }

    int page = request->pagination().page();
    int pageSize = request->pagination().page_size();
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    int totalCount = 0;
    auto records = queryManager_->queryPerformanceRecords(
        request->server_name(), range, page, pageSize, &totalCount);

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

::grpc::Status QueryServiceImpl::QueryTrend(
    ::grpc::ServerContext *context,
    const ::monitor::proto::QueryTrendRequest *request,
    ::monitor::proto::QueryTrendResponse *response) {
    (void)context;

    if (!queryManager_) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                            "Query manager not initialized");
    }

    TimeRange range = convertTimeRange(request->time_range());
    if (!queryManager_->validateTimeRange(range)) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                            "Invalid time range: start_time > end_time");
    }

    auto records = queryManager_->queryTrend(request->server_name(), range,
                                             request->interval_seconds());

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

::grpc::Status QueryServiceImpl::QueryAnomaly(
    ::grpc::ServerContext *context,
    const ::monitor::proto::QueryAnomalyRequest *request,
    ::monitor::proto::QueryAnomalyResponse *response) {
    (void)context;

    if (!queryManager_) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                            "Query manager not initialized");
    }

    TimeRange range = convertTimeRange(request->time_range());
    if (!queryManager_->validateTimeRange(range)) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                            "Invalid time range: start_time > end_time");
    }

    AnomalyThreshold thresholds;
    thresholds.cpu_threshold =
        request->cpu_threshold() > 0 ? request->cpu_threshold() : 80.0f;
    thresholds.memory_threshold =
        request->mem_threshold() > 0 ? request->mem_threshold() : 90.0f;
    thresholds.disk_threshold =
        request->disk_threshold() > 0 ? request->disk_threshold() : 85.0f;
    thresholds.change_rate_threshold = request->change_rate_threshold() > 0
                                           ? request->change_rate_threshold()
                                           : 0.5f;

    int page = request->pagination().page();
    int pageSize = request->pagination().page_size();
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    int totalCount = 0;
    auto records = queryManager_->queryAnomalyRecords(
        request->server_name(), range, thresholds, page, pageSize, &totalCount);

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

::grpc::Status QueryServiceImpl::QueryScoreRank(
    ::grpc::ServerContext *context,
    const ::monitor::proto::QueryScoreRankRequest *request,
    ::monitor::proto::QueryScoreRankResponse *response) {
    (void)context;

    if (!queryManager_) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                            "Query manager not initialized");
    }

    SortOrder order = (request->order() == ::monitor::proto::ASC)
                          ? SortOrder::ASC
                          : SortOrder::DESC;

    int page = request->pagination().page();
    int pageSize = request->pagination().page_size();
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    int totalCount = 0;
    auto records =
        queryManager_->queryServerScoreRank(order, page, pageSize, &totalCount);

    for (const auto &rec : records) {
        auto *protoRec = response->add_servers();
        protoRec->set_server_name(rec.server_name);
        protoRec->set_score(rec.score);
        setTimestamp(protoRec->mutable_last_update(), rec.last_updated);
        protoRec->set_status(rec.status == ServerStatus::ONLINE
                                 ? ::monitor::proto::ONLINE
                                 : ::monitor::proto::OFFLINE);
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

::grpc::Status QueryServiceImpl::QueryLatestScore(
    ::grpc::ServerContext *context,
    const ::monitor::proto::QueryLatestScoreRequest *request,
    ::monitor::proto::QueryLatestScoreResponse *response) {
    (void)context;
    (void)request;

    if (!queryManager_) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                            "Query manager not initialized");
    }

    ClusterStats stats;
    auto records = queryManager_->queryLatestServerScores(&stats);

    for (const auto &rec : records) {
        auto *protoRec = response->add_servers();
        protoRec->set_server_name(rec.server_name);
        protoRec->set_score(rec.score);
        setTimestamp(protoRec->mutable_last_update(), rec.last_updated);
        protoRec->set_status(rec.status == ServerStatus::ONLINE
                                 ? ::monitor::proto::ONLINE
                                 : ::monitor::proto::OFFLINE);
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

    return grpc::Status::OK;
}

::grpc::Status QueryServiceImpl::QueryNetDetail(
    ::grpc::ServerContext *context,
    const ::monitor::proto::QueryDetailRequest *request,
    ::monitor::proto::QueryNetDetailResponse *response) {
    (void)context;

    if (!queryManager_) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                            "Query manager not initialized");
    }

    TimeRange time_range = convertTimeRange(request->time_range());
    if (!queryManager_->validateTimeRange(time_range)) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                            "Invalid time range: start_time > end_time");
    }

    int page = request->pagination().page();
    int pageSize = request->pagination().page_size();
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    int totalCount = 0;
    auto records = queryManager_->queryNetDetailRecords(
        request->server_name(), time_range, page, pageSize, &totalCount);

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

::grpc::Status QueryServiceImpl::QueryDiskDetail(
    ::grpc::ServerContext *context,
    const ::monitor::proto::QueryDetailRequest *request,
    ::monitor::proto::QueryDiskDetailResponse *response) {
    (void)context;

    if (!queryManager_) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                            "Query manager not initialized");
    }

    TimeRange time_range = convertTimeRange(request->time_range());
    if (!queryManager_->validateTimeRange(time_range)) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                            "Invalid time range: start_time > end_time");
    }

    int page = request->pagination().page();
    int pageSize = request->pagination().page_size();
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    int totalCount = 0;
    auto records = queryManager_->queryDiskDetailRecords(
        request->server_name(), time_range, page, pageSize, &totalCount);

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

::grpc::Status QueryServiceImpl::QueryMemDetail(
    ::grpc::ServerContext *context,
    const ::monitor::proto::QueryDetailRequest *request,
    ::monitor::proto::QueryMemDetailResponse *response) {
    (void)context;

    if (!queryManager_) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                            "Query manager not initialized");
    }

    TimeRange time_range = convertTimeRange(request->time_range());
    if (!queryManager_->validateTimeRange(time_range)) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                            "Invalid time range: start_time > end_time");
    }

    int page = request->pagination().page();
    int pageSize = request->pagination().page_size();
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    int totalCount = 0;
    auto records = queryManager_->queryMemDetailRecords(
        request->server_name(), time_range, page, pageSize, &totalCount);

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

::grpc::Status QueryServiceImpl::QuerySoftIrqDetail(
    ::grpc::ServerContext *context,
    const ::monitor::proto::QueryDetailRequest *request,
    ::monitor::proto::QuerySoftIrqDetailResponse *response) {
    (void)context;

    if (!queryManager_) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                            "Query manager not initialized");
    }

    TimeRange time_range = convertTimeRange(request->time_range());
    if (!queryManager_->validateTimeRange(time_range)) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                            "Invalid time range: start_time > end_time");
    }

    int page = request->pagination().page();
    int pageSize = request->pagination().page_size();
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    int totalCount = 0;
    auto records = queryManager_->querySoftIrqDetailRecords(
        request->server_name(), time_range, page, pageSize, &totalCount);

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

} // namespace monitor