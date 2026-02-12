#include <grpcpp/server_builder.h>
#include "HostManager.h"
#include "GrpcServer.h"
#include "QueryService.h"
#include "monitor_info.pb.h"
#include "QueryManager.h"

constexpr char kDefaultListenAddress[] = "0.0.0.0:50051";
constexpr char kDefaultMysqlHost[] = "127.0.0.1";
constexpr char kDefaultMysqlUser[] = "harry";
constexpr char kDefaultMysqlPass[] = "hhxx588395..";
constexpr char kDefaultMysqlDb[] = "monitor_db";

int main(int argc, char *argv[])
{
    std::string listenAddress = kDefaultListenAddress;
    if (argc > 1) listenAddress = argv[1];

    std::cout << "Starting Monitor Client (Manager Mode)..." << std::endl;
    std::cout << "Listening on: " << listenAddress << std::endl;

    // create grpc server
    monitor::GrpcServerImpl service;

    // create host manager
    monitor::HostManager hostManager;
    service.setDataReceivedCallback(
        [&hostManager](const monitor::proto::MonitorInfo &info)
        { hostManager.onDataReceived(info); });

    // start host manager
    hostManager.start();

    // create QueryManager
    monitor::QueryManager queryManager;
#ifdef ENABLE_MYSQL
    if (queryManager.init(kDefaultMysqlHost, kDefaultMysqlUser,
                          kDefaultMysqlPass, kDefaultMysqlDb))
    {
        std::cout << "QueryManager initialized successfully" << std::endl;
    }
    else
    {
        std::cerr << "Warning: QueryManager initialization failed, "
                  << "query service will not be available" << std::endl;
    }
#endif

    // create query service
    monitor::QueryServiceImpl queryService(&queryManager);

    // start grpc server
    grpc::ServerBuilder builder;
    builder.AddListeningPort(listenAddress, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    builder.RegisterService(&queryService);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Monitor Client listening on " << listenAddress << std::endl;
    std::cout << "Waiting for workers to push data..." << std::endl;
    std::cout << "Query service available for performance data queries"
              << std::endl;
    server->Wait();

    return 0;
}