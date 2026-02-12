find_package(libmysqlclient CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

option(ENABLE_MYSQL "Enable MySQL support" ON)

set(MANAGER_SOURCES
    src/main.cc
    src/HostManager.cc
    src/QueryManager.cc
    src/rpc/GrpcServer.cc
    src/rpc/QueryService.cc
)

add_executable(manager ${MANAGER_SOURCES})

target_include_directories(manager PUBLIC 
    ${PROJECT_SOURCE_DIR}/manager/include
    ${PROJECT_SOURCE_DIR}/manager/include/rpc
)

set(MANAGER_LIBS
    monitor_proto    
    gRPC::grpc++
)

if(ENABLE_MYSQL)
    list(APPEND MANAGER_LIBS libmysqlclient::libmysqlclient)
endif()

target_link_libraries(manager PUBLIC ${MANAGER_LIBS})