/*!
 * @file          CpuLoadMonitor.h
 * @author        harry
 * @date          2026-02-09
 */
#pragma once

#include <grpcpp/support/status.h>

#include <memory>

#include "MetricCollector.h"

#include "monitor_info.grpc.pb.h"
#include "monitor_info.pb.h"

namespace monitor
{
/**
 * @brief         gRPC Manager Implementation
 *
 */
class GrpcManagerImpl : public monitor::proto::GrpcManager::Service
{
public:
    GrpcManagerImpl();
    virtual ~GrpcManagerImpl();

    /**
     * @brief         Get the Monitor Info object
     *
     * @param         context
     * @param         request
     * @param         response
     * @return        ::grpc::Status
     */
    ::grpc::Status GetMonitorInfo(
        ::grpc::ServerContext *context,
        const ::google::protobuf::Empty *request,
        ::monitor::proto::MonitorInfo *response) override;

private:
    std::unique_ptr<MetricCollector> collector_;
};
}; // namespace monitor