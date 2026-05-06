find_package(gRPC CONFIG REQUIRED)

add_executable(simulated_workers_push
    simulated_workers_push.cc
)

target_link_libraries(simulated_workers_push PRIVATE
    monitor_proto
    gRPC::grpc++
)
