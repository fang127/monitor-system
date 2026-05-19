#include "GrpcServer.h"
#include <grpcpp/support/status.h>
#include <chrono>
#include <mutex>
#include <utility>

namespace monitor {
namespace {

/**
 * @brief         从 MonitorInfo 中解析唯一主机 ID，优先使用主机名和 IP 地址，否则回退到 name 字段
 *
 * @param         info 监控数据
 * @return        主机唯一标识
 */
std::string resolveHostID(const monitor::proto::MonitorInfo &info) {
    // 如果有主机信息，优先使用主机名和IP地址组合来生成唯一ID
    // 如果主机名和IP地址都存在，使用 "hostname_ip" 形式；如果只有IP地址，使用IP地址；如果只有主机名，使用主机名
    // 如果都没有，则使用MonitorInfo的name字段作为ID
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

::grpc::Status GrpcServerImpl::SetMonitorInfo(::grpc::ServerContext *context,
                                              const ::monitor::proto::MonitorInfo *request,
                                              ::google::protobuf::Empty *response) {
    // 校验请求
    if (!request) return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "Empty request");

    std::string hostname = resolveHostID(*request);

    // 主机名不能为空，否则无法唯一标识主机
    if (hostname.empty()) return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "Missing hostname");

    // 保存带时间戳的监控数据
    // 后续如有性能需要，可考虑使用无锁结构
    {
        std::lock_guard<std::mutex> lock(mutex_);
        hostDatas_[hostname] = {*request, std::chrono::system_clock::now()};
    }

    // 利用线程池异步处理回调，避免阻塞gRPC线程
    if (dispatcher_ && callback_) {
        // 拷贝请求数据和回调函数到lambda中，确保它们在异步执行时仍然有效
        monitor::proto::MonitorInfo requestCopy = *request;
        DataReceivedCallback callback = callback_;
        // 提交异步任务到线程池，执行回调函数
        if (!dispatcher_->submitMonitorTask(
                hostname,
                [callback = std::move(callback), requestCopy = std::move(requestCopy)]() { callback(requestCopy); })) {
            return ::grpc::Status::OK;
        }
    } else if (callback_) {
        // 如果没有线程池，直接在当前线程执行回调函数（不推荐，可能会阻塞gRPC线程）
        callback_(*request);
    }

    return ::grpc::Status::OK;
}

::grpc::Status GrpcServerImpl::GetMonitorInfo(::grpc::ServerContext *context, const ::google::protobuf::Empty *request,
                                              ::monitor::proto::MonitorInfo *response) {
    // to do ... 可以考虑增加参数来指定获取特定主机的数据，或者增加分页支持等功能
    std::lock_guard<std::mutex> lock(mutex_);
    if (!hostDatas_.empty()) {
        *response = hostDatas_.begin()->second.info;
        return ::grpc::Status::OK;
    }
    return ::grpc::Status(::grpc::StatusCode::NOT_FOUND, "No monitor data available");
}

std::unordered_map<std::string, HostData> GrpcServerImpl::getAllHostDatas() {
    std::lock_guard<std::mutex> lock(mutex_);
    return hostDatas_;
}

bool GrpcServerImpl::getHostData(const std::string &hostID, HostData &hostData) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = hostDatas_.find(hostID);
    if (it != hostDatas_.end()) {
        hostData = it->second;
        return true;
    }
    return false;
}
}; // namespace monitor
