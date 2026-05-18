find_package(gRPC CONFIG REQUIRED)

# eBPF 支持选项
option(ENABLE_EBPF "Enable eBPF-based network monitoring" ON)
option(ENABLE_MYSQL_MONITOR "Enable MySQL instance monitoring in worker" ON)

if(ENABLE_MYSQL_MONITOR)
    find_package(libmysqlclient CONFIG REQUIRED)
endif()

if(ENABLE_EBPF)
    # 找到eBPF依赖库
    message(STATUS "eBPF support enabled")
    find_package(libbpf CONFIG REQUIRED)
    find_package(elfutils CONFIG REQUIRED)
    find_package(ZLIB CONFIG REQUIRED)

    set(EBPF_SRC_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/ebpf)
    set(EBPF_SKEL_H ${CMAKE_CURRENT_BINARY_DIR}/include/monitor/net_stats.skel.h)
    get_target_property(LIBBPF_INCLUDE_DIRS libbpf::libbpf INTERFACE_INCLUDE_DIRECTORIES)
    if(NOT LIBBPF_INCLUDE_DIRS)
        set(LIBBPF_INCLUDE_DIRS ${libbpf_INCLUDE_DIRS})
    endif()
    if(NOT LIBBPF_INCLUDE_DIRS)
        message(FATAL_ERROR "Could not determine libbpf include directories")
    endif()
    string(REPLACE ";" " " LIBBPF_INCLUDE_DIRS "${LIBBPF_INCLUDE_DIRS}")
    # 生成eBPF骨架头文件
    add_custom_command(
        OUTPUT ${EBPF_SKEL_H}   
        COMMAND  make -C ${EBPF_SRC_DIRS} "LIBBPF_INCLUDE=${LIBBPF_INCLUDE_DIRS}"  # 构建eBPF程序
        COMMAND ${CMAKE_COMMAND} -E copy ${EBPF_SRC_DIRS}/.output/net_stats.skel.h ${EBPF_SKEL_H} # 复制生成的骨架头文件到构建目录
        DEPENDS ${EBPF_SRC_DIRS}/net_stats.bpf.c # 依赖eBPF源文件
        COMMENT "Building eBPF program and generating skeleton header"
    )
    add_custom_target(ebpf_skeleton DEPENDS ${EBPF_SKEL_H}) # 定义一个目标来生成eBPF骨架头文件
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

if(ENABLE_MYSQL_MONITOR)
    list(APPEND WORKER_SRC src/monitor/MysqlMonitor.cc)
endif()

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
    add_dependencies(worker ebpf_skeleton) # 确保在构建 worker 之前生成 eBPF 骨架头文件
    target_compile_definitions(worker PRIVATE ENABLE_EBPF=1) # 定义宏以启用 eBPF 相关代码
    list(APPEND WORKER_LIBS libbpf::libbpf elfutils::elfutils ZLIB::ZLIB) # 添加 eBPF 相关库链接
endif()

if(ENABLE_MYSQL_MONITOR)
    target_compile_definitions(worker PRIVATE ENABLE_MYSQL_MONITOR=1)
    list(APPEND WORKER_LIBS libmysqlclient::libmysqlclient)
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
        COMMAND make -C ${CMAKE_CURRENT_SOURCE_DIR}/src/ebpf "LIBBPF_INCLUDE=${LIBBPF_INCLUDE_DIRS}"
        COMMENT "Building eBPF programs"
    )
endif()
