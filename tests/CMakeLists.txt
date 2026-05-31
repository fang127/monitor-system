find_package(gRPC CONFIG REQUIRED)

add_executable(simulated_workers_push
    simulated_workers_push.cpp
)

target_link_libraries(simulated_workers_push PRIVATE
    monitor_proto
    gRPC::grpc++
)

add_executable(manager_stress_test
    manager_stress_test.cpp
)

target_link_libraries(manager_stress_test PRIVATE
    monitor_proto
    gRPC::grpc++
)

if(TARGET manager_core)
    add_executable(manager_scope_tests
        manager_scope_tests.cpp
    )

    target_link_libraries(manager_scope_tests PRIVATE
        manager_core
    )
endif()
