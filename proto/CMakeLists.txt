find_package(Protobuf REQUIRED)
find_package(c-ares REQUIRED)
find_package(gRPC REQUIRED)
# find_package(Threads)
#
# Protobuf/Grpc source files
#
set(PROTO_FILES
    monitor_info.proto
    cpu_load.proto
    cpu_softirq.proto
    cpu_stat.proto
    mem_info.proto
    net_info.proto
    disk_info.proto
    query_api.proto
)

add_library(monitor_proto ${PROTO_FILES})    # 生成 monitor_proto 静态库

# 根据找到的包配置链接库
target_link_libraries(monitor_proto PUBLIC protobuf::libprotobuf)
target_link_libraries(monitor_proto PUBLIC gRPC::grpc gRPC::grpc++)
target_include_directories(monitor_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})     # 生成的代码路径（build/proto/）

#
# Compile protobuf and grpc files in mointor proto target to cpp
#
if(TARGET gRPC::grpc_cpp_plugin)
    get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)
else()
    set(grpc_cpp_plugin_location ${GRPC_CPP_PLUGIN})
endif()
protobuf_generate(TARGET monitor_proto LANGUAGE cpp)   # 生成 Protobuf 序列化代码
# 生成 gRPC 服务代码
protobuf_generate(TARGET monitor_proto LANGUAGE grpc GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}")