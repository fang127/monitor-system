#include "GrpcManagerImpl.h"
#include "MetricCollector.h"

namespace monitor {
/**
 * @brief         构造 gRPC Manager 服务实现并初始化指标采集器
 *
 */
GrpcManagerImpl::GrpcManagerImpl() { collector_ = std::make_unique<MetricCollector>(); }

/**
 * @brief         采集并返回当前监控数据
 *
 * @param         context gRPC 服务端上下文
 * @param         request 空请求对象
 * @param         response 监控数据响应
 * @return        gRPC 调用状态
 */
::grpc::Status GrpcManagerImpl::GetMonitorInfo(::grpc::ServerContext *context, const ::google::protobuf::Empty *request,
                                               ::monitor::proto::MonitorInfo *response) {
    collector_->collectAll(response);
    return ::grpc::Status::OK;
}
} // namespace monitor
