#include "GrpcServer.h"
#include <grpcpp/support/status.h>
#include <chrono>
#include <mutex>

namespace monitor {
::grpc::Status GrpcServerImpl::SetMonitorInfo(
    ::grpc::ServerContext *context,
    const ::monitor::proto::MonitorInfo *request,
    ::google::protobuf::Empty *response) {
    if (!request) {
        return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                              "Empty request");
    }

    std::string hostname = request->name();
    if (hostname.empty() && request->has_host_info())
        hostname = request->host_info().hostname();

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

    std::cout << "Received monitor data from host: " << hostname << std::endl;

    // callback
    if (callback_) callback_(*request);

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