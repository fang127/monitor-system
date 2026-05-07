#include "GrpcServer.h"
#include <grpcpp/support/status.h>
#include <chrono>
#include <mutex>
#include <utility>

namespace monitor {
namespace {

std::string resolveHostID(const monitor::proto::MonitorInfo &info) {
    if (info.has_host_info()) {
        const auto &hostInfo = info.host_info();
        const std::string &hostName = hostInfo.hostname();
        const std::string &ip = hostInfo.ip_address();
        if (!hostName.empty() && !ip.empty()) return hostName + "_" + ip;
        if (!ip.empty()) return ip;
        if (!hostName.empty()) return hostName;
    }
    return info.name();
}

} // namespace

::grpc::Status GrpcServerImpl::SetMonitorInfo(
    ::grpc::ServerContext *context,
    const ::monitor::proto::MonitorInfo *request,
    ::google::protobuf::Empty *response) {
    if (!request) {
        return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                              "Empty request");
    }

    std::string hostname = resolveHostID(*request);

    if (hostname.empty()) {
        return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                              "Missing hostname");
    }

    // Store the monitor info with a timestamp
    // to do ... Using lock-free structure for better performance if needed
    {
        std::lock_guard<std::mutex> lock(mutex_);
        hostDatas_[hostname] = {*request, std::chrono::system_clock::now()};
    }

    if (dispatcher_ && callback_) {
        monitor::proto::MonitorInfo requestCopy = *request;
        DataReceivedCallback callback = callback_;
        if (!dispatcher_->submitMonitorTask(
                hostname,
                [callback = std::move(callback),
                 requestCopy = std::move(requestCopy)]() {
                    callback(requestCopy);
                })) {
            return ::grpc::Status::OK;
        }
    } else if (callback_) {
        callback_(*request);
    }

    return ::grpc::Status::OK;
}

::grpc::Status GrpcServerImpl::GetMonitorInfo(
    ::grpc::ServerContext *context, const ::google::protobuf::Empty *request,
    ::monitor::proto::MonitorInfo *response) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!hostDatas_.empty()) {
        *response = hostDatas_.begin()->second.info;
        return ::grpc::Status::OK;
    }
    return ::grpc::Status(::grpc::StatusCode::NOT_FOUND,
                          "No monitor data available");
}

std::unordered_map<std::string, HostData> GrpcServerImpl::getAllHostDatas() {
    std::lock_guard<std::mutex> lock(mutex_);
    return hostDatas_;
}

bool GrpcServerImpl::getHostData(const std::string &hostID,
                                 HostData &hostData) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = hostDatas_.find(hostID);
    if (it != hostDatas_.end()) {
        hostData = it->second;
        return true;
    }
    return false;
}
}; // namespace monitor
