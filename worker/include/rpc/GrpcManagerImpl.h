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

namespace monitor {
/**
 * @brief         worker 侧 gRPC Manager 服务实现，按请求采集并返回当前监控数据
 *
 */
class GrpcManagerImpl : public monitor::proto::GrpcManager::Service {
public:
    /**
     * @brief         构造 gRPC Manager 服务实现并初始化指标采集器
     *
     */
    GrpcManagerImpl();

    /**
     * @brief         析构 gRPC Manager 服务实现
     *
     */
    virtual ~GrpcManagerImpl();

    /**
     * @brief         采集并返回当前监控数据
     *
     * @param         context gRPC 服务端上下文
     * @param         request 空请求对象
     * @param         response 监控数据响应
     * @return        gRPC 调用状态
     */
    ::grpc::Status GetMonitorInfo(
        ::grpc::ServerContext *context,
        const ::google::protobuf::Empty *request,
        ::monitor::proto::MonitorInfo *response) override;

private:
    std::unique_ptr<MetricCollector> collector_; // 指标采集器实例
};
}; // namespace monitor
