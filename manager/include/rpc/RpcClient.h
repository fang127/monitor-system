#pragma once

#include <string>

#include "monitor_info.grpc.pb.h"
#include "monitor_info.pb.h"

namespace monitor {

/**
 * @brief         test RPC client for fetching monitoring data from a remote
 * host.
 *
 */
class RpcClient {
public:
    explicit RpcClient(const std::string &hostAddress = "localhost:50051");
    ~RpcClient() = default;

    /**
     * @brief         Get the Monitor Info object
     *
     * @param         monitor_info
     * @return
     * @return
     */
    bool getMonitorInfo(monitor::proto::MonitorInfo *info);

    /**
     * @brief         Get the Host Address object
     *
     * @return
     */
    const std::string &getHostAddress() const { return hostAddress_; }

private:
    std::unique_ptr<monitor::proto::GrpcManager::Stub> stub_;
    std::string hostAddress_;
};

} // namespace monitor