#include "GrpcManagerImpl.h"
#include "MetricCollector.h"

namespace monitor
{
GrpcManagerImpl::GrpcManagerImpl()
{
    collector_ = std::make_unique<MetricCollector>();
}

::grpc::Status GrpcManagerImpl::GetMonitorInfo(
    ::grpc::ServerContext *context, const ::google::protobuf::Empty *request,
    ::monitor::proto::MonitorInfo *response)
{
    collector_->collectAll(response);
    return ::grpc::Status::OK;
}
} // namespace monitor