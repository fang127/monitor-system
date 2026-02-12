#pragma once

#include "query_api.grpc.pb.h"
#include "query_api.pb.h"
#include "QueryManager.h"

namespace monitor
{
/**
 * @brief         this class implements the gRPC service defined in
 * query_api.proto, providing methods to handle various types of queries related
 * to performance data, trends, anomalies, scores, and detailed metrics for
 * network, disk, memory, and soft interrupts.
 *
 */
class QueryServiceImpl final : public monitor::proto::QueryService::Service
{
public:
    explicit QueryServiceImpl(QueryManager *query_manager);
    virtual ~QueryServiceImpl() = default;

    /**
     * @brief         query performance data based on the provided request
     * parameters, which may include filters such as time range, metric types,
     * and other criteria. The response will contain the requested performance
     * data formatted according to the specifications defined in the protobuf
     * messages. This method serves as the primary entry point for clients to
     * retrieve performance metrics from the monitoring system, enabling them to
     * analyze and visualize the performance of their applications or
     * infrastructure over time.
     *
     * @param         context
     * @param         request
     * @param         response
     * @return
     */
    ::grpc::Status QueryPerformance(
        ::grpc::ServerContext *context,
        const ::monitor::proto::QueryPerformanceRequest *request,
        ::monitor::proto::QueryPerformanceResponse *response) override;

    /**
     * @brief         query performance trends based on the provided request
     * parameters, which may include filters such as time range, metric types,
     * and other criteria. The response will contain the requested performance
     * data formatted according to the specifications defined in the protobuf
     * messages. This method serves as the primary entry point for clients to
     * retrieve performance metrics from the monitoring system, enabling them to
     * analyze and visualize the performance of their applications or
     * infrastructure over time.
     * @param         context
     * @param         request
     * @param         response
     * @return
     */
    ::grpc::Status QueryTrend(
        ::grpc::ServerContext *context,
        const ::monitor::proto::QueryTrendRequest *request,
        ::monitor::proto::QueryTrendResponse *response) override;

    /**
     * @brief         query performance anomalies based on the provided request
     * parameters, which may include filters such as time range, metric types,
     * and other criteria. The response will contain the requested performance
     *
     * @param         context
     * @param         request
     * @param         response
     * @return
     */
    ::grpc::Status QueryAnomaly(
        ::grpc::ServerContext *context,
        const ::monitor::proto::QueryAnomalyRequest *request,
        ::monitor::proto::QueryAnomalyResponse *response) override;

    /**
     * @brief         query performance score ranks based on the provided
     * request parameters, which may include filters such as time range, metric
     * types, and other criteria. The response will contain the requested
     * performance.
     *
     * @param         context
     * @param         request
     * @param         response
     * @return
     */
    ::grpc::Status QueryScoreRank(
        ::grpc::ServerContext *context,
        const ::monitor::proto::QueryScoreRankRequest *request,
        ::monitor::proto::QueryScoreRankResponse *response) override;

    /**
     * @brief         query the latest performance score based on the provided
     * request parameters, which may include filters such as time range, metric
     * types, and other criteria. The response will contain the requested
     * performance score formatted according to the specifications defined in
     * the protobuf messages. This method allows clients to retrieve the most
     * recent performance score for their applications or infrastructure,
     * enabling them to quickly assess the current performance status and make
     * informed decisions based on the latest performance data available in the
     * monitoring system.
     *
     * @param         context
     * @param         request
     * @param         response
     * @return
     */
    ::grpc::Status QueryLatestScore(
        ::grpc::ServerContext *context,
        const ::monitor::proto::QueryLatestScoreRequest *request,
        ::monitor::proto::QueryLatestScoreResponse *response) override;

    /**
     * @brief         query detailed performance data for network metrics based
     * on the provided request parameters, which may include filters such as
     * time range, specific network interfaces, and other criteria. The response
     * will contain the requested detailed performance data formatted according
     * to the specifications defined in the protobuf messages. This method
     * allows clients to retrieve in-depth performance metrics for
     * network-related activities, enabling them to analyze and troubleshoot
     * network performance issues effectively.
     *
     * @param         context
     * @param         request
     * @param         response
     * @return
     */
    ::grpc::Status QueryNetDetail(
        ::grpc::ServerContext *context,
        const ::monitor::proto::QueryDetailRequest *request,
        ::monitor::proto::QueryNetDetailResponse *response) override;

    /**
     * @brief         query detailed performance data for disk metrics based on
     * the provided request parameters, which may include filters such as time
     * range, specific disk devices, and other criteria. The response will
     * contain the requested detailed performance data formatted according to
     * the specifications defined in the protobuf messages. This method allows
     * clients to retrieve in-depth performance metrics for disk-related
     * activities, enabling them to analyze and troubleshoot disk performance
     * issues effectively.
     *
     * @param         context
     * @param         request
     * @param         response
     * @return
     */
    ::grpc::Status QueryDiskDetail(
        ::grpc::ServerContext *context,
        const ::monitor::proto::QueryDetailRequest *request,
        ::monitor::proto::QueryDiskDetailResponse *response) override;

    /**
     * @brief         query detailed performance data for memory metrics based
     * on the provided request parameters, which may include filters such as
     * time range, specific memory types, and other criteria. The response will
     * contain the requested detailed performance data formatted according to
     * the specifications defined in the protobuf messages. This method allows
     * clients to retrieve in-depth performance metrics for memory-related
     * activities, enabling them to analyze and troubleshoot memory performance
     * issues effectively.
     *
     * @param         context
     * @param         request
     * @param         response
     * @return
     */
    ::grpc::Status QueryMemDetail(
        ::grpc::ServerContext *context,
        const ::monitor::proto::QueryDetailRequest *request,
        ::monitor::proto::QueryMemDetailResponse *response) override;

    /**
     * @brief         query detailed performance data for soft interrupt metrics
     * based on the provided request parameters, which may include filters such
     * as time range, specific interrupt types, and other criteria. The response
     * will contain the requested detailed performance data formatted according
     * to the specifications defined in the protobuf messages. This method
     * allows clients to retrieve in-depth performance metrics for soft
     * interrupt-related activities, enabling them to analyze and troubleshoot
     * soft interrupt performance issues effectively.
     *
     * @param         context
     * @param         request
     * @param         response
     * @return
     */
    ::grpc::Status QuerySoftIrqDetail(
        ::grpc::ServerContext *context,
        const ::monitor::proto::QueryDetailRequest *request,
        ::monitor::proto::QuerySoftIrqDetailResponse *response) override;

private:
    /**
     * @brief         convert a protobuf TimeRange message to a TimeRange struct
     * used internally by the QueryManager.
     *
     * @param         proto_range
     * @return
     */
    TimeRange convertTimeRange(const ::monitor::proto::TimeRange &range);

    /**
     * @brief         Set the Timestamp object
     *
     * @param         ts
     * @param         tp
     */
    void setTimestamp(::google::protobuf::Timestamp *ts,
                      const std::chrono::system_clock::time_point &tp);

    QueryManager *queryManager_;
};

} // namespace monitor