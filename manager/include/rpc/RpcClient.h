#pragma once

#include <string>

#include "monitor_info.grpc.pb.h"
#include "monitor_info.pb.h"

namespace monitor {

/**
 * @brief         测试用 RPC 客户端，用于从远端主机获取监控数据
 *
 */
class RpcClient {
public:
    /**
     * @brief         构造 RPC 客户端并连接指定主机地址
     *
     * @param         hostAddress 远端 gRPC 服务地址
     */
    explicit RpcClient(const std::string &hostAddress = "localhost:50051");
    ~RpcClient() = default;

    /**
     * @brief         获取远端主机的监控数据
     *
     * @param         info 输出参数，保存监控数据
     * @return        获取成功返回 true，否则返回 false
     */
    bool getMonitorInfo(monitor::proto::MonitorInfo *info);

    /**
     * @brief         获取远端主机地址
     *
     * @return        主机地址引用
     */
    const std::string &getHostAddress() const { return hostAddress_; }

private:
    std::unique_ptr<monitor::proto::GrpcManager::Stub> stub_; // gRPC 客户端 stub
    std::string hostAddress_;                                 // 远端主机地址
};

} // namespace monitor
