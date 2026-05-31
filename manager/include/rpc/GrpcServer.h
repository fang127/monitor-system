#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include "ManagerDispatcher.h"
#include "WorkerIdentity.h"
#include "monitor_info.grpc.pb.h"
#include "monitor_info.pb.h"

namespace monitor {
/**
 * @brief         监控数据结构体
 *
 */
struct HostData {
    monitor::proto::MonitorInfo info;
    std::chrono::system_clock::time_point timestamp;
};

// 定义一个回调函数类型，用于处理接收到的监控信息
using DataReceivedCallback = std::function<void(const monitor::proto::MonitorInfo &, const WorkerIdentity &)>;

/**
 * @brief         gRPC 监控数据服务实现，负责接收 worker
 * 推送的监控信息，维护主机数据缓存，并通过回调把数据交给后续处理模块
 *
 */
class GrpcServerImpl final : public monitor::proto::GrpcManager::Service {
public:
    GrpcServerImpl() = default;
    virtual ~GrpcServerImpl() = default;

    /**
     * @brief         接收并保存 worker 推送的监控数据，同时触发已注册的数据接收回调
     *
     * @param         context gRPC 服务端上下文
     * @param         request 监控数据请求
     * @param         response 空响应对象
     * @return        gRPC 调用状态
     */
    ::grpc::Status SetMonitorInfo(::grpc::ServerContext *context, const ::monitor::proto::MonitorInfo *request,
                                  ::google::protobuf::Empty *response) override;

    /**
     * @brief         获取当前缓存中的第一条监控数据，主要用于兼容旧查询接口
     *
     * @param         context gRPC 服务端上下文
     * @param         request 空请求对象
     * @param         response 监控数据响应
     * @return        gRPC 调用状态
     */
    ::grpc::Status GetMonitorInfo(::grpc::ServerContext *context, const ::google::protobuf::Empty *request,
                                  ::monitor::proto::MonitorInfo *response) override;

    /**
     * @brief         设置数据接收回调函数
     *
     * @param         callback 数据接收回调函数
     */
    void setDataReceivedCallback(const DataReceivedCallback &callback) { callback_ = callback; }

    /**
     * @brief         以移动方式设置数据接收回调函数
     *
     * @param         callback 数据接收回调函数
     */
    void setDataReceivedCallback(DataReceivedCallback &&callback) { callback_ = std::move(callback); }

    /**
     * @brief         设置任务调度器，用于异步分发监控推送处理任务
     *
     * @param         dispatcher 调度器指针
     */
    void setDispatcher(ManagerDispatcher *dispatcher) { dispatcher_ = dispatcher; }

    /**
     * @brief         获取所有主机的监控数据快照
     *
     * @return        主机 ID 到监控数据的映射
     */
    std::unordered_map<std::string, HostData> getAllHostDatas();

    /**
     * @brief         根据主机 ID 获取指定主机的监控数据
     *
     * @param         hostID 主机唯一标识
     * @param         hostData 输出参数，保存查询到的主机数据
     * @return        找到主机数据返回 true，否则返回 false
     */
    bool getHostData(const std::string &hostID, HostData &hostData);

private:
    std::mutex mutex_;                                    // 保护 hostDatas_ 的互斥锁
    std::unordered_map<std::string, HostData> hostDatas_; // 存储主机数据的映射，键为主机ID，值为HostData结构体
    DataReceivedCallback callback_;                       // 用于处理接收到的监控信息的回调函数
    ManagerDispatcher *dispatcher_ = nullptr;             // 指向 ManagerDispatcher 的指针，用于分发监控信息
};
} // namespace monitor
