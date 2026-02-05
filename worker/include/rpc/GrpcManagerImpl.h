/*
 * @Author: harry
 * @Date: 2026-02-05 14:32:44
 * @Version: 1.0
 * @LastEditors: harry
 * @LastEditTime: 2026-02-05 14:37:04
 * @Description:
 * @FilePath: /monitor-system/worker/include/rpc/GrpcManagerImpl.h
 */
#pragma once

#include <grpcpp/support/status.h>

#include <memory>

#include "monitor/metric_collector.h"

#include "monitor_info.grpc.pb.h"
#include "monitor_info.pb.h"

namespace monitor
{
class GrpcManagerImpl : public monitor::proto::GrpcManager::Service
{
public:
    GrpcManagerImpl() = default;
    virtual ~GrpcManagerImpl() = default;

private:
    std::unique_ptr<MetricCollector> collector_;
};
}; // namespace monitor