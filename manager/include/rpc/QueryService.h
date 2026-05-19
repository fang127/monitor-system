#pragma once

#include "query_api.grpc.pb.h"
#include "query_api.pb.h"
#include "QueryManager.h"
#include "ManagerDispatcher.h"
#include "RedisConnectionPool.h"

namespace monitor {
/**
 * @brief         查询 gRPC 服务实现，提供性能、趋势、异常、评分和各类明细指标的查询接口
 *
 */
class QueryServiceImpl final : public monitor::proto::QueryService::Service {
public:
    /**
     * @brief         构造查询服务实现，注入查询管理器、任务调度器和 Redis 缓存
     *
     * @param         query_manager 查询管理器
     * @param         dispatcher 任务调度器，可为空
     * @param         redis_cache Redis 缓存，可为空
     */
    explicit QueryServiceImpl(QueryManager *query_manager, ManagerDispatcher *dispatcher = nullptr,
                              RedisCache *redis_cache = nullptr);
    virtual ~QueryServiceImpl() = default;

    /**
     * @brief         按请求条件查询服务器历史性能数据
     *
     * @param         context gRPC 服务端上下文
     * @param         request 性能查询请求
     * @param         response 性能查询响应
     * @return        gRPC 调用状态
     */
    ::grpc::Status QueryPerformance(::grpc::ServerContext *context,
                                    const ::monitor::proto::QueryPerformanceRequest *request,
                                    ::monitor::proto::QueryPerformanceResponse *response) override;

    /**
     * @brief         按请求条件查询服务器性能趋势数据
     *
     * @param         context gRPC 服务端上下文
     * @param         request 趋势查询请求
     * @param         response 趋势查询响应
     * @return        gRPC 调用状态
     */
    ::grpc::Status QueryTrend(::grpc::ServerContext *context, const ::monitor::proto::QueryTrendRequest *request,
                              ::monitor::proto::QueryTrendResponse *response) override;

    /**
     * @brief         按请求条件查询服务器性能异常记录
     *
     * @param         context gRPC 服务端上下文
     * @param         request 异常查询请求
     * @param         response 异常查询响应
     * @return        gRPC 调用状态
     */
    ::grpc::Status QueryAnomaly(::grpc::ServerContext *context, const ::monitor::proto::QueryAnomalyRequest *request,
                                ::monitor::proto::QueryAnomalyResponse *response) override;

    /**
     * @brief         查询服务器性能评分排行
     *
     * @param         context gRPC 服务端上下文
     * @param         request 评分排行查询请求
     * @param         response 评分排行查询响应
     * @return        gRPC 调用状态
     */
    ::grpc::Status QueryScoreRank(::grpc::ServerContext *context,
                                  const ::monitor::proto::QueryScoreRankRequest *request,
                                  ::monitor::proto::QueryScoreRankResponse *response) override;

    /**
     * @brief         查询最新服务器评分和集群统计信息
     *
     * @param         context gRPC 服务端上下文
     * @param         request 最新评分查询请求
     * @param         response 最新评分查询响应
     * @return        gRPC 调用状态
     */
    ::grpc::Status QueryLatestScore(::grpc::ServerContext *context,
                                    const ::monitor::proto::QueryLatestScoreRequest *request,
                                    ::monitor::proto::QueryLatestScoreResponse *response) override;

    /**
     * @brief         查询网络指标明细数据
     *
     * @param         context gRPC 服务端上下文
     * @param         request 明细查询请求
     * @param         response 网络明细查询响应
     * @return        gRPC 调用状态
     */
    ::grpc::Status QueryNetDetail(::grpc::ServerContext *context, const ::monitor::proto::QueryDetailRequest *request,
                                  ::monitor::proto::QueryNetDetailResponse *response) override;

    /**
     * @brief         查询磁盘指标明细数据
     *
     * @param         context gRPC 服务端上下文
     * @param         request 明细查询请求
     * @param         response 磁盘明细查询响应
     * @return        gRPC 调用状态
     */
    ::grpc::Status QueryDiskDetail(::grpc::ServerContext *context, const ::monitor::proto::QueryDetailRequest *request,
                                   ::monitor::proto::QueryDiskDetailResponse *response) override;

    /**
     * @brief         查询内存指标明细数据
     *
     * @param         context gRPC 服务端上下文
     * @param         request 明细查询请求
     * @param         response 内存明细查询响应
     * @return        gRPC 调用状态
     */
    ::grpc::Status QueryMemDetail(::grpc::ServerContext *context, const ::monitor::proto::QueryDetailRequest *request,
                                  ::monitor::proto::QueryMemDetailResponse *response) override;

    /**
     * @brief         查询软中断指标明细数据
     *
     * @param         context gRPC 服务端上下文
     * @param         request 明细查询请求
     * @param         response 软中断明细查询响应
     * @return        gRPC 调用状态
     */
    ::grpc::Status QuerySoftIrqDetail(::grpc::ServerContext *context,
                                      const ::monitor::proto::QueryDetailRequest *request,
                                      ::monitor::proto::QuerySoftIrqDetailResponse *response) override;

    /**
     * @brief         查询 MySQL 指标明细数据
     *
     * @param         context gRPC 服务端上下文
     * @param         request 明细查询请求
     * @param         response MySQL 明细查询响应
     * @return        gRPC 调用状态
     */
    ::grpc::Status QueryMysqlDetail(::grpc::ServerContext *context, const ::monitor::proto::QueryDetailRequest *request,
                                    ::monitor::proto::QueryMysqlDetailResponse *response) override;

private:
    /**
     * @brief         将 protobuf 的 TimeRange 消息转换为 QueryManager 内部使用的 TimeRange 结构
     *
     * @param         range protobuf 时间范围
     * @return        内部时间范围结构
     */
    TimeRange convertTimeRange(const ::monitor::proto::TimeRange &range);

    /**
     * @brief         设置 protobuf 时间戳
     *
     * @param         ts 待写入的 protobuf 时间戳
     * @param         tp 系统时间点
     */
    void setTimestamp(::google::protobuf::Timestamp *ts, const std::chrono::system_clock::time_point &tp);

    QueryManager *queryManager_;    // 查询管理器指针，负责查询逻辑和数据读取
    ManagerDispatcher *dispatcher_; // 调度器指针，负责把查询任务分发到查询工作线程
    RedisCache *redisCache_;        // Redis 缓存指针，用于缓存查询结果并减少重复数据读取
};

} // namespace monitor
