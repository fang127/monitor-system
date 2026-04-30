#include "RpcClient.h"

#include <grpcpp/grpcpp.h>
#include <iostream>

namespace monitor {

RpcClient::RpcClient(const std::string &hostAddress)
    : hostAddress_(hostAddress) {
    auto channel =
        grpc::CreateChannel(hostAddress, grpc::InsecureChannelCredentials());
    stub_ = monitor::proto::GrpcManager::NewStub(channel);
}

bool RpcClient::getMonitorInfo(monitor::proto::MonitorInfo *info) {
    if (!info) return false;

    ::grpc::ClientContext context;
    ::google::protobuf::Empty request;

    ::grpc::Status status = stub_->GetMonitorInfo(&context, request, info);

    if (status.ok()) {
        return true;
    } else {
        std::cerr << "Failed to get monitor info from " << hostAddress_ << ": "
                  << status.error_message() << std::endl;
        return false;
    }
}

} // namespace monitor