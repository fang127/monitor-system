find_package(libmysqlclient CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(folly CONFIG REQUIRED)
find_package(redis++ CONFIG REQUIRED)

set(MANAGER_LIBS
    monitor_proto
    gRPC::grpc++
    Folly::folly
)

# 优先使用redis++的动态库，如果找不到则使用静态库
if(TARGET redis++::redis++)
    list(APPEND MANAGER_LIBS redis++::redis++)
elseif(TARGET redis++::redis++_static)
    list(APPEND MANAGER_LIBS redis++::redis++_static)
else()
    message(FATAL_ERROR "redis-plus-plus target not found")
endif()


option(ENABLE_MYSQL "Enable MySQL support" ON)

set(MANAGER_CORE_SOURCES
    src/HostManager.cc
    src/QueryManager.cc
    src/ManagerConfig.cc
    src/ManagerDispatcher.cc
    src/MysqlConnectionPool.cc
    src/RedisConnectionPool.cc
    src/rpc/GrpcServer.cc
    src/rpc/QueryService.cc
)

add_library(manager_core STATIC ${MANAGER_CORE_SOURCES})

target_include_directories(manager_core PUBLIC
    ${PROJECT_SOURCE_DIR}/manager/include
    ${PROJECT_SOURCE_DIR}/manager/include/rpc
)

if(ENABLE_MYSQL)
    target_compile_definitions(manager_core PUBLIC ENABLE_MYSQL) # 定义宏以启用MySQL相关代码
    list(APPEND MANAGER_LIBS libmysqlclient::libmysqlclient)
endif()

target_link_libraries(manager_core PUBLIC ${MANAGER_LIBS})

add_executable(manager src/main.cc)
target_link_libraries(manager PUBLIC manager_core)
