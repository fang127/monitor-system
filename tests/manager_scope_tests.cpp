#include "HostManager.h"
#include "QueryService.h"
#include "WorkerIdentity.h"
#include "GrpcServer.h"
#include "google/protobuf/empty.pb.h"
#include "monitor_info.pb.h"
#include "query_api.pb.h"

#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>

#include <iostream>
#include <map>
#include <string>

namespace grpc {
namespace testing {

class ServerContextTestSpouse {
public:
    explicit ServerContextTestSpouse(ServerContext *context) : context_(context) {}

    void addClientMetadata(const std::string &key, const std::string &value) {
        clientMetadataStorage_.insert(std::pair<std::string, std::string>(key, value));
        context_->client_metadata_.map()->clear();
        for (const auto &item : clientMetadataStorage_) {
            context_->client_metadata_.map()->insert(std::pair<grpc::string_ref, grpc::string_ref>(
                item.first.c_str(), grpc::string_ref(item.second.data(), item.second.size())));
        }
    }

private:
    ServerContext *context_;
    std::multimap<std::string, std::string> clientMetadataStorage_;
};

} // namespace testing
} // namespace grpc

namespace {

monitor::proto::MonitorInfo makeMonitorInfo() {
    monitor::proto::MonitorInfo info;
    info.set_name("unit-host");
    auto *host = info.mutable_host_info();
    host->set_hostname("unit-host");
    host->set_ip_address("127.0.0.1");
    return info;
}

bool expectStatus(const grpc::Status &status, grpc::StatusCode code, const std::string &name) {
    if (status.error_code() != code) {
        std::cerr << name << " 状态码不符合预期，got=" << status.error_code() << " want=" << code
                  << " message=" << status.error_message() << std::endl;
        return false;
    }
    return true;
}

bool testGrpcRejectsMissingWorkerIdentity() {
    monitor::GrpcServerImpl service;
    grpc::ServerContext context;
    auto request = makeMonitorInfo();
    google::protobuf::Empty response;

    const grpc::Status status = service.SetMonitorInfo(&context, &request, &response);
    return expectStatus(status, grpc::StatusCode::UNAUTHENTICATED, "缺少 worker identity 的推送");
}

bool testGrpcAcceptsValidWorkerIdentity() {
    monitor::GrpcServerImpl service;
    bool callbackCalled = false;
    monitor::WorkerIdentity receivedIdentity;
    service.setDataReceivedCallback(
        [&](const monitor::proto::MonitorInfo &, const monitor::WorkerIdentity &workerIdentity) {
            callbackCalled = true;
            receivedIdentity = workerIdentity;
        });

    grpc::ServerContext context;
    grpc::testing::ServerContextTestSpouse contextSpouse(&context);
    contextSpouse.addClientMetadata("x-monitor-worker-id", "worker-a");
    contextSpouse.addClientMetadata("x-monitor-worker-token", "token-a");
    auto request = makeMonitorInfo();
    google::protobuf::Empty response;

    const grpc::Status status = service.SetMonitorInfo(&context, &request, &response);
    if (!expectStatus(status, grpc::StatusCode::OK, "带 worker identity 的推送")) return false;
    if (!callbackCalled) {
        std::cerr << "有效 worker 推送没有触发 manager 回调" << std::endl;
        return false;
    }
    if (receivedIdentity.worker_id != "worker-a" || receivedIdentity.worker_token != "token-a") {
        std::cerr << "manager 回调收到的 worker identity 不符合预期" << std::endl;
        return false;
    }
    return true;
}

bool testHostManagerDropsUnregisteredWorker() {
    monitor::HostManager hostManager;
    monitor::ManagerConfig config;
    monitor::ManagerMetrics metrics;
    hostManager.configure(config, &metrics, nullptr, nullptr);

    hostManager.onDataReceived(makeMonitorInfo(), monitor::WorkerIdentity{"unknown-worker", "bad-token"});

    if (metrics.dropped_monitor_samples.load() != 1) {
        std::cerr << "未知 worker 应被计入丢弃指标，got=" << metrics.dropped_monitor_samples.load() << std::endl;
        return false;
    }
    return true;
}

bool testQueryServiceRejectsMissingScopeBeforeQueryManager() {
    monitor::QueryServiceImpl service(nullptr);
    grpc::ServerContext context;
    monitor::proto::QueryLatestScoreRequest request;
    monitor::proto::QueryLatestScoreResponse response;

    const grpc::Status status = service.QueryLatestScore(&context, &request, &response);
    return expectStatus(status, grpc::StatusCode::UNAUTHENTICATED, "缺少查询作用域的 latest 查询");
}

bool testQueryServiceRequiresManagerAfterScopeAccepted() {
    monitor::QueryServiceImpl service(nullptr);
    grpc::ServerContext context;
    monitor::proto::QueryLatestScoreRequest request;
    monitor::proto::QueryLatestScoreResponse response;
    request.mutable_scope()->set_tenant_id("tenant-a");
    request.mutable_scope()->set_team_id("team-a");

    const grpc::Status status = service.QueryLatestScore(&context, &request, &response);
    return expectStatus(status, grpc::StatusCode::UNAVAILABLE, "带查询作用域但未初始化 QueryManager 的 latest 查询");
}

} // namespace

int main() {
    bool ok = true;
    ok = testGrpcRejectsMissingWorkerIdentity() && ok;
    ok = testGrpcAcceptsValidWorkerIdentity() && ok;
    ok = testHostManagerDropsUnregisteredWorker() && ok;
    ok = testQueryServiceRejectsMissingScopeBeforeQueryManager() && ok;
    ok = testQueryServiceRequiresManagerAfterScopeAccepted() && ok;
    return ok ? 0 : 1;
}
