find_package(gRPC CONFIG REQUIRED)
add_library(worker_grpc_lib INTERFACE)
target_link_libraries(worker_grpc_lib INTERFACE gRPC::grpc gRPC::grpc++)

# eBPF 支持选项
option(ENABLE_EBPF "Enable eBPF-based network monitoring" ON)

if(ENABLE_EBPF)
    # 找到eBPF依赖库
    message(STATUS "eBPF support enabled")
    find_package(libbpf CONFIG REQUIRED)
    find_package(elfutils CONFIG REQUIRED)
    find_package(ZLIB CONFIG REQUIRED)

    set(EBPF_SRC_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/ebpf)
    set(EBPF_SKEL_H ${CMAKE_CURRENT_BINARY_DIR}/include/monitor/net_stats.skel.h)
    # 生成eBPF骨架头文件
    add_custom_command(
        OUTPUT ${EBPF_SKEL_H}   
        COMMAND  make -C ${EBPF_SRC_DIRS} 
        COMMAND ${CMAKE_COMMAND} -E copy ${EBPF_SRC_DIRS}/.output/net_stats.skel.h ${EBPF_SKEL_H}
        DEPENDS ${EBPF_SRC_DIRS}/net_stats.bpf.c
        COMMENT "Building eBPF program and generating skeleton header"
    )
    add_custom_target(ebpf_skeleton DEPENDS ${EBPF_SKEL_H})
    set(EBPF_FOUND TRUE)
endif()

set(WORKER_SRC
    src/main.cc
    src/monitor/MetricCollector.cc
    src/monitor/CpuLoadMonitor.cc
    src/monitor/CpuSoftirqMonitor.cc
    src/monitor/CpuStatMonitor.cc
    src/monitor/MemMonitor.cc
    src/monitor/DiskMonitor.cc
    src/monitor/HostInfoMonitor.cc
    src/rpc/MonitorPusher.cc
    src/utils/ReadFile.cc
)

# 根据 eBPF 支持选项添加源文件和库
if(EBPF_FOUND)
    list(APPEND WORKER_SRC src/monitor/NetEbpfMonitor.cc)
else()
    list(APPEND WORKER_SRC src/monitor/NetMonitor.cc)
endif()

add_executable(worker ${WORKER_SRC})

target_include_directories(worker PUBLIC 
    ${PROJECT_SOURCE_DIR}/worker/include/monitor 
    ${PROJECT_SOURCE_DIR}/worker/include/utils
    ${PROJECT_SOURCE_DIR}/worker/include/rpc
    ${CMAKE_CURRENT_BINARY_DIR}/include/monitor
)

# 链接 gRPC 库及 proto
set(WORKER_LIBS monitor_proto gRPC::grpc++)

# eBPF 依赖
if(EBPF_FOUND)
    add_dependencies(worker ebpf_skeleton)
    target_compile_definitions(worker PRIVATE ENABLE_EBPF=1)
    list(APPEND WORKER_LIBS libbpf::libbpf elfutils::elfutils ZLIB::ZLIB)
endif()

target_link_libraries(worker PUBLIC ${WORKER_LIBS})

# 内核模块构建（可选）
add_custom_target(kernel_modules
    COMMAND make -C ${CMAKE_CURRENT_SOURCE_DIR}/src/kmod
    COMMENT "Building kernel modules"
)

# eBPF 程序构建（可选）
if(EBPF_FOUND)
    add_custom_target(ebpf_programs
        COMMAND make -C ${CMAKE_CURRENT_SOURCE_DIR}/src/ebpf
        COMMENT "Building eBPF programs"
    )
endif()