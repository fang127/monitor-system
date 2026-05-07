#include <grpcpp/server_builder.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include "HostManager.h"
#include "GrpcServer.h"
#include "ManagerConfig.h"
#include "ManagerDispatcher.h"
#include "ManagerMetrics.h"
#ifdef ENABLE_MYSQL
#include "MysqlConfig.h"
#include "MysqlConnectionPool.h"
#endif
#include "QueryService.h"
#include "monitor_info.pb.h"
#include "QueryManager.h"
#include "RedisConnectionPool.h"

constexpr char kDefaultListenAddress[] = "0.0.0.0:50051";

int main(int argc, char *argv[]) {
    std::string listenAddress = kDefaultListenAddress;
    if (argc > 1) listenAddress = argv[1];

    std::cout << "Starting Monitor Client (Manager Mode)..." << std::endl;
    std::cout << "Listening on: " << listenAddress << std::endl;

    // 加载配置
    monitor::ManagerConfig managerConfig = monitor::loadManagerConfigFromEnv();
    // 初始化监控指标
    monitor::ManagerMetrics metrics;
    // 创建并启动调度器
    monitor::ManagerDispatcher dispatcher(managerConfig, &metrics);
    dispatcher.start(); // 启动调度器后台线程，处理数据接收和查询请求

    // 初始化数据库连接池
#ifdef ENABLE_MYSQL
    const monitor::MysqlConfig mysqlConfig = monitor::loadMysqlConfigFromEnv();
    monitor::MysqlConnectionPool mysqlWritePool(mysqlConfig, managerConfig.mysql_write_pool_min,
                                                managerConfig.mysql_write_pool_max, managerConfig.mysql_connect_timeout,
                                                managerConfig.mysql_read_timeout, managerConfig.mysql_health_check,
                                                managerConfig.mysql_idle_ttl, &metrics);
    monitor::MysqlConnectionPool mysqlQueryPool(mysqlConfig, managerConfig.mysql_query_pool_min,
                                                managerConfig.mysql_query_pool_max, managerConfig.mysql_connect_timeout,
                                                managerConfig.mysql_read_timeout, managerConfig.mysql_health_check,
                                                managerConfig.mysql_idle_ttl, &metrics);
    mysqlWritePool.start(); // 创建连接并且启动后台线程监控连接池状态
    mysqlQueryPool.start(); // 创建连接并且启动后台线程监控连接池状态
#endif

    // 初始化 Redis 连接池和缓存
    std::unique_ptr<monitor::RedisConnectionPool> redisPool;
    std::unique_ptr<monitor::RedisCache> redisCache;
    if (managerConfig.redis_enabled) {
        redisPool = std::make_unique<monitor::RedisConnectionPool>(
            managerConfig.redis_uri, managerConfig.redis_pool_min, managerConfig.redis_pool_max,
            managerConfig.redis_connect_timeout, managerConfig.redis_read_timeout, managerConfig.redis_health_check,
            managerConfig.redis_idle_ttl, &metrics);
        redisPool->start();
        redisCache = std::make_unique<monitor::RedisCache>(redisPool.get(), managerConfig.redis_cache_ttl, &metrics);
    }

    // 创建 gRPC 服务实现
    monitor::GrpcServerImpl service;
    service.setDispatcher(&dispatcher);

    // create host manager
    monitor::HostManager hostManager;
    hostManager.configure(managerConfig, &metrics,
#ifdef ENABLE_MYSQL
                          &mysqlWritePool,
#else
                          nullptr,
#endif
                          redisCache.get());
    service.setDataReceivedCallback(
        [&hostManager](const monitor::proto::MonitorInfo &info) { hostManager.onDataReceived(info); });

    // 启动 HostManager 后台线程，处理主机状态更新和数据清理等任务
    hostManager.start();

    // 创建QueryManager，提供查询接口，依赖数据库连接池和缓存
    monitor::QueryManager queryManager;
#ifdef ENABLE_MYSQL
    std::cout << "MySQL config: " << mysqlConfig.host << ":" << mysqlConfig.port << "/" << mysqlConfig.database
              << std::endl;
    if (queryManager.init(mysqlConfig.host, mysqlConfig.port, mysqlConfig.user, mysqlConfig.password,
                          mysqlConfig.database, &mysqlQueryPool, &managerConfig, &metrics)) {
        std::cout << "QueryManager initialized successfully" << std::endl;
    } else {
        std::cerr << "Warning: QueryManager initialization failed, "
                  << "query service will not be available" << std::endl;
    }
#endif

    // create query service
    monitor::QueryServiceImpl queryService(&queryManager, &dispatcher, redisCache.get());

    std::atomic<bool> metricsRunning{true};
    std::thread metricsThread([&] {
        while (metricsRunning.load()) {
            std::this_thread::sleep_for(managerConfig.metrics_log_interval);
            metrics.print(dispatcher.queueSize(), dispatcher.workerCount(),
#ifdef ENABLE_MYSQL
                          mysqlWritePool.availableCount(), mysqlQueryPool.availableCount(),
#else
                          0, 0,
#endif
                          redisCache ? redisCache->availableCount() : 0);
        }
    });

    // start grpc server
    grpc::ServerBuilder builder;
    builder.SetSyncServerOption(grpc::ServerBuilder::SyncServerOption::NUM_CQS, managerConfig.grpc_num_cqs);
    builder.SetSyncServerOption(grpc::ServerBuilder::SyncServerOption::MIN_POLLERS, managerConfig.grpc_min_pollers);
    builder.SetSyncServerOption(grpc::ServerBuilder::SyncServerOption::MAX_POLLERS, managerConfig.grpc_max_pollers);
    builder.AddListeningPort(listenAddress, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);      // 注册数据接收服务
    builder.RegisterService(&queryService); // 注册查询服务
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Monitor Client listening on " << listenAddress << std::endl;
    std::cout << "Waiting for workers to push data..." << std::endl;
    std::cout << "Query service available for performance data queries" << std::endl;
    server->Wait();

    metricsRunning.store(false);
    if (metricsThread.joinable()) metricsThread.join();
    dispatcher.stop();
    return 0;
}
