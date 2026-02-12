#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include "monitor_info.grpc.pb.h"
#include "monitor_info.pb.h"

namespace monitor
{
struct HostData
{
    monitor::proto::MonitorInfo info;
    std::chrono::system_clock::time_point timestamp;
};

using DataReceivedCallback =
    std::function<void(const monitor::proto::MonitorInfo &)>;

/**
 * @brief         gRPC server implementation for handling monitor information.
 * This class provides methods to set and get monitor information, as well as
 * manage callbacks for received data. It maintains an internal map of host
 * data, allowing retrieval of information based on host identifiers.
 *
 */
class GrpcServerImpl final : public monitor::proto::GrpcManager::Service
{
public:
    GrpcServerImpl() = default;
    virtual ~GrpcServerImpl() = default;

    /**
     * @brief         Set the Monitor Info object
     *
     * @param         context
     * @param         request
     * @param         response
     * @return
     */
    ::grpc::Status SetMonitorInfo(::grpc::ServerContext *context,
                                  const ::monitor::proto::MonitorInfo *request,
                                  ::google::protobuf::Empty *response) override;

    /**
     * @brief         Get the first Monitor Info object
     *
     * @param         context
     * @param         request
     * @param         response
     * @return
     */
    ::grpc::Status GetMonitorInfo(
        ::grpc::ServerContext *context,
        const ::google::protobuf::Empty *request,
        ::monitor::proto::MonitorInfo *response) override;

    /**
     * @brief         Set the Data Received Callback object
     *
     * @param         callback
     */
    void setDataReceivedCallback(const DataReceivedCallback &callback)
    {
        callback_ = callback;
    }

    /**
     * @brief         Set the Data Received Callback object
     *
     * @param         callback
     */
    void setDataReceivedCallback(DataReceivedCallback &&callback)
    {
        callback_ = std::move(callback);
    }

    /**
     * @brief         Get the All Host Datas object
     *
     * @return
     */
    std::unordered_map<std::string, HostData> getAllHostDatas();

    /**
     * @brief         Get the Host Data object
     *
     * @param         hostId unique identifier for the host
     * @param         hostData output parameter to store the retrieved HostData
     * @return
     * @return
     */
    bool getHostData(const std::string &hostID, HostData &hostData);

private:
    std::mutex mutex_;
    std::unordered_map<std::string, HostData> hostDatas_;
    DataReceivedCallback callback_;
};
} // namespace monitor